#include "roadnet_file.h"
#include "common/mod_args.h"

#include "common/json.h"
#include "common/utility.h"

#include <cctype>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

// 贴合/隧道口参数，和roadnet_jing/checkboard保持一致(两个mod互不include，常量各抄一份)。
#define TERRAIN_FIT_FALLOFF 1.5f
// 隧道资产房顶比路面高多少(地图单位，1=10m)：端墙从隧道端头的地形最高处封到"该端路面高度+这个值"(也就是资产房顶)，房顶以下留着洞口。
#define TUNNEL_ROOF_CLEARANCE 1.f
#define LANE_WIDTH 0.5f
#define MESH_UNIT 0.5f
// 桥墩参数(单位element=10m)：默认间距40m、最小间距20m、墩半径2m，沿用火车桥墩(route_train.cpp)的取值，路更宽所以半径放大。
#define PIER_DEFAULT_SPACING 4.0f
#define PIER_MIN_SPACING 2.0f
#define PIER_RADIUS 0.2f

// 行道树/路灯的"default" tag资产，和roadnet_checkboard.cpp完全一致(两个mod互不include，常量各抄一份)。
#define TREE_MESH_PATH "/Game/3rdParty/Mossy_Tree/StaticMeshes/mossy-tree.mossy-tree"
#define TREE_SCALE 0.6f
#define TREE_SPACING 1.f
#define TREE_SIDEWALK_RATIO 0.25f
#define LAMP_BLUEPRINT_PATH "/Game/3rdParty/Stylized_street_light/Actor/BP_StreetLight.BP_StreetLight_C"
#define LAMP_SPACING 1.5f
#define LAMP_SIDEWALK_RATIO 0.9f
#define LAMP_YAW_OFFSET 0.f

using namespace std;

namespace {
	// tag -> 资产。要加新资产就在这里加一行(同时编辑器里给道路填同名tag)；未知tag会告警并当作没有。
	struct TreeAsset { string mesh; float scale; float spacing; float sidewalkRatio; };
	struct LampAsset { string blueprint; float spacing; float sidewalkRatio; float yawOffset; };
	const TreeAsset* TreeAssetFor(const string& tag) {
		static const unordered_map<string, TreeAsset> table = {
			{ "default", { TREE_MESH_PATH, TREE_SCALE, TREE_SPACING, TREE_SIDEWALK_RATIO } },
		};
		auto it = table.find(tag);
		return it == table.end() ? nullptr : &it->second;
	}
	const LampAsset* LampAssetFor(const string& tag) {
		static const unordered_map<string, LampAsset> table = {
			{ "default", { LAMP_BLUEPRINT_PATH, LAMP_SPACING, LAMP_SIDEWALK_RATIO, LAMP_YAW_OFFSET } },
		};
		auto it = table.find(tag);
		return it == table.end() ? nullptr : &it->second;
	}

	float NumberOr(const JsonValue& value, float fallback) {
		return value.IsNumeric() ? value.AsFloat() : fallback;
	}

	// asset=车道资产名(.roadnet roads[].asset，缺省default)：资产在Content/Asset/Roadnet/<首字母大写的asset>/Mesh/<asset>_<车道类型>。
	string MeshPathFor(const string& asset, const int lanes[6], bool tunnel) {
		string suffix = to_string(lanes[0]) + "_" + to_string(lanes[1]) + "_" + to_string(lanes[2]) + "_"
			+ to_string(lanes[3]) + "_" + to_string(lanes[4]) + "_" + to_string(lanes[5]);
		if (tunnel) suffix += "_T";
		string folder = asset;
		if (!folder.empty()) folder[0] = static_cast<char>(toupper(static_cast<unsigned char>(folder[0])));
		return "/Game/Asset/Roadnet/" + folder + "/Mesh/" + asset + "_" + suffix + "." + asset + "_" + suffix;
	}

	AREA_TYPE AreaFromName(const string& areaName) {
		static const unordered_map<string, AREA_TYPE> table = {
			{ "AREA_NONE", AREA_NONE },
			{ "AREA_RESIDENCE_HIGH", AREA_RESIDENCE_HIGH }, { "AREA_RESIDENCE_MIDDLE", AREA_RESIDENCE_MIDDLE }, { "AREA_RESIDENCE_LOW", AREA_RESIDENCE_LOW },
			{ "AREA_COMMERCE_HIGH", AREA_COMMERCE_HIGH }, { "AREA_COMMERCE_MIDDLE", AREA_COMMERCE_MIDDLE }, { "AREA_COMMERCE_LOW", AREA_COMMERCE_LOW },
			{ "AREA_INDUSTRY_HIGH", AREA_INDUSTRY_HIGH }, { "AREA_INDUSTRY_MIDDLE", AREA_INDUSTRY_MIDDLE }, { "AREA_INDUSTRY_LOW", AREA_INDUSTRY_LOW },
			{ "AREA_OFFICE_HIGH", AREA_OFFICE_HIGH }, { "AREA_OFFICE_MIDDLE", AREA_OFFICE_MIDDLE }, { "AREA_OFFICE_LOW", AREA_OFFICE_LOW },
			{ "AREA_TEST", AREA_TEST }, { "AREA_PLACEHOLDER", AREA_PLACEHOLDER },
		};
		auto it = table.find(areaName);
		return it == table.end() ? AREA_NONE : it->second;
	}

	// 路口缩进(写进Road::SetSetback)，和roadnet_jing.cpp的AssignJunctionSetbacks同一份逻辑，说明见roadnet_jing.cpp和Core/map/roadnet.md。
	void AssignJunctionSetbacks(vector<Road>& roads, const vector<Intersection>& intersections) {
		const float PI = 3.14159265358979f;
		const float EPS = 0.05f;
		struct Arm { Road* road; bool isStart; float angle; float halfWidth; };
		unordered_map<int, vector<Arm>> armsByNode;
		unordered_set<int> intersectionIds;
		for (const Intersection& intersection : intersections) intersectionIds.insert(intersection.GetId());
		for (Road& road : roads) {
			road.SetSetback(true, 0.f);
			road.SetSetback(false, 0.f);
			for (bool isStart : { true, false }) {
				int nodeId = isStart ? road.GetStart().GetId() : road.GetEnd().GetId();
				if (!intersectionIds.count(nodeId)) continue;
				float tx, ty, tz;
				road.GetTangent(isStart ? 0.f : 1.f, tx, ty, tz);
				float angle = isStart ? atan2f(ty, tx) : atan2f(-ty, -tx);
				armsByNode[nodeId].push_back({ &road, isStart, angle, road.GetTotalWidth() * 0.5f });
			}
		}
		for (auto& [nodeId, arms] : armsByNode) {
			sort(arms.begin(), arms.end(), [](const Arm& a, const Arm& b) { return a.angle < b.angle; });
			// 每条路在路口每一侧(逆时针/顺时针方向的相邻路)各算一个缩进，取较大的：和这一侧相邻路的夹角theta<=直角时精确算——
			// 本路和对方朝向对方那一侧的路缘线在路口里的交点，沿本路中线的距离 = (对方半宽 + 本路半宽*cos(theta)) / sin(theta)；
			// theta>直角时直接取对方半宽；theta>=平角(减容差)那一侧没有重叠，不缩进。再按0.45倍路长封顶(同Core的tFrac clamp)。
			// 只连两条路(拐角)的路口平角容差放宽到5度：两条路接近一条直线(夹角>=175度)时不缩进，其余同上。
			const size_t count = arms.size();
			const float flatTolerance = count == 2 ? 5.f * PI / 180.f : EPS;
			for (size_t k = 0; k < count; k++) {
				auto sideSetback = [&](size_t other, bool counterClockwise) {
					if (other == k) return 0.f;
					float gap = counterClockwise ? arms[other].angle - arms[k].angle : arms[k].angle - arms[other].angle;
					while (gap <= 0.f) gap += 2.f * PI;
					if (gap >= PI - flatTolerance) return 0.f;
					if (gap > PI * 0.5f) return arms[other].halfWidth;
					return max(0.f, (arms[other].halfWidth + arms[k].halfWidth * cosf(gap)) / max(sinf(gap), 1e-3f));
				};
				float setback = max(sideSetback((k + 1) % count, true), sideSetback((k + count - 1) % count, false));
				float length = arms[k].road->CalcDistance();
				if (length > 1e-6f) setback = min(setback, length * 0.45f);
				arms[k].road->SetSetback(arms[k].isStart, setback);
			}
		}
	}
}

int FileRoadnet::count = 0;

FileRoadnet::FileRoadnet(const string& args) : id(count++) {
	filterRoadPlants = false; // 行道树/路灯只看每条路自己的tag，见LayoutRows
	path = ModArgs(args).GetString("path");
}

FileRoadnet::~FileRoadnet() {

}

const char* FileRoadnet::GetId() {
	return "roadnet_file";
}

const char* FileRoadnet::GetType() const {
	return "roadnet_file";
}

const char* FileRoadnet::GetName() {
	name = "文件路网" + to_string(id);
	return name.data();
}

void FileRoadnet::DistributeRoadnet(int width, int height,
	const function<const string&(int, int)>& getTerrain,
	const function<bool(int, int)>& isConstruction,
	const function<pair<bool, float>(int, int)>& getWater,
	const function<float(float, float)>& getHeight,
	int nodeStaticCount) {
	Node::SetCount(nodeStaticCount);
	treeTags.clear();
	lampTags.clear();

	if (path.empty()) {
		debugf("Warning: FileRoadnet has no --path, roadnet left empty.\n");
		return;
	}
	ifstream file(path, ios::binary);
	if (!file) {
		debugf("Warning: FileRoadnet cannot open %s, roadnet left empty.\n", path.c_str());
		return;
	}
	string text((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
	JsonValue root;
	JsonReader reader;
	if (!reader.Parse(text, root, false) || !root.IsObject() || !root["output"].IsObject()) {
		debugf("Warning: FileRoadnet %s is not a valid .roadnet file: %s\n", path.c_str(), reader.GetErrorMessages().c_str());
		return;
	}
	const JsonValue& output = root["output"];

	// 节点：type = "intersection"(路口) / "extern"(城外连接点) / "plain"(普通点，如坡段衔接处)。
	// 同一个id的所有端点必须是同一个Node id(Road构造时按值拷贝节点但保留id)，路口还要登记进intersections。
	unordered_map<string, Node> nodes;
	const JsonValue& nodeArray = output["nodes"];
	for (unsigned int i = 0; i < nodeArray.size(); i++) {
		const JsonValue& entry = nodeArray[i];
		string nodeKey = entry["id"].AsString();
		float x = NumberOr(entry["x"], 0.f), y = NumberOr(entry["y"], 0.f), z = NumberOr(entry["z"], 0.f);
		string type = entry["type"].IsString() ? entry["type"].AsString() : "plain";
		if (type == "intersection") {
			intersections.emplace_back(x, y, z);
			nodes.emplace(nodeKey, static_cast<const Node&>(intersections.back()));
		}
		else {
			Node node("roadnet", x, y, z);
			if (type == "extern") externs.emplace_back(node);
			nodes.emplace(nodeKey, node);
		}
	}

	// 道路：kind = ground / hug(贴地形爬升，请求地形贴合) / bridge(抬升桥面或坡) / tunnel(隧道内) / mouth(隧道口坡段，整段挖洞)。
	vector<vector<pair<int, Connection>>> bridgeChains;
	unordered_map<string, size_t> bridgeChainIndex;
	const JsonValue& roadArray = output["roads"];
	// 贴合请求的端点圆帽：端点连着桥/隧道/隧道口(路面不贴地形)的那一端关掉圆帽，否则圆帽会把地形抬到盖住桥头。
	unordered_set<string> elevatedNodes;
	for (unsigned int i = 0; i < roadArray.size(); i++) {
		const JsonValue& entry = roadArray[i];
		string roadKind = entry["kind"].IsString() ? entry["kind"].AsString() : "ground";
		if (roadKind != "bridge" && roadKind != "tunnel" && roadKind != "mouth") continue;
		elevatedNodes.insert(entry["from"].AsString());
		elevatedNodes.insert(entry["to"].AsString());
	}
	for (unsigned int i = 0; i < roadArray.size(); i++) {
		const JsonValue& entry = roadArray[i];
		auto fromIt = nodes.find(entry["from"].AsString());
		auto toIt = nodes.find(entry["to"].AsString());
		if (fromIt == nodes.end() || toIt == nodes.end()) {
			debugf("Warning: FileRoadnet road %u references an unknown node, skipped.\n", i);
			continue;
		}
		int lanes[6] = { 1, 0, 1, 1, 0, 1 };
		const JsonValue& laneArray = entry["lanes"];
		for (unsigned int k = 0; k < 6 && k < laneArray.size(); k++) lanes[k] = max(0, laneArray[k].AsInt());
		string kind = entry["kind"].IsString() ? entry["kind"].AsString() : "ground";
		string roadName = entry["name"].IsString() ? entry["name"].AsString() : "road" + to_string(i);

		string asset = entry["asset"].IsString() && !entry["asset"].AsString().empty() ? entry["asset"].AsString() : "default";
		string meshPath = MeshPathFor(asset, lanes, kind == "tunnel");
		roads.emplace_back(roadName, fromIt->second, toIt->second, meshPath, MESH_UNIT);
		Road& road = roads.back();
		treeTags.push_back(entry["treeTag"].IsString() ? entry["treeTag"].AsString() : "");
		lampTags.push_back(entry["lampTag"].IsString() ? entry["lampTag"].AsString() : "");

		const JsonValue& controlArray = entry["controls"];
		vector<pair<Node, float>> controls;
		for (unsigned int k = 0; k < controlArray.size(); k++) {
			const JsonValue& control = controlArray[k];
			if (!control.IsArray() || control.size() < 3) continue;
			float weight = control.size() >= 4 ? NumberOr(control[3], 1.f) : 1.f;
			controls.emplace_back(Node("roadnet", control[0].AsFloat(), control[1].AsFloat(), control[2].AsFloat()), weight);
		}
		if (!controls.empty()) road.AddControls(controls);

		for (int k = 0; k < lanes[0]; k++) road.AddVehicleLane(0, LANE_WIDTH);
		for (int k = 0; k < lanes[1]; k++) road.AddParkingLane(0, LANE_WIDTH);
		for (int k = 0; k < lanes[2]; k++) road.AddPedestrianLane(0, LANE_WIDTH);
		for (int k = 0; k < lanes[3]; k++) road.AddVehicleLane(1, LANE_WIDTH);
		for (int k = 0; k < lanes[4]; k++) road.AddParkingLane(1, LANE_WIDTH);
		for (int k = 0; k < lanes[5]; k++) road.AddPedestrianLane(1, LANE_WIDTH);

		// 开口落在某一侧时换用"该侧不含人行道"的资产；隧道内路面没有开口。
		if (kind != "tunnel") {
			if (lanes[2] > 0) { int l[6] = { lanes[0], lanes[1], 0, lanes[3], lanes[4], lanes[5] }; road.SetSidewalkFreeMesh(0, MeshPathFor(asset, l, false)); }
			if (lanes[5] > 0) { int l[6] = { lanes[0], lanes[1], lanes[2], lanes[3], lanes[4], 0 }; road.SetSidewalkFreeMesh(1, MeshPathFor(asset, l, false)); }
		}
		if (kind == "mouth") {
			// wallFrom/wallTo由编辑器写：这一端连着隧道(kind=tunnel的路)就加端墙(from=t1端/to=t2端)，和两端高度无关。
			bool wallFrom = entry["wallFrom"].IsBool() && entry["wallFrom"].AsBool();
			bool wallTo = entry["wallTo"].IsBool() && entry["wallTo"].AsBool();
			AddTunnelHatch(&road, 0.f, 1.f, road.GetTotalWidth(),
				wallFrom ? fromIt->second.GetZ() + TUNNEL_ROOF_CLEARANCE : kNoEndWall, wallTo ? toIt->second.GetZ() + TUNNEL_ROOF_CLEARANCE : kNoEndWall);
		}
		// hug=路面贴合地形(路面高度已由编辑器按地形采样好，请求把地形再贴合到路面上做过渡)；fit=地形贴合路面(路面高度取节点z，整段把地形削平/填平到路面)。
		if (kind == "hug" || kind == "fit") {
			AddTerrainFit(&road, 0.f, 1.f, road.GetTotalWidth(), TERRAIN_FIT_FALLOFF,
				elevatedNodes.find(entry["from"].AsString()) == elevatedNodes.end(),
				elevatedNodes.find(entry["to"].AsString()) == elevatedNodes.end());
		}

		// 桥：kind=bridge的段按"chain"归成一条链(编辑器把经过只连着两条桥的节点的连续桥归一组)，
		// "order"是行进顺序，"reverse"表示这一段是反着画的(链里要把首尾和控制点倒过来才能首尾相接)，交给Core选位放桥墩。
		// 链里是几何副本(端点沿用路的Node，保留id，Core靠id把本链从障碍里排除)。没有chain字段(旧文件)时每条路自成一链。
		if (kind == "bridge") {
			string chainKey = entry["chain"].IsString() ? entry["chain"].AsString()
				: (entry["chain"].IsNumeric() ? to_string(entry["chain"].AsInt()) : "road" + to_string(i));
			int order = entry["order"].IsNumeric() ? entry["order"].AsInt() : static_cast<int>(i);
			bool reverse = entry["reverse"].IsBool() && entry["reverse"].AsBool();
			Connection piece(reverse ? toIt->second : fromIt->second, reverse ? fromIt->second : toIt->second);
			if (!controls.empty()) {
				vector<pair<Node, float>> ordered = controls;
				if (reverse) std::reverse(ordered.begin(), ordered.end());
				piece.AddControls(ordered);
			}
			auto found = bridgeChainIndex.find(chainKey);
			if (found == bridgeChainIndex.end()) {
				found = bridgeChainIndex.emplace(chainKey, bridgeChains.size()).first;
				bridgeChains.emplace_back();
			}
			bridgeChains[found->second].emplace_back(order, std::move(piece));
		}
	}
	if (!bridgeChains.empty()) {
		vector<vector<Connection>> chains;
		for (auto& pieces : bridgeChains) {
			std::stable_sort(pieces.begin(), pieces.end(), [](const pair<int, Connection>& a, const pair<int, Connection>& b) { return a.first < b.first; });
			chains.emplace_back();
			for (auto& piece : pieces) chains.back().push_back(std::move(piece.second));
		}
		AddPier("", std::move(chains), PIER_DEFAULT_SPACING, PIER_MIN_SPACING, PIER_RADIUS);
	}

	// 地块：corners是4个角(按环绕顺序)，margin按FACE_DIRECTION(西/东/北/南)，boundary{方向:roads下标}。
	// 必须在roads不再增长之后再取边界Road指针。
	const JsonValue& lotArray = output["lots"];
	vector<int> lotOfOutput(lotArray.size(), -1); // output.lots下标 -> lots下标(无效的lot被跳过，下标会错开)，presets用
	for (unsigned int i = 0; i < lotArray.size(); i++) {
		const JsonValue& entry = lotArray[i];
		const JsonValue& corners = entry["corners"];
		if (!corners.IsArray() || corners.size() != 4) continue;
		vector<Node> cornerNodes;
		for (unsigned int k = 0; k < 4; k++)
			cornerNodes.emplace_back("roadnet", corners[k][0].AsFloat(), corners[k][1].AsFloat(), corners[k].size() > 2 ? NumberOr(corners[k][2], 0.f) : 0.f);
		vector<float> margin(4, 0.f);
		for (unsigned int k = 0; k < 4 && k < entry["margin"].size(); k++) margin[k] = NumberOr(entry["margin"][k], 0.f);
		unordered_map<int, Road*> boundary;
		const JsonValue& boundaryObject = entry["boundary"];
		for (int direction = 0; direction < 4; direction++) {
			const JsonValue& slot = boundaryObject[to_string(direction)];
			if (!slot.IsNumeric()) continue;
			int roadIndex = slot.AsInt();
			if (roadIndex >= 0 && roadIndex < static_cast<int>(roads.size())) boundary[direction] = &roads[roadIndex];
		}
		try {
			lots.emplace_back(cornerNodes[0], cornerNodes[1], cornerNodes[2], cornerNodes[3], margin, boundary);
			lots.back().SetArea(AreaFromName(entry["area"].AsString()));
			lotOfOutput[i] = static_cast<int>(lots.size()) - 1;
		}
		catch (...) {
			debugf("Warning: FileRoadnet lot %u is not a valid rectangle, skipped.\n", i);
		}
	}

	// 绿化区：points是闭合多边形顶点(不重复首点)，links{edge,road}是"哪条边连哪条road(roads下标)"。
	// 行为(挑mod、摆圆/树、接导航)全在Core的Map::InitGreens，这里只搬数据。
	const JsonValue& greenArray = output["greens"];
	for (unsigned int i = 0; i < greenArray.size(); i++) {
		const JsonValue& entry = greenArray[i];
		const JsonValue& points = entry["points"];
		if (!points.IsArray() || points.size() < 3) continue;
		GreenSite site;
		if (entry["id"].IsString()) site.modId = entry["id"].AsString();   // 空/缺省=Core按权重随机
		for (unsigned int k = 0; k < points.size(); k++) {
			site.xy.push_back(points[k][0].AsFloat());
			site.xy.push_back(points[k][1].AsFloat());
		}
		const JsonValue& linkArray = entry["links"];
		for (unsigned int k = 0; k < linkArray.size(); k++) {
			const JsonValue& link = linkArray[k];
			if (!link["edge"].IsNumeric() || !link["road"].IsNumeric()) continue;
			GreenSite::Link item;
			item.edge = link["edge"].AsInt();
			item.road = link["road"].AsInt();
			if (item.edge < 0 || item.edge >= static_cast<int>(points.size())) continue;
			if (item.road < 0 || item.road >= static_cast<int>(roads.size())) continue;
			site.links.push_back(item);
		}
		greens.push_back(std::move(site));
	}

	// 园区/建筑预设：lot是output.lots下标，rect=[x0,x1,y0,y1]在该lot本地坐标系(见RoadnetMod::PlacementPreset)，id以zone_开头是园区。
	// 全部在Core(Map::PlacePresets)里落地，这里只搬数据。
	const JsonValue& presetArray = output["presets"];
	for (unsigned int i = 0; i < presetArray.size(); i++) {
		const JsonValue& entry = presetArray[i];
		const JsonValue& rect = entry["rect"];
		if (!entry["lot"].IsNumeric() || !entry["direction"].IsNumeric() || !rect.IsArray() || rect.size() != 4) continue;
		int outputLot = entry["lot"].AsInt();
		if (outputLot < 0 || outputLot >= static_cast<int>(lotOfOutput.size()) || lotOfOutput[outputLot] < 0) {
			debugf("Warning: FileRoadnet preset %u refers to a missing lot, skipped.\n", i);
			continue;
		}
		int direction = entry["direction"].AsInt();
		if (direction < 0 || direction > 3) continue;
		PlacementPreset preset;
		preset.lotIndex = lotOfOutput[outputLot];
		preset.modId = entry["id"].IsString() ? entry["id"].AsString() : "";
		preset.direction = direction;
		preset.x0 = NumberOr(rect[0], 0.f); preset.x1 = NumberOr(rect[1], 0.f);
		preset.y0 = NumberOr(rect[2], 0.f); preset.y1 = NumberOr(rect[3], 0.f);
		preset.outputIndex = static_cast<int>(i);
		presets.push_back(std::move(preset));
	}

	// 独立线路(output.transit，MapEditor"线路"工具)：节点/连线/线路环，只搬数据，route mod在--skip时才用，见transit_data.h。
	const JsonValue& transitObject = output["transit"];
	if (transitObject.IsObject()) {
		const JsonValue& nodeArray = transitObject["nodes"];
		for (unsigned int i = 0; i < nodeArray.size(); i++) {
			TransitData::Node node;
			node.x = NumberOr(nodeArray[i]["x"], 0.f); node.y = NumberOr(nodeArray[i]["y"], 0.f); node.z = NumberOr(nodeArray[i]["z"], 0.f);
				node.isExtern = nodeArray[i]["extern"].IsBool() && nodeArray[i]["extern"].AsBool();
			transit.nodes.push_back(node);
		}
		const int nodeCount = static_cast<int>(transit.nodes.size());
		const JsonValue& connArray = transitObject["conns"];
		for (unsigned int i = 0; i < connArray.size(); i++) {
			const JsonValue& entry = connArray[i];
			if (!entry["from"].IsNumeric() || !entry["to"].IsNumeric() || !entry["route"].IsString()) continue;
			TransitData::Conn conn;
			conn.from = entry["from"].AsInt(); conn.to = entry["to"].AsInt();
			if (conn.from < 0 || conn.from >= nodeCount || conn.to < 0 || conn.to >= nodeCount || conn.from == conn.to) continue;
			const JsonValue& ctrl = entry["ctrl"];
			if (ctrl.IsArray() && ctrl.size() == 2) { conn.hasCtrl = true; conn.cx = NumberOr(ctrl[0], 0.f); conn.cy = NumberOr(ctrl[1], 0.f); }
			if (entry["kind"].IsString()) conn.kind = entry["kind"].AsString();
			conn.route = entry["route"].AsString();
			transit.conns.push_back(std::move(conn));
		}
		const JsonValue& loopArray = transitObject["loops"];
		for (unsigned int i = 0; i < loopArray.size(); i++) {
			const JsonValue& entry = loopArray[i];
			const JsonValue& itemArray = entry["items"];
			if (!entry["route"].IsString() || !itemArray.IsArray()) continue;
			TransitData::Loop loop;
			loop.route = entry["route"].AsString();
			for (unsigned int k = 0; k < itemArray.size(); k++) {
				const JsonValue& item = itemArray[k];
				TransitData::Item parsed;
				if (item["preset"].IsNumeric()) {
					parsed.preset = item["preset"].AsInt();
					parsed.stop = item["stop"].IsBool() && item["stop"].AsBool();
					if (parsed.preset < 0) continue;
				}
				else if (item["node"].IsNumeric()) {
					parsed.node = item["node"].AsInt();
					if (parsed.node < 0 || parsed.node >= nodeCount) continue;
				}
				else continue;
				loop.items.push_back(parsed);
			}
			if (loop.items.size() >= 2) transit.loops.push_back(std::move(loop));
		}
		// 路网路线(公交)：站引用output.roads下标，越界/缺字段的站丢弃，不足2站的线丢弃。
		const JsonValue& roadLineArray = transitObject["roadLines"];
		for (unsigned int i = 0; i < roadLineArray.size(); i++) {
			const JsonValue& entry = roadLineArray[i];
			const JsonValue& stopArray = entry["stops"];
			if (!entry["route"].IsString() || !stopArray.IsArray()) continue;
			TransitData::RoadLine line;
			line.route = entry["route"].AsString();
			for (unsigned int k = 0; k < stopArray.size(); k++) {
				const JsonValue& stop = stopArray[k];
				if (!stop["road"].IsNumeric() || !stop["station"].IsString()) continue;
				TransitData::RoadStop parsed;
				parsed.road = stop["road"].AsInt();
				if (parsed.road < 0 || parsed.road >= static_cast<int>(roads.size())) { debugf("Warning: FileRoadnet bus line %u stop %u references road %d out of range, skipped.\n", i, k, parsed.road); continue; }
				parsed.x = NumberOr(stop["x"], 0.f); parsed.y = NumberOr(stop["y"], 0.f);
				parsed.rightSide = !(stop["side"].IsNumeric() && stop["side"].AsInt() == 1);
				parsed.station = stop["station"].AsString();
				line.stops.push_back(std::move(parsed));
			}
			if (line.stops.size() >= 2) transit.roadLines.push_back(std::move(line));
		}
	}

	AssignJunctionSetbacks(roads, intersections);
	debugf("Log: FileRoadnet loaded %s: %d roads, %d intersections, %d lots, %d greens, transit %d nodes/%d conns/%d loops/%d bus lines.\n", path.c_str(),
		static_cast<int>(roads.size()), static_cast<int>(intersections.size()), static_cast<int>(lots.size()), static_cast<int>(greens.size()),
		static_cast<int>(transit.nodes.size()), static_cast<int>(transit.conns.size()), static_cast<int>(transit.loops.size()), static_cast<int>(transit.roadLines.size()));
}

void FileRoadnet::LayoutTrees(const vector<Road*>& hostRoads) {
	LayoutRows(hostRoads, treeTags,
		[](const string& tag, float& spacing, float& sidewalkRatio) {
			const TreeAsset* asset = TreeAssetFor(tag);
			if (!asset) return false;
			spacing = asset->spacing; sidewalkRatio = asset->sidewalkRatio;
			return true;
		},
		[this](const string& tag, int, float x0, float y0, float x1, float y1, int num, float) {
			const TreeAsset* asset = TreeAssetFor(tag);
			AddTreeLine({ 0.f, x0, 0.f, y0 }, { 0.f, x1, 0.f, y1 }, num, asset->scale, asset->mesh);
		});
}

void FileRoadnet::LayoutLamps(const vector<Road*>& hostRoads) {
	LayoutRows(hostRoads, lampTags,
		[](const string& tag, float& spacing, float& sidewalkRatio) {
			const LampAsset* asset = LampAssetFor(tag);
			if (!asset) return false;
			spacing = asset->spacing; sidewalkRatio = asset->sidewalkRatio;
			return true;
		},
		[this](const string& tag, int side, float x0, float y0, float x1, float y1, int num, float angleDegrees) {
			const LampAsset* asset = LampAssetFor(tag);
			float rotation = angleDegrees + (side == 0 ? -90.f : 90.f) + asset->yawOffset;
			AddLampLine({ 0.f, x0, 0.f, y0 }, { 0.f, x1, 0.f, y1 }, num, rotation, asset->blueprint);
		});
}

void FileRoadnet::LayoutRows(const vector<Road*>& hostRoads, const vector<string>& tags, const RowStyleFunc& style, const RowEmitter& emit) {
	unordered_set<int> intersectionIds;
	for (const Intersection& intersection : intersections) intersectionIds.insert(intersection.GetId());
	// 路口收缩距离读Road上mod写好的缩进(AssignJunctionSetbacks)，只有连着Intersection的那端才有缩进。
	auto endGap = [&](const Road* road, bool isStart) {
		int endId = isStart ? road->GetStart().GetId() : road->GetEnd().GetId();
		return intersectionIds.count(endId) ? road->GetSetback(isStart) : 0.f;
	};

	unordered_set<string> warned;
	for (size_t index = 0; index < hostRoads.size() && index < tags.size() && index < roads.size(); index++) {
		const Road* road = hostRoads[index];
		const string& tag = tags[index];
		if (!road || tag.empty() || road->IsPathRoad()) continue;
		// hostRoads前面和mod->roads一一对应；起点id对不上说明顺序变了，宁可不种也不种错。
		if (road->GetStart().GetId() != roads[index].GetStart().GetId()) continue;
		float spacing = 0.f, sidewalkRatio = 0.f;
		if (!style(tag, spacing, sidewalkRatio)) {
			if (warned.insert(tag).second) debugf("Warning: FileRoadnet unknown tree/lamp tag \"%s\", skipped.\n", tag.c_str());
			continue;
		}
		// 不按路的类型/高度筛：tag是编辑器里这条路自己指定的(隧道/桥/隧道口也可以指定)，指定放什么就放什么。
		float len = road->CalcDistance();
		if (len < 1e-4f) continue;
		float startInset = endGap(road, true) + spacing * 0.5f;
		float endInset = endGap(road, false) + spacing * 0.5f;
		float usable = len - startInset - endInset;
		if (usable <= 0.f) continue;
		bool curved = !road->GetControls().empty();
		float shift = (road->GetSideWidth(0) - road->GetSideWidth(1)) * 0.5f;

		// 沿路距离s(从start量起)上哪些位置要去掉(开口)。
		int num = static_cast<int>(usable / spacing) + 1;
		float step = num > 1 ? usable / (num - 1) : 0.f;
		auto posS = [&](int i) { return num > 1 ? startInset + i * step : startInset + usable * 0.5f; };
		vector<bool> keep(num, true);
		for (const RoadOpening& opening : road->GetOpenings()) {
			float center = opening.t * len;
			float half = opening.width * 0.5f + spacing * 0.5f;
			for (int i = 0; i < num; i++) {
				if (fabs(posS(i) - center) < half) keep[i] = false;
			}
		}

		for (int side = 0; side < 2; side++) {
			const vector<float>& pedLanes = road->GetPedestrianLanes(side);
			if (pedLanes.empty()) continue;
			float inner = 0.f;
			for (float w : road->GetVehicleLanes(side)) inner += w;
			for (float w : road->GetParkingLanes(side)) inner += w;
			float signedOffset = (inner + pedLanes[0] * sidewalkRatio) * (side == 0 ? 1.f : -1.f) - shift;

			// 路上距离s处、该侧人行道上的点和路方向角(度)；曲线路按弧长参数取点和切线。
			auto placeAt = [&](float s, float& x, float& y, float& angleDegrees) {
				float f = std::min(1.f, std::max(0.f, s / len));
				Node p = road->GetPoint(f);
				float tx, ty, tz;
				road->GetTangent(f, tx, ty, tz);
				float tl = sqrtf(tx * tx + ty * ty);
				if (tl < 1e-6f) { tx = 1.f; ty = 0.f; tl = 1.f; }
				float ux = tx / tl, uy = ty / tl;
				x = p.GetX() - uy * signedOffset;
				y = p.GetY() + ux * signedOffset;
				angleDegrees = atan2f(uy, ux) * 57.2957795f;
			};

			for (int i = 0; i < num;) {
				if (!keep[i]) { i++; continue; }
				int j = i;
				while (j + 1 < num && keep[j + 1]) j++;
				if (!curved) {
					// 直路：一整段连续的树位作为一条线(和checkboard一样)。
					float x0, y0, x1, y1, angle;
					placeAt(posS(i), x0, y0, angle);
					placeAt(posS(j), x1, y1, angle);
					emit(tag, side, x0, y0, x1, y1, j - i + 1, angle);
				}
				else {
					// 曲线路：逐点(每点一个num=1的"排")，朝向取该点切线。
					for (int k = i; k <= j; k++) {
						float x, y, angle;
						placeAt(posS(k), x, y, angle);
						emit(tag, side, x, y, x, y, 1, angle);
					}
				}
				i = j + 1;
			}
		}
	}
}
