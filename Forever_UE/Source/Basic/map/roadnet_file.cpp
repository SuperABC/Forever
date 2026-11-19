#include "roadnet_file.h"
#include "common/mod_args.h"

#include "common/json.h"
#include "common/utility.h"

#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

// 贴合/隧道口参数，和roadnet_jing/checkboard保持一致(两个mod互不include，常量各抄一份)。
#define TERRAIN_FIT_FALLOFF 1.5f
#define LANE_WIDTH 0.5f
#define MESH_UNIT 0.5f
// 桥墩参数(单位element=10m)：默认间距40m、最小间距20m、墩半径2m，沿用火车桥墩(route_train.cpp)的取值，路更宽所以半径放大。
#define PIER_DEFAULT_SPACING 4.0f
#define PIER_MIN_SPACING 2.0f
#define PIER_RADIUS 0.2f

using namespace std;

namespace {
	float NumberOr(const JsonValue& value, float fallback) {
		return value.IsNumeric() ? value.AsFloat() : fallback;
	}

	string MeshPathFor(const int lanes[6], bool tunnel) {
		string suffix = to_string(lanes[0]) + "_" + to_string(lanes[1]) + "_" + to_string(lanes[2]) + "_"
			+ to_string(lanes[3]) + "_" + to_string(lanes[4]) + "_" + to_string(lanes[5]);
		if (tunnel) suffix += "_T";
		return "/Game/Asset/Roadnet/Default/Mesh/default_" + suffix + ".default_" + suffix;
	}

	AREA_TYPE AreaFromName(const string& areaName) {
		static const unordered_map<string, AREA_TYPE> table = {
			{ "AREA_NONE", AREA_NONE },
			{ "AREA_RESIDENCE_HIGH", AREA_RESIDENCE_HIGH }, { "AREA_RESIDENCE_MIDDLE", AREA_RESIDENCE_MIDDLE }, { "AREA_RESIDENCE_LOW", AREA_RESIDENCE_LOW },
			{ "AREA_COMMERCE_HIGH", AREA_COMMERCE_HIGH }, { "AREA_COMMERCE_MIDDLE", AREA_COMMERCE_MIDDLE }, { "AREA_COMMERCE_LOW", AREA_COMMERCE_LOW },
			{ "AREA_INDUSTRY_HIGH", AREA_INDUSTRY_HIGH }, { "AREA_INDUSTRY_MIDDLE", AREA_INDUSTRY_MIDDLE }, { "AREA_INDUSTRY_LOW", AREA_INDUSTRY_LOW },
			{ "AREA_OFFICE_HIGH", AREA_OFFICE_HIGH }, { "AREA_OFFICE_MIDDLE", AREA_OFFICE_MIDDLE }, { "AREA_OFFICE_LOW", AREA_OFFICE_LOW },
			{ "AREA_GREEN", AREA_GREEN }, { "AREA_TEST", AREA_TEST },
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
			size_t count = arms.size();
			for (size_t k = 0; k < count; k++) {
				auto gapTo = [&](size_t other, bool counterClockwise) {
					if (other == k) return 2.f * PI;
					float gap = counterClockwise ? arms[other].angle - arms[k].angle : arms[k].angle - arms[other].angle;
					while (gap <= 0.f) gap += 2.f * PI;
					return gap;
				};
				float nextGap = gapTo((k + 1) % count, true), prevGap = gapTo((k + count - 1) % count, false);
				float nextWidth = arms[(k + 1) % count].halfWidth, prevWidth = arms[(k + count - 1) % count].halfWidth;
				float minGap = min(nextGap, prevGap);
				float setback = 0.f;
				if (minGap >= PI - EPS) {
					setback = 0.f;
				}
				else if (minGap < PI * 0.5f - EPS) {
					setback = arms[k].halfWidth;
					if (nextGap < PI * 0.5f - EPS) setback = max(setback, nextWidth);
					if (prevGap < PI * 0.5f - EPS) setback = max(setback, prevWidth);
					setback *= 2.f;
				}
				else {
					if (nextGap < PI - EPS) setback = max(setback, nextWidth);
					if (prevGap < PI - EPS) setback = max(setback, prevWidth);
				}
				float length = arms[k].road->CalcDistance();
				if (length > 1e-6f) setback = min(setback, length * 0.45f);
				arms[k].road->SetSetback(arms[k].isStart, setback);
			}
		}
	}
}

int FileRoadnet::count = 0;

FileRoadnet::FileRoadnet(const string& args) : id(count++) {
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
	const function<pair<bool, float>(int, int)>& getWater,
	const function<float(float, float)>& getHeight,
	int nodeStaticCount) {
	Node::SetCount(nodeStaticCount);

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

		string meshPath = MeshPathFor(lanes, kind == "tunnel");
		roads.emplace_back(roadName, fromIt->second, toIt->second, meshPath, MESH_UNIT);
		Road& road = roads.back();

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
			if (lanes[2] > 0) { int l[6] = { lanes[0], lanes[1], 0, lanes[3], lanes[4], lanes[5] }; road.SetSidewalkFreeMesh(0, MeshPathFor(l, false)); }
			if (lanes[5] > 0) { int l[6] = { lanes[0], lanes[1], lanes[2], lanes[3], lanes[4], 0 }; road.SetSidewalkFreeMesh(1, MeshPathFor(l, false)); }
		}
		if (kind == "mouth") AddTunnelHatch(&road, 0.f, 1.f, road.GetTotalWidth());
		if (kind == "hug") AddTerrainFit(&road, 0.f, 1.f, road.GetTotalWidth(), TERRAIN_FIT_FALLOFF);

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
		}
		catch (...) {
			debugf("Warning: FileRoadnet lot %u is not a valid rectangle, skipped.\n", i);
		}
	}

	AssignJunctionSetbacks(roads, intersections);
	debugf("Log: FileRoadnet loaded %s: %d roads, %d intersections, %d lots.\n", path.c_str(),
		static_cast<int>(roads.size()), static_cast<int>(intersections.size()), static_cast<int>(lots.size()));
}
