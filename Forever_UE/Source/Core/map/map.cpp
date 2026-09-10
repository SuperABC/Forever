#include "map.h"

#include "common/config.h"
#include "common/utility.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

using namespace std;

namespace {
	unordered_map<string, string> ToArgsMap(const vector<pair<string, string>>& entries) {
		unordered_map<string, string> map;
		for (const auto& [id, args] : entries) {
			map[id] = args;
		}
		return map;
	}

	// 从道路中轴线到lanes[index]车道中心的偏移距离(lanes按从内到外排列，index0=最内侧)，
	// 和roadnet.cpp里的同名私有helper逻辑一致(两处都是很小的独立算法，没有共享的必要)。
	float LaneCenterOffset(const vector<float>& lanes, int index) {
		float offset = 0.f;
		for (int i = 0; i < index && i < static_cast<int>(lanes.size()); i++) {
			offset += lanes[i];
		}
		if (index < static_cast<int>(lanes.size())) {
			offset += lanes[index] * 0.5f;
		}
		return offset;
	}

	void RemoveGraphEdgeOneWay(unordered_map<int, vector<pair<int, Connection*>>>& graph, int fromId, int toId) {
		auto it = graph.find(fromId);
		if (it == graph.end()) return;
		auto& edges = it->second;
		edges.erase(remove_if(edges.begin(), edges.end(),
			[toId](const pair<int, Connection*>& e) { return e.first == toId; }), edges.end());
	}
}

Map::Map(int width, int height) :
	width(width),
	height(height),
	elements(static_cast<size_t>(width)* height),
	terrainFactory(),
	terrainTextures() {
	terrainTextures = {
		{ "plain", { 0, "/Game/Asset/Textures/Terrain/PlainDiffuse.PlainDiffuse" } },
		{ "construction", { 0, "/Game/Asset/Textures/Terrain/PlainDiffuse.PlainDiffuse" } }
	};
}

Map::~Map() {
	unordered_set<Connection*> uniqueEdges;
	for (auto& [id, edges] : vehicleNavGraph) {
		for (auto& [toId, edge] : edges) uniqueEdges.insert(edge);
	}
	for (auto& [id, edges] : pedestrianNavGraph) {
		for (auto& [toId, edge] : edges) uniqueEdges.insert(edge);
	}
	for (Connection* edge : uniqueEdges) delete edge;

	for (Node* n : navAnchorNodes) delete n;
	for (RoadJunction* j : junctions) delete j;
	delete roadnet;
}

void Map::InitTerrains() {
	vector<string> mods = Config::GetMods();
	terrainFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("terrain_mods")));

	// modLoader是Map的成员(不是局部变量)——它持有的dll句柄必须活到Map析构为止,
	// terrainFactory.CreateTerrain以后随时可能被调用,详见map.h的注释。
	modLoader.RegisterConcept<TerrainFactory>(mods, "RegisterModTerrains", "FinishModTerrains", &terrainFactory);
}

void Map::InitContents() {
	pair<bool, float> water{ false, 0.f };
	auto getTerrain = [this](int x, int y) -> string {
		return this->GetTerrain(x, y);
		};
	auto setTerrain = [this, &water](int x, int y, string terrain) -> bool {
		return this->SetTerrain(x, y, terrain, water);
		};
	auto getHeight = [this](int x, int y) -> float {
		return this->GetHeight(x, y);
		};
	auto setHeight = [this](int x, int y, float height) -> bool {
		return this->SetHeight(x, y, height);
		};

	auto terrainIds = terrainFactory.GetRegisteredIds();
	vector<Terrain*> terrains;
	for (auto& id : terrainIds) {
		terrains.push_back(new Terrain(&terrainFactory, id));
		terrains.back()->SetupTexture();
	}
	sort(terrains.begin(), terrains.end(), [](const Terrain* a, const Terrain* b) {
		return a->GetPriority() > b->GetPriority();
		});

	int preset = static_cast<int>(terrainTextures.size());
	for (size_t i = 0; i < terrains.size(); i++) {
		terrainTextures[terrains[i]->GetType()] = { static_cast<int>(i) + preset, terrains[i]->GetTexture() };
		water = terrains[i]->GetWater();
		terrains[i]->DistributeTerrain(width, height, getTerrain, setTerrain, getHeight, setHeight);
	}
	for (auto* terrain : terrains) {
		delete terrain;
	}
	terrains.clear();

	debugf("Log: Filter plain elements to construction.\n");
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			bool allPlain = true;
			for (int dy = -1; dy <= 1 && allPlain; dy++) {
				for (int dx = -1; dx <= 1 && allPlain; dx++) {
					int nx = x + dx, ny = y + dy;
					if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
						allPlain = false;
					}
					else {
						string t = getTerrain(nx, ny);
						if (t != "plain" && t != "construction") {
							allPlain = false;
						}
					}
				}
			}
			if (allPlain) {
				setTerrain(x, y, "construction");
			}
		}
	}
}

void Map::InitRoadnet() {
	vector<string> mods = Config::GetMods();
	roadnetFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("roadnet_mods")));
	modLoader.RegisterConcept<RoadnetFactory>(mods, "RegisterModRoadnets", "FinishModRoadnets", &roadnetFactory);

	for (auto& [id, args] : Config::GetConceptMods("roadnet_mods")) {
		roadnetFactory.SetConfig(id, true);
	}
	string activeId = roadnetFactory.GetRoadnet();
	if (activeId.empty()) {
		// config.json没有显式配置roadnet_mods时，退化选第一个被发现注册的mod，
		// 避免PIE里完全没有路网可看(和ForeverModSubsystem"缺配置就用默认"的容错风格一致)。
		auto ids = roadnetFactory.GetRegisteredIds();
		if (!ids.empty()) {
			roadnetFactory.SetConfig(ids[0], true);
			activeId = ids[0];
		}
	}
	if (activeId.empty()) {
		debugf("Warning: no roadnet mod available, skip InitRoadnet.\n");
		return;
	}

	roadnet = new Roadnet(&roadnetFactory, activeId);

	auto getTerrain = [this](int x, int y) -> string { return this->GetTerrain(x, y); };
	auto getWater = [this](int x, int y) -> pair<bool, float> { return this->GetWater(x, y); };
	roadnet->DistributeRoadnet(width, height, getTerrain, getWater, Node::GetCount());
	roadnet->AllocateAddress();

	// 把mod通过RoadnetMod::AddHatch产出的地形挖洞标记(隧道口用)转发进Map::AddHatch，
	// 复用Terrain已有的挖洞渲染机制——AddHatch自己会按Quad的旋转AABB分发到重叠的Element。
	for (const auto& [quad, rotation] : roadnet->GetHatches()) {
		AddHatch(quad, rotation);
	}

	// 按Intersection收集与之相连的Road，逐个建RoadJunction。
	unordered_map<int, vector<Road*>> roadsByIntersection;
	for (Road* road : roadnet->GetRoads()) {
		Node start = road->GetStart();
		Node end = road->GetEnd();
		roadsByIntersection[start.GetId()].push_back(road);
		if (end.GetId() != start.GetId()) {
			roadsByIntersection[end.GetId()].push_back(road);
		}
	}

	unordered_map<Road*, const RoadJunctionApproach*> startApproach, endApproach;
	for (Intersection* intersection : roadnet->GetIntersections()) {
		auto it = roadsByIntersection.find(intersection->GetId());
		if (it == roadsByIntersection.end()) continue;

		RoadJunction* junction = RoadJunction::Build(intersection, it->second, navAnchorNodes);
		junctions.push_back(junction);

		for (const RoadJunctionApproach& approach : junction->GetApproaches()) {
			if (approach.isStart) startApproach[approach.road] = &approach;
			else endApproach[approach.road] = &approach;
		}
	}

	// extern端点没有RoadJunction(externs不是Intersection，只是地图边缘的路网残端)，
	// 直接复用extern自己的Node当该端锚点——地图边缘不需要精确车道横向偏移几何。
	unordered_map<int, Node*> externById;
	for (Node* e : roadnet->GetExterns()) {
		externById[e->GetId()] = e;
	}

	auto resolveAnchor = [&](Road* road, bool atStart, bool isVehicle, int side) -> Node* {
		auto& approachMap = atStart ? startApproach : endApproach;
		auto it = approachMap.find(road);
		if (it != approachMap.end()) {
			const RoadJunctionApproach* ap = it->second;
			if (isVehicle) {
				if (side == 0) return atStart ? ap->vehicleOutbound : ap->vehicleInbound;
				else return atStart ? ap->vehicleInbound : ap->vehicleOutbound;
			}
			return ap->pedestrianSide[side];
		}
		Node endpoint = atStart ? road->GetStart() : road->GetEnd();
		auto externIt = externById.find(endpoint.GetId());
		return (externIt != externById.end()) ? externIt->second : nullptr;
		};

	// 每条Road的"最内侧车道贯通线"：车行边单向插入(按该side实际通行方向)，行人边双向插入。
	for (Road* road : roadnet->GetRoads()) {
		for (int cat = 0; cat < 2; cat++) { // 0=vehicle, 1=pedestrian
			bool isVehicle = (cat == 0);
			for (int side = 0; side < 2; side++) {
				const vector<float>& lanes = isVehicle ? road->GetVehicleLanes(side) : road->GetPedestrianLanes(side);
				if (lanes.empty()) continue;

				// side0沿Road Start->End方向通行(from=Start)，side1沿End->Start(from=End)。
				bool fromIsStart = (side == 0);
				Node* fromAnchor = resolveAnchor(road, fromIsStart, isVehicle, side);
				Node* toAnchor = resolveAnchor(road, !fromIsStart, isVehicle, side);
				if (!fromAnchor || !toAnchor) continue;

				Connection* edge = new Connection(*fromAnchor, *toAnchor);
				auto& graph = isVehicle ? vehicleNavGraph : pedestrianNavGraph;
				graph[fromAnchor->GetId()].emplace_back(toAnchor->GetId(), edge);
				if (!isVehicle) {
					graph[toAnchor->GetId()].emplace_back(fromAnchor->GetId(), edge);
				}

				int idx = cat * 2 + side;
				throughLines[road][idx] = { edge, fromAnchor, toAnchor };
			}
		}
	}

	// 每个RoadJunction内部连接：车行全联通(单向，inbound->outbound)；行人人行横道+转角(双向)。
	for (RoadJunction* junction : junctions) {
		vector<Connection*> vehicleConns, pedestrianConns;
		junction->BuildConnectors(vehicleConns, pedestrianConns);

		for (Connection* conn : vehicleConns) {
			Node start = conn->GetStart();
			Node end = conn->GetEnd();
			vehicleNavGraph[start.GetId()].emplace_back(end.GetId(), conn);
		}
		for (Connection* conn : pedestrianConns) {
			Node start = conn->GetStart();
			Node end = conn->GetEnd();
			pedestrianNavGraph[start.GetId()].emplace_back(end.GetId(), conn);
			pedestrianNavGraph[end.GetId()].emplace_back(start.GetId(), conn);
		}
	}
}

Node* Map::AddRoadAccessNode(const string& roadName, float t, bool isVehicle, bool useForwardSide, float openingWidth) {
	if (!roadnet) return nullptr;

	Road* road = nullptr;
	for (Road* r : roadnet->GetRoads()) {
		if (r->GetName() == roadName) { road = r; break; }
	}
	if (!road) return nullptr;

	int side = useForwardSide ? 0 : 1;
	const vector<float>& lanes = isVehicle ? road->GetVehicleLanes(side) : road->GetPedestrianLanes(side);
	if (lanes.empty()) return nullptr;

	int idx = (isVehicle ? 0 : 2) + side;
	ThroughLine& line = throughLines[road][idx];
	if (!line.fromAnchor || !line.toAnchor) return nullptr;

	Node basePoint = road->GetPoint(t);
	float tdx, tdy, tdz;
	road->GetTangent(t, tdx, tdy, tdz);
	float tlen = sqrtf(tdx * tdx + tdy * tdy);
	if (tlen < 1e-6f) tlen = 1.f;
	float perp0X = tdy / tlen, perp0Y = -tdx / tlen;
	float sideSign = (side == 0) ? 1.f : -1.f;
	float offsetDist = LaneCenterOffset(lanes, 0);
	// 车道横断面以Connection连线为几何中心居中(见Source/Core/map/roadnet.md"车道居中"一节)，
	// 和RoadJunction::Build的makeAnchor是同一个换算：offsetDist*sideSign是"以老的side0/side1
	// 分界线为原点"算出来的有符号偏移，减去shift才是"以居中后的连线为原点"的偏移。
	float shift = (road->GetSideWidth(0) - road->GetSideWidth(1)) * 0.5f;
	float signedOffset = offsetDist * sideSign - shift;
	float nx = basePoint.GetX() + perp0X * signedOffset;
	float ny = basePoint.GetY() + perp0Y * signedOffset;

	Node* newNode = new Node(isVehicle ? "vehicle" : "pedestrian", nx, ny, basePoint.GetZ());
	navAnchorNodes.push_back(newNode);

	auto& graph = isVehicle ? vehicleNavGraph : pedestrianNavGraph;

	if (lanes.size() == 1) {
		// 唯一车道(也就是被保存的"最内侧车道")：把现有贯通线在t处切成两段。
		if (line.edge) {
			RemoveGraphEdgeOneWay(graph, line.fromAnchor->GetId(), line.toAnchor->GetId());
			if (!isVehicle) {
				RemoveGraphEdgeOneWay(graph, line.toAnchor->GetId(), line.fromAnchor->GetId());
			}
			delete line.edge;
			line.edge = nullptr;
		}
	}
	// 车道数>=2时贯通线(内侧车道)保持不变，另外新增一条外侧"起点-新Node-终点"两段线，
	// 两端仍连回该Road原本的起止锚点(和内侧线共用同一对fromAnchor/toAnchor)。

	Connection* seg1 = new Connection(*line.fromAnchor, *newNode);
	Connection* seg2 = new Connection(*newNode, *line.toAnchor);
	graph[line.fromAnchor->GetId()].emplace_back(newNode->GetId(), seg1);
	graph[newNode->GetId()].emplace_back(line.toAnchor->GetId(), seg2);
	if (!isVehicle) {
		graph[newNode->GetId()].emplace_back(line.fromAnchor->GetId(), seg1);
		graph[line.toAnchor->GetId()].emplace_back(newNode->GetId(), seg2);
	}

	RoadOpening opening;
	opening.t = t;
	opening.width = openingWidth;
	opening.forwardSide = useForwardSide;
	opening.isVehicle = isVehicle;
	road->AddOpening(opening);

	return newNode;
}

const vector<Road*>& Map::GetRoads() const {
	static const vector<Road*> empty;
	return roadnet ? roadnet->GetRoads() : empty;
}

const vector<Intersection*>& Map::GetIntersections() const {
	static const vector<Intersection*> empty;
	return roadnet ? roadnet->GetIntersections() : empty;
}

const vector<Node*>& Map::GetExterns() const {
	static const vector<Node*> empty;
	return roadnet ? roadnet->GetExterns() : empty;
}

const vector<pair<Lot*, unordered_map<int, Road*>>>& Map::GetLots() const {
	static const vector<pair<Lot*, unordered_map<int, Road*>>> empty;
	return roadnet ? roadnet->GetLots() : empty;
}

const vector<RoadJunction*>& Map::GetJunctions() const {
	return junctions;
}

Lot* Map::LocateLot(const string& road, int index) const {
	return roadnet ? roadnet->LocateLot(road, index) : nullptr;
}

const unordered_map<int, vector<pair<int, Connection*>>>& Map::GetVehicleNavGraph() const {
	return vehicleNavGraph;
}

const unordered_map<int, vector<pair<int, Connection*>>>& Map::GetPedestrianNavGraph() const {
	return pedestrianNavGraph;
}

const vector<Node*>& Map::GetNavAnchorNodes() const {
	return navAnchorNodes;
}

pair<int, int> Map::GetSize() const {
	return make_pair(width, height);
}

bool Map::CheckXY(int x, int y) const {
	return x >= 0 && y >= 0 && x < width && y < height;
}

Element& Map::At(int x, int y) {
	return elements[static_cast<size_t>(y) * width + x];
}

const Element& Map::At(int x, int y) const {
	return elements[static_cast<size_t>(y) * width + x];
}

string Map::GetTerrain(int x, int y) const {
	if (!CheckXY(x, y)) {
		debugf("Warning: Invalid coordinates (%d, %d) for map.\n", x, y);
		return "";
	}
	return At(x, y).terrain;
}

bool Map::SetTerrain(int x, int y, const string& terrain, pair<bool, float> water) {
	if (!CheckXY(x, y)) {
		debugf("Warning: Invalid coordinates (%d, %d) for map.\n", x, y);
		return false;
	}
	Element& e = At(x, y);
	e.terrain = terrain;
	e.water = water;
	return true;
}

float Map::GetHeight(int x, int y) const {
	if (!CheckXY(x, y)) {
		debugf("Warning: Invalid coordinates (%d, %d) for map.\n", x, y);
		return 0.f;
	}
	return At(x, y).height;
}

bool Map::SetHeight(int x, int y, float height) {
	if (!CheckXY(x, y)) {
		debugf("Warning: Invalid coordinates (%d, %d) for map.\n", x, y);
		return false;
	}
	At(x, y).height = height;
	return true;
}

pair<bool, float> Map::GetWater(int x, int y) const {
	if (!CheckXY(x, y)) {
		debugf("Warning: Invalid coordinates (%d, %d) for map.\n", x, y);
		return { false, 0.f };
	}
	return At(x, y).water;
}

const vector<pair<Quad, float>>& Map::GetHatches(int x, int y) const {
	static const vector<pair<Quad, float>> empty;
	if (!CheckXY(x, y)) {
		debugf("Warning: Invalid coordinates (%d, %d) for map.\n", x, y);
		return empty;
	}
	return At(x, y).hatches;
}

void Map::AddHatch(Quad q, float rotation) {
	float hw = q.GetSizeX() * 0.5f;
	float hh = q.GetSizeY() * 0.5f;
	float absCos = abs(cos(rotation));
	float absSin = abs(sin(rotation));
	float ahw = hw * absCos + hh * absSin;
	float ahh = hw * absSin + hh * absCos;

	float wx = q.GetPosX(), wy = q.GetPosY();
	int x0 = static_cast<int>(wx - ahw);
	int x1 = static_cast<int>(wx + ahw);
	int y0 = static_cast<int>(wy - ahh);
	int y1 = static_cast<int>(wy + ahh);

	for (int cy = y0; cy <= y1; cy++) {
		for (int cx = x0; cx <= x1; cx++) {
			if (!CheckXY(cx, cy)) continue;
			At(cx, cy).hatches.emplace_back(q, rotation);
		}
	}
}

const unordered_map<string, pair<int, string>>& Map::GetTerrainTextures() const {
	return terrainTextures;
}
