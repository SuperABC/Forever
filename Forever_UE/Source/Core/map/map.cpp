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

	for (Zone* z : zones) delete z;
	for (Building* b : buildings) delete b;
}

void Map::InitTerrains() {
	vector<string> mods = Config::GetMods();
	terrainFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("terrain_mods")));

	// modLoader是Map的成员(不是局部变量)——它持有的dll句柄必须活到Map析构为止,
	// terrainFactory.CreateTerrain以后随时可能被调用,详见map.h的注释。
	modLoader.RegisterConcept<TerrainFactory>(mods, "RegisterModTerrains", "FinishModTerrains", &terrainFactory);

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

	// laneIndex只在isVehicle时有意义(每条车道各自的锚点)；行人固定用该侧唯一的锚点，
	// 忽略laneIndex(见RoadJunctionApproach::pedestrianSide注释，这次没有扩展成逐车道)。
	// extern端点(地图边缘残端)没有RoadJunction，所有车道退化成同一个Node，因为地图边缘
	// 不需要精确车道级偏移几何。
	auto resolveAnchor = [&](Road* road, bool atStart, bool isVehicle, int side, int laneIndex) -> Node* {
		auto& approachMap = atStart ? startApproach : endApproach;
		auto it = approachMap.find(road);
		if (it != approachMap.end()) {
			const RoadJunctionApproach* ap = it->second;
			if (isVehicle) {
				const vector<Node*>& anchors = (side == 0)
					? (atStart ? ap->vehicleOutbound : ap->vehicleInbound)
					: (atStart ? ap->vehicleInbound : ap->vehicleOutbound);
				return (laneIndex >= 0 && laneIndex < static_cast<int>(anchors.size())) ? anchors[laneIndex] : nullptr;
			}
			return ap->pedestrianSide[side];
		}
		Node endpoint = atStart ? road->GetStart() : road->GetEnd();
		auto externIt = externById.find(endpoint.GetId());
		return (externIt != externById.end()) ? externIt->second : nullptr;
		};

	// 每条Road的贯通线：车行边单向插入(按该side实际通行方向)，行人边双向插入。每条物理车道
	// (不只是最内侧/最靠左最靠右)都各自有一条贯通线、各自的锚点(车行；行人仍然每侧共用一个
	// 锚点，见resolveAnchor注释)——这次(第九轮迁移)从"只保存最内侧/单行道两端车道"改成每条
	// 车道都有自己的贯通线和锚点，起因是PIE导航图可视化验证时发现多车道路段只画出一条线、
	// 和实际车道数对不上；如果只加贯通线条目但仍然共用同一个锚点，可视化上多条线会重叠成
	// 一条看不出区别，所以锚点本身也要逐车道化(RoadJunction::Build，见roadnet.md"车道级
	// 导航锚点"一节)。
	for (Road* road : roadnet->GetRoads()) {
		for (int cat = 0; cat < 2; cat++) { // 0=vehicle, 1=pedestrian
			bool isVehicle = (cat == 0);
			for (int side = 0; side < 2; side++) {
				const vector<float>& lanes = isVehicle ? road->GetVehicleLanes(side) : road->GetPedestrianLanes(side);
				if (lanes.empty()) continue;

				// side0沿Road Start->End方向通行(from=Start)，side1沿End->Start(from=End)。
				bool fromIsStart = (side == 0);
				int idx = cat * 2 + side;

				for (int laneIndex = 0; laneIndex < static_cast<int>(lanes.size()); laneIndex++) {
					Node* fromAnchor = resolveAnchor(road, fromIsStart, isVehicle, side, laneIndex);
					Node* toAnchor = resolveAnchor(road, !fromIsStart, isVehicle, side, laneIndex);
					if (!fromAnchor || !toAnchor) continue;

					Connection* edge = new Connection(*fromAnchor, *toAnchor);
					auto& graph = isVehicle ? vehicleNavGraph : pedestrianNavGraph;
					graph[fromAnchor->GetId()].emplace_back(toAnchor->GetId(), edge);
					if (!isVehicle) {
						graph[toAnchor->GetId()].emplace_back(fromAnchor->GetId(), edge);
					}
					throughLines[road][idx].push_back({ edge, fromAnchor, toAnchor, laneIndex });
				}
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

namespace {
	// 按剩余空闲面积(Lot::GetFreeAcreage())降序排序的全图lot列表——每个mod的Distribute()
	// 调用前都要重新算一次，因为上一个mod的显式占位可能已经改变了各lot的剩余空闲面积
	// (关键设计决策3)。
	vector<Lot*> SortLotsByFreeAcreage(const vector<Lot*>& lots) {
		vector<Lot*> sorted = lots;
		std::sort(sorted.begin(), sorted.end(), [](Lot* a, Lot* b) {
			return a->GetFreeAcreage() > b->GetFreeAcreage();
			});
		return sorted;
	}
}

void Map::InitZones() {
	vector<string> mods = Config::GetMods();
	zoneFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("zone_mods")));
	modLoader.RegisterConcept<ZoneFactory>(mods, "RegisterModZones", "FinishModZones", &zoneFactory);

	PathLaneSpec pathSpec;
	for (auto& id : zoneFactory.GetRegisteredIds()) {
		ZoneMod* scanner = zoneFactory.CreateZone(id);
		if (!scanner) continue;

		vector<Lot*> lots = SortLotsByFreeAcreage(GetLots());
		scanner->Distribute(lots);

		for (auto& request : scanner->explicitPlacements) {
			if (!request.lot) continue;
			Quad placed;
			bool success = request.lot->RequestPlacement(request.direction, request.marginStart,
				request.marginEnd, request.depth, pathSpec, &placed);
			if (success) {
				Zone* zone = new Zone(&zoneFactory, id);
				zone->SetPosition(placed.GetPosX(), placed.GetPosY(), placed.GetSizeX(), placed.GetSizeY());
				// freeLots全部继承同一个顶层Lot的rotation(Lot::SplitWithPath产出的每一段都是
				// 同一个rotation)，所以直接从request.lot取就是这块占位实际的世界朝向。
				zone->SetRotation(request.lot->GetRotation());
				zone->SetParentLot(request.lot);
				zones.push_back(zone);
			}
		}

		zoneFactory.DestroyZone(scanner);
	}
}

void Map::InitBuildings() {
	vector<string> mods = Config::GetMods();
	buildingFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("building_mods")));
	modLoader.RegisterConcept<BuildingFactory>(mods, "RegisterModBuildings", "FinishModBuildings", &buildingFactory);

	PathLaneSpec pathSpec;
	unordered_map<string, BuildingMod*> scanners; // 留到下面FillRemainder阶段查RandomAcreage/Min/Max

	for (auto& id : buildingFactory.GetRegisteredIds()) {
		BuildingMod* scanner = buildingFactory.CreateBuilding(id);
		if (!scanner) continue;

		vector<Lot*> lots = SortLotsByFreeAcreage(GetLots());
		scanner->Distribute(lots);

		// candidateWeights是mod自己push进去的纯数据(lot指针+权重)，这里由Map(Forever.dll
		// 编译的代码)代为调用lot->AddCandidate(...)——不能让mod自己直接调用这个非虚成员
		// 函数，否则Lot::candidates这个vector的内部缓冲会被mod dll的分配器分配、却由
		// Forever.dll的分配器释放，退出时析构会崩溃，详见building_mod.h的注释。
		for (auto& cw : scanner->candidateWeights) {
			if (cw.lot) cw.lot->AddCandidate(id, cw.weight);
		}

		for (auto& request : scanner->explicitPlacements) {
			if (!request.lot) continue;
			Quad placed;
			bool success = request.lot->RequestPlacement(request.direction, request.marginStart,
				request.marginEnd, request.depth, pathSpec, &placed);
			if (success) {
				Building* building = new Building(&buildingFactory, id);
				building->SetPosition(placed.GetPosX(), placed.GetPosY(), placed.GetSizeX(), placed.GetSizeY());
				building->SetRotation(request.lot->GetRotation());
				building->SetParentLot(request.lot);
				buildings.push_back(building);
			}
		}

		scanners[id] = scanner;
	}

	for (Lot* lot : GetLots()) {
		auto randomAcreage = [&scanners](const string& type) -> float {
			auto it = scanners.find(type);
			return it != scanners.end() ? it->second->RandomAcreage() : 0.f;
			};
		auto acreageMinMax = [&scanners](const string& type) -> pair<float, float> {
			auto it = scanners.find(type);
			if (it == scanners.end()) return { 0.f, 0.f };
			return { it->second->GetAcreageMin(), it->second->GetAcreageMax() };
			};

		auto results = lot->FillRemainder(pathSpec, randomAcreage, acreageMinMax);

		for (auto& result : results) {
			Building* building = new Building(&buildingFactory, result.type);
			building->SetPosition(result.footprint.GetPosX(), result.footprint.GetPosY(),
				result.footprint.GetSizeX(), result.footprint.GetSizeY());
			// FillRemainder是在lot自己的freeLots池里切的，同样继承lot这个顶层Lot的rotation。
			building->SetRotation(lot->GetRotation());
			building->SetParentLot(lot);
			buildings.push_back(building);
		}

		lot->ClearCandidates();
	}

	for (auto& [id, scanner] : scanners) {
		buildingFactory.DestroyBuilding(scanner);
	}
}

const vector<Zone*>& Map::GetZones() const {
	return zones;
}

const vector<Building*>& Map::GetBuildings() const {
	return buildings;
}

vector<Road*> Map::GetPathRoads() const {
	vector<Road*> result;
	for (Lot* lot : GetLots()) {
		for (Road* r : lot->GetPathRoads()) {
			result.push_back(r);
		}
	}
	return result;
}

const string& Map::GetPathRoadMaterial() const {
	static const string empty;
	return roadnet ? roadnet->GetPathRoadMaterial() : empty;
}

Node* Map::AddRoadAccessNode(const string& roadName, float t, bool isVehicle, bool useForwardSide, float openingWidth) {
	if (!roadnet) return nullptr;

	Road* road = nullptr;
	for (Road* r : roadnet->GetRoads()) {
		if (r->GetName() == roadName) { road = r; break; }
	}
	if (!road) return nullptr;

	int requestedSide = useForwardSide ? 0 : 1;
	const vector<float>& requestedLanes = isVehicle ? road->GetVehicleLanes(requestedSide) : road->GetPedestrianLanes(requestedSide);
	const vector<float>& oppositeLanes = isVehicle ? road->GetVehicleLanes(1 - requestedSide) : road->GetPedestrianLanes(1 - requestedSide);

	// 单行道特殊情况：请求的side本身没有对应类别车道，但对侧有——说明这是单行道，
	// useForwardSide不再表示"正向/反向"，改按"要最靠右(true)还是最靠左(false)的车道"重新
	// 解释，实际车道从对侧取，见map.h的AddRoadAccessNode注释。两侧都没有车道就是真的没有
	// 对应类别的通行空间，返回nullptr。
	int side;
	if (!requestedLanes.empty()) {
		side = requestedSide;
	}
	else if (!oppositeLanes.empty()) {
		side = 1 - requestedSide;
	}
	else {
		return nullptr;
	}
	const vector<float>& lanes = isVehicle ? road->GetVehicleLanes(side) : road->GetPedestrianLanes(side);
	const vector<float>& otherSideLanes = isVehicle ? road->GetVehicleLanes(1 - side) : road->GetPedestrianLanes(1 - side);
	bool oneWay = otherSideLanes.empty();

	float sideSign = (side == 0) ? 1.f : -1.f;
	float shift = (road->GetSideWidth(0) - road->GetSideWidth(1)) * 0.5f;

	// 车道下标选择：每条车道现在都有自己专属的贯通线(见InitRoadnet的建图注释)，不再需要
	// "内侧线不动、外侧新增分支"这套workaround——直接选出目标车道，找到它自己的贯通线断开
	// 就行。双向路(对侧也有同类车道)没有方向可选，固定用最外侧车道(和原始设计"新访问点代表
	// 外侧车道"一致，唯一车道时"最外侧"就是它自己)；单行路(对侧完全没有同类车道)没有
	// "内外侧"参照，按useForwardSide要求的左右方向，在这一侧的车道里找有符号偏移(居中后，
	// 见roadnet.md"车道居中"一节)最靠近那个方向极值的车道——useForwardSide=true要最靠右
	// (偏移最大)，false要最靠左(偏移最小)。
	int laneIndex = 0;
	if (lanes.size() >= 2) {
		if (oneWay) {
			float bestOffset = 0.f;
			bool first = true;
			for (int i = 0; i < static_cast<int>(lanes.size()); i++) {
				float candidate = LaneCenterOffset(lanes, i) * sideSign - shift;
				bool better = first || (useForwardSide ? (candidate > bestOffset) : (candidate < bestOffset));
				if (better) {
					bestOffset = candidate;
					laneIndex = i;
					first = false;
				}
			}
		}
		else {
			laneIndex = static_cast<int>(lanes.size()) - 1;
		}
	}

	int idx = (isVehicle ? 0 : 2) + side;
	vector<ThroughLine>& lines = throughLines[road][idx];
	ThroughLine* line = nullptr;
	for (ThroughLine& l : lines) {
		if (l.laneIndex == laneIndex) { line = &l; break; }
	}
	if (!line || !line->fromAnchor || !line->toAnchor) return nullptr;

	Node basePoint = road->GetPoint(t);
	float tdx, tdy, tdz;
	road->GetTangent(t, tdx, tdy, tdz);
	float tlen = sqrtf(tdx * tdx + tdy * tdy);
	if (tlen < 1e-6f) tlen = 1.f;
	float perp0X = tdy / tlen, perp0Y = -tdx / tlen;
	// 车道横断面以Connection连线为几何中心居中(见Source/Core/map/roadnet.md"车道居中"一节)，
	// 和RoadJunction::Build的makeAnchor是同一个换算：offsetDist*sideSign是"以老的side0/side1
	// 分界线为原点"算出来的有符号偏移，减去shift才是"以居中后的连线为原点"的偏移。
	float offsetDist = LaneCenterOffset(lanes, laneIndex);
	float signedOffset = offsetDist * sideSign - shift;
	float nx = basePoint.GetX() + perp0X * signedOffset;
	float ny = basePoint.GetY() + perp0Y * signedOffset;

	Node* newNode = new Node(isVehicle ? "vehicle" : "pedestrian", nx, ny, basePoint.GetZ());
	navAnchorNodes.push_back(newNode);

	auto& graph = isVehicle ? vehicleNavGraph : pedestrianNavGraph;

	// 目标车道自己的贯通线直接断开——每条车道都有专属贯通线之后，这里不再需要按车道数分支。
	if (line->edge) {
		RemoveGraphEdgeOneWay(graph, line->fromAnchor->GetId(), line->toAnchor->GetId());
		if (!isVehicle) {
			RemoveGraphEdgeOneWay(graph, line->toAnchor->GetId(), line->fromAnchor->GetId());
		}
		delete line->edge;
		line->edge = nullptr;
	}

	Connection* seg1 = new Connection(*line->fromAnchor, *newNode);
	Connection* seg2 = new Connection(*newNode, *line->toAnchor);
	graph[line->fromAnchor->GetId()].emplace_back(newNode->GetId(), seg1);
	graph[newNode->GetId()].emplace_back(line->toAnchor->GetId(), seg2);
	if (!isVehicle) {
		graph[newNode->GetId()].emplace_back(line->fromAnchor->GetId(), seg1);
		graph[line->toAnchor->GetId()].emplace_back(newNode->GetId(), seg2);
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

const vector<Lot*>& Map::GetLots() const {
	static const vector<Lot*> empty;
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
