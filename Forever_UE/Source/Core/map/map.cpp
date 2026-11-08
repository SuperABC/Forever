#include "map.h"

#include "common/utility.h"

#include "common/config.h"
#include "common/registry.h"
#include "map/room.h"
#include "player/asset.h"
#include "populace/populace.h"
#include "populace/citizen.h"
#include "story/change.h"
#include "story/script.h"
#include "story/script_factory.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <limits>
#include <map>
#include <queue>
#include <sstream>
#include <unordered_set>


using namespace std;

namespace {
	// 室外起点接入车道时，车道方向与车头夹角180度折合的额外距离（地图单位，1.5=15m）。
	constexpr float kHeadingPenaltyMapUnits = 1.5f;
	constexpr int kEntryRoadCandidates = 2; // 入线时只比较最近的几条有车道的路（每条两个方向，最多 2x 次寻路）

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

	// 和roadnet.cpp里的同名私有helper逻辑一致，这里独立一份。
	float SumWidths(const vector<float>& lanes) {
		float sum = 0.f;
		for (float w : lanes) sum += w;
		return sum;
	}

	void RemoveGraphEdgeOneWay(unordered_map<int, vector<pair<int, Connection*>>>& graph, int fromId, int toId) {
		auto it = graph.find(fromId);
		if (it == graph.end()) return;
		auto& edges = it->second;
		edges.erase(remove_if(edges.begin(), edges.end(),
			[toId](const pair<int, Connection*>& e) { return e.first == toId; }), edges.end());
	}

		// Assign()通过PlacementEmitFunc回调交回占位请求：函数体编译在Core这一侧，push_back用Core自己的分配器(跨DLL安全)。
	void EmitPlacementRequest(void* context, const LotPlacementRequest& request) {
		static_cast<vector<LotPlacementRequest>*>(context)->push_back(request);
	}
}

Map::Map(int width, int height) :
	width(width),
	height(height),
	elements(static_cast<size_t>(width)* height),
	terrainFactory(Registry::Get().GetTerrainFactory()),
	roadnetFactory(Registry::Get().GetRoadnetFactory()),
	zoneFactory(Registry::Get().GetZoneFactory()),
	buildingFactory(Registry::Get().GetBuildingFactory()),
	roomFactory(Registry::Get().GetRoomFactory()),
	componentFactory(Registry::Get().GetComponentFactory()),
	scriptFactory(Registry::Get().GetScriptFactory()),
	terrainTextures() {
	TerrainTextureInfo plainTexture{ 0,
		"/Game/Asset/Textures/Terrain/PlainDiffuse.PlainDiffuse",
		"/Game/Asset/Textures/Terrain/PlainNormal.PlainNormal",
		"/Game/Asset/Textures/Terrain/PlainRoughness.PlainRoughness" };
	terrainTextures = {
		{ "plain", plainTexture },
		{ "construction", plainTexture }
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

	for (auto& [name, z] : zones) delete z;
	for (auto& [name, b] : buildings) delete b;
	for (Lot* lot : orphanLots) delete lot;
}

void Map::InitTerrains() {
	// 这两个回调会传进Basic等mod DLL：std::string一律按引用传，由各自模块自己拷贝——按值返回/传参时
	// 一边分配一边释放，mod的CRT堆和Forever模块(UE的FMemory)不是同一个，超过短字符串长度(15字符，
	// 比如"terrain_mountain")就会在释放时崩溃。
	auto getTerrain = [this](int x, int y) -> const string& { return this->GetTerrainRef(x, y); };
	auto setTerrain = [this](int x, int y, const string& terrain) -> bool {
		return this->SetTerrain(x, y, terrain);
		};
	auto getHeight = [this](int x, int y) -> float {
		return this->GetHeight(x, y);
		};
	auto setHeight = [this](int x, int y, float height) -> bool {
		return this->SetHeight(x, y, height);
		};
	// 水面per-cell独立设置,不再跟着SetTerrain走一个全局共用值——每个mod自己决定哪些格子
	// 要水、水位多少，同一次DistributeTerrain调用里不同格子可以给不同水位(倾斜水面)。
	auto setWater = [this](int x, int y, bool hasWater, float h) -> bool {
		return this->SetWater(x, y, hasWater, h);
		};
	auto setFineHeights = [this](int x, int y, const float* data) -> bool {
		return this->SetFineHeights(x, y, data);
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
		TerrainTexturePaths texturePaths = terrains[i]->GetTextures();
		terrainTextures[terrains[i]->GetType()] = { static_cast<int>(i) + preset,
			texturePaths.diffuse, texturePaths.normal, texturePaths.roughness };
		terrains[i]->DistributeTerrain(width, height, getTerrain, setTerrain, getHeight, setHeight, setWater, setFineHeights);
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
	if (roadnet) {
		debugf("Warning: roadnet already exists (generated or restored from save), skip InitRoadnet.\n");
		return;
	}
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

	auto getTerrain = [this](int x, int y) -> const string& { return this->GetTerrainRef(x, y); };
	auto getWater = [this](int x, int y) -> pair<bool, float> { return this->GetWater(x, y); };
	auto getHeight = [this](float x, float y) -> float { return this->SampleHeight(x, y); };
	roadnet->DistributeRoadnet(width, height, getTerrain, getWater, getHeight, Node::GetCount());
	roadnet->AllocateAddress();

	// 把mod通过RoadnetMod::AddHatch产出的地形挖洞标记(隧道口用)转发进Map::AddHatch，
	// 复用Terrain已有的挖洞渲染机制——AddHatch自己会按Quad的旋转AABB分发到重叠的Element。
	for (const TerrainHatch& hatch : roadnet->GetHatches()) {
		AddHatch(hatch);
	}

	// 地形贴合：路面高度已经由mod定好，把路面下和路边的地形高度烤到路面上。
	ApplyTerrainFits(roadnet->GetTerrainFits());

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

	// 还有一类端点既不是RoadJunction也不是extern：mod把一条路拆成多段独立Road时的中间分段点(如JingRoadnet隧道的flatNode/splitNode，
	// 见roadnet_basic.md"隧道")。它们不是Intersection(走RoadJunction::Build会摆出强制水平的路口平面)，也不能像extern那样把所有车道
	// 收成一点(两侧车道配置一致，应按真实宽度摆开)，所以现算"直接经过"锚点，详见map.md"InitRoadnet"。
	// passthroughAnchorCache按(端点id,车行/行人,side,laneIndex)缓存，保证前后两段Road在分段点拿到同一个Node*；key用位运算压缩(节点id远小于2^47)。
	unordered_map<int64_t, Node*> passthroughAnchorCache;
	auto makePassthroughKey = [](int nodeId, bool isVehicle, int side, int laneIndex) -> int64_t {
		return (static_cast<int64_t>(nodeId) << 16) | (isVehicle ? (1LL << 15) : 0)
			| (static_cast<int64_t>(side) << 8) | static_cast<int64_t>(laneIndex & 0xFF);
		};
	// 现算一个"直接经过"锚点：位置=端点坐标+沿该端切线的右手垂线方向(和RoadJunction::Build
	// 里makeAnchor同一套约定)按车道宽度偏移，setback=0(这类点没有喇叭口，不需要沿路收缩)。
	auto computePassthroughAnchor = [](Road* road, bool atStart, bool isVehicle, int side, int laneIndex) -> Node* {
		float tdx, tdy, tdz;
		road->GetTangent(atStart ? 0.f : 1.f, tdx, tdy, tdz);
		float tlen = sqrt(tdx * tdx + tdy * tdy);
		if (tlen < 1e-6f) tlen = 1.f;
			// 右手边是(-tdy,tdx)：本项目+Y=南，见ComputeLaneAnchorPosition实现处。
		float perp0X = -tdy / tlen, perp0Y = tdx / tlen;

		float side0Width = road->GetSideWidth(0), side1Width = road->GetSideWidth(1);
		float shift = (side0Width - side1Width) * 0.5f;

		float offsetDist;
		if (isVehicle) {
			const vector<float>& lanes = road->GetVehicleLanes(side);
			if (laneIndex < 0 || laneIndex >= static_cast<int>(lanes.size())) return nullptr;
			offsetDist = LaneCenterOffset(lanes, laneIndex);
		} else {
			if (road->GetPedestrianLanes(side).empty()) return nullptr;
			offsetDist = SumWidths(road->GetVehicleLanes(side)) + SumWidths(road->GetParkingLanes(side))
				+ LaneCenterOffset(road->GetPedestrianLanes(side), 0);
		}
		float sideSign = (side == 0) ? 1.f : -1.f;
		float signedOffset = offsetDist * sideSign - shift;

		Node endpoint = atStart ? road->GetStart() : road->GetEnd();
		float x = endpoint.GetX() + perp0X * signedOffset;
		float y = endpoint.GetY() + perp0Y * signedOffset;
		return new Node("roadnet", x, y, endpoint.GetZ());
		};

	// laneIndex只在isVehicle时有意义(每条车道各自的锚点)；行人固定用该侧唯一的锚点，
	// 忽略laneIndex(见RoadJunctionApproach::pedestrianSide注释，这次没有扩展成逐车道)。
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
		if (externIt != externById.end()) return externIt->second; // 地图边缘：没有真正分叉，
			// 所有车道退化成同一个点，见上面externById注释。

		int64_t key = makePassthroughKey(endpoint.GetId(), isVehicle, side, isVehicle ? laneIndex : 0);
		auto cacheIt = passthroughAnchorCache.find(key);
		if (cacheIt != passthroughAnchorCache.end()) return cacheIt->second;
		Node* anchor = computePassthroughAnchor(road, atStart, isVehicle, side, laneIndex);
		if (!anchor) return nullptr;
		navAnchorNodes.push_back(anchor);
		passthroughAnchorCache[key] = anchor;
		return anchor;
		};

	// 每条Road的贯通线：车行边单向(按该side通行方向)，行人边双向；每条物理车道各有一条贯通线和各自的锚点
	// (车行逐车道，行人每侧共用一个锚点)，否则多条线共用端点会重叠，见roadnet.md"车道级导航锚点"。
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
					throughLines[road][idx].push_back({ edge, fromAnchor, toAnchor, laneIndex, 0.f, 1.f });
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
		// 按剩余空闲面积降序排序的lot列表；每个mod的Assign前都要重新排，因为上一个mod的占位会改变各lot的剩余面积。
	vector<Lot*> SortLotsByFreeAcreage(const vector<Lot*>& lots) {
		vector<Lot*> sorted = lots;
		std::sort(sorted.begin(), sorted.end(), [](Lot* a, Lot* b) {
			return a->GetFreeAcreage() > b->GetFreeAcreage();
			});
		return sorted;
	}

	// ResolvePathEndAnchors()开头那段"小路自己方向 vs host的perp0做点积判断近侧"的纯查询版本
	// (不产生任何副作用)——FlushPendingPathRoadLinks()排序前用它预判一条link的某一端最终会
	// 落在host road哪一侧，不需要真的执行断开。和ResolveAccessLane同一个"拆出纯查询版本给排序
	// 阶段预判"的做法。
	int ResolveNearSide(Road* path, bool isStartEnd, Road* hostRoad, float hostT) {
		Node baseNode = isStartEnd ? path->GetStart() : path->GetEnd();
		Node otherNode = isStartEnd ? path->GetEnd() : path->GetStart();
		float dirX = otherNode.GetX() - baseNode.GetX();
		float dirY = otherNode.GetY() - baseNode.GetY();
		float dirLen = sqrt(dirX * dirX + dirY * dirY);
		if (dirLen < 1e-6f) dirLen = 1.f;
		dirX /= dirLen; dirY /= dirLen;

		float hdx, hdy, hdz;
		hostRoad->GetTangent(hostT, hdx, hdy, hdz);
		float hlen = sqrt(hdx * hdx + hdy * hdy);
		if (hlen < 1e-6f) hlen = 1.f;
		// 右手边修正，见Map::ComputeLaneAnchorPosition实现处的说明：+Y=南，公式是(-hdy,hdx)。
		float hostPerp0X = -hdy / hlen, hostPerp0Y = hdx / hlen;

		float dot = dirX * hostPerp0X + dirY * hostPerp0Y;
		return (dot >= 0.f) ? 0 : 1;
	}
}

void Map::InitZones() {
	// 一个Zone独占一个mod实例：先用static Assign一次扫完全图lot拿到占位请求(不需要实例)，RequestPlacement成功才CreateZone，
	// 详见map.md"InitZones"。
	PathLaneSpec pathSpec;
	for (auto& id : zoneFactory.GetRegisteredIds()) {
		vector<Lot*> lots = SortLotsByFreeAcreage(GetLots());
		vector<LotPlacementRequest> requests;
		zoneFactory.Assign(id, lots, &EmitPlacementRequest, &requests);

		for (auto& request : requests) {
			Lot* lot = request.lot;
			if (!lot) continue;
			Quad placedQuad;
			unordered_map<int, Road*> boundaryRoads;
			size_t linksBefore = lot->GetPathRoadLinks().size();
			bool success = lot->RequestPlacement(request.direction, request.marginStart,
				request.marginEnd, request.depth, pathSpec, &placedQuad, &boundaryRoads);
			// 不管这次placement最终成功还是失败都要接图——SplitWithPath产出的小路即使整体
			// 请求失败也已经是真实持久化的几何(被某个freeLot的边界引用着)，处理顺序天然
			// =创建顺序(同一顶层Lot内部cascading cut时，后一刀如果连到前一刀新建的小路，
			// 前一刀的link一定排在更靠前的位置，先被处理)。
			const auto& allLinks = lot->GetPathRoadLinks();
			for (size_t i = linksBefore; i < allLinks.size(); i++) {
				pendingPathRoadLinks.push_back(allLinks[i]);
			}
			if (!success) continue;

			ZoneMod* mod = zoneFactory.CreateZone(id); // 到这里才真正创建，唯一一次
			if (!mod) continue;
			Zone* zone = new Zone(&zoneFactory, mod);
			zone->SetPosition(placedQuad.GetPosX(), placedQuad.GetPosY(), placedQuad.GetSizeX(), placedQuad.GetSizeY());
			// Zone::GetRotation()直接转发parentLot->GetRotation()，不需要另外调SetRotation。
			zone->SetParentLot(lot);
			for (auto& [dir, road] : boundaryRoads) {
				zone->SetBoundaryRoad(dir, road);
			}
			zone->Layout(request.direction); // 必须在SetPosition/SetBoundaryRoad之后
			zone->BuildDoors(*this); // 园区大门——Layout()填好mod->gates之后才能读到数据
			BuildZoneScript(zone);

			// 出入口和内部道路共用一份anchorCache(每个zone一份)：坐标+类别重合就复用同一个zone侧锚点。围墙/大门无需搬运(Zone直接转发mod)。
			vector<tuple<float, float, bool, Node*>> anchorCache;
			for (const ZoneAccessPoint& pt : mod->vehicleEntries) {
				ConnectZoneAccessPoint(zone, pt.x, pt.y, pt.width, true, true, anchorCache);
			}
			for (const ZoneAccessPoint& pt : mod->vehicleExits) {
				ConnectZoneAccessPoint(zone, pt.x, pt.y, pt.width, true, false, anchorCache);
			}
			for (const ZoneAccessPoint& pt : mod->pedestrianAccess) {
				ConnectZoneAccessPoint(zone, pt.x, pt.y, pt.width, false, true, anchorCache);
			}

			vector<Road*> builtInternalRoads;
			for (const ZoneInternalRoadSpec& roadSpec : mod->internalRoads) {
				builtInternalRoads.push_back(ConnectZoneInternalRoad(zone, roadSpec, anchorCache));
			}
			zone->SetInternalRoads(builtInternalRoads);

			// 内部建筑统一由InitBuildings落地(Layout要用InitBuildings开头才加载的buildingLayoutLibrary)，直接读zone->GetMod()->internalBuildings。

			if (!AddZone(zone)) {
				delete zone; // ~Zone()里factory->DestroyZone(mod)会跟着跑，重名时不留悬空引用
			}
		}
	}
}

void Map::PromoteAdaptiveRemainder(Lot* parent, Lot* usedChild) {
	if (!parent || !usedChild || parent == usedChild) return;

	auto& parentPool = parent->GetFreeLots();
	for (size_t i = 0; i < parentPool.size(); i++) {
		if (parentPool[i] == usedChild) {
			parentPool.erase(parentPool.begin() + i);
			break;
		}
	}

	// usedChild自己已经被RequestPlacement用过一次，这次GetFreeLots()只是读它内部真实的survivor
	// 列表(freeLotsInitialized早已是true，不会再触发lazy-init分配)。
	auto& childPool = usedChild->GetFreeLots();
	for (Lot* survivor : childPool) {
		parentPool.push_back(survivor);
	}
	childPool.clear();

	parent->AdoptPathRoadLinks(usedChild->TakePathRoadLinks());
}

void Map::InitBuildings() {
	buildingLayoutLibrary.ReadTemplates(Config::GetLayouts());

	PathLaneSpec pathSpec;

	// 显式占位：static Assign一次扫完全部lot拿到占位请求，RequestPlacement成功才CreateBuilding(问的过程不需要实例)。
	for (auto& id : buildingFactory.GetRegisteredIds()) {
		vector<LotPlacementRequest> requests;
		buildingFactory.Assign(id, GetLots(), &EmitPlacementRequest, &requests);

		for (auto& request : requests) {
			Lot* lot = request.lot;
			if (!lot) continue;
			Quad placedQuad;
			unordered_map<int, Road*> boundaryRoads;
			size_t linksBefore = lot->GetPathRoadLinks().size();
			bool success = lot->RequestPlacement(request.direction, request.marginStart,
				request.marginEnd, request.depth, pathSpec, &placedQuad, &boundaryRoads);
			const auto& allLinks = lot->GetPathRoadLinks();
			for (size_t i = linksBefore; i < allLinks.size(); i++) {
				pendingPathRoadLinks.push_back(allLinks[i]);
			}
			if (!success) continue;

			// request.lot是FindAdaptivePlacement(mod DLL内纯读)命中的嵌套子块时adaptiveParent非空：要把子块用剩的空间/新小路搬回
			// adaptiveParent，下一个building类型才能看到真实剩余空间，见map.md"自适应占位"。
			if (request.adaptiveParent) {
				PromoteAdaptiveRemainder(request.adaptiveParent, lot);
				// 自适应命中的子块是Lot::FindAdaptivePlacement点中的freeLots嵌套子块，不是路网分配过地址
				// 的顶层Lot，GetAddresses()为空，building->GetAddress()会退化成空路名(" <楼名>")，
				// Map::LocateBuilding/LocateRoom、npc_teleport这类按地址找房间的都找不到它。子块属于
				// adaptiveParent这块地，直接沿用它的地址(LocateBuilding按地址比较所属地块，见SameBlock)。
				if (lot->GetAddresses().empty()) {
					for (const auto& [road, index] : request.adaptiveParent->GetAddresses()) {
						lot->AddAddress(road, index);
					}
				}
			}

			BuildingMod* mod = buildingFactory.CreateBuilding(id); // 到这里才真正创建，唯一一次
			if (!mod) continue;
			Building* building = new Building(&buildingFactory, mod);
			building->SetPosition(placedQuad.GetPosX(), placedQuad.GetPosY(),
				placedQuad.GetSizeX(), placedQuad.GetSizeY());
			building->SetParentLot(lot);
			for (auto& [dir, road] : boundaryRoads) {
				building->SetBoundaryRoad(dir, road);
			}
			BuildingNavResult navResult;
			building->Layout(request.direction, buildingLayoutLibrary, roomFactory, componentFactory, navResult);
			// 显式占位传真实direction
			MergeBuildingNavigation(building, navResult);
			ForwardBuildingHatches(building);
			building->BuildDoors(*this);
			building->BuildCabins(*this);
			BuildBuildingScripts(building);
			if (!AddBuilding(building)) delete building; // ~Building()里DestroyBuilding(mod)会跟着跑
		}
	}

	// 权重登记：不再需要任何mod实例，直接查BuildingFactory注册的static GetPower(area)，按
	// lot->GetArea()索引，登记进lot->AddCandidate(...)供下面FillRemainder使用。
	for (auto& id : buildingFactory.GetRegisteredIds()) {
		for (Lot* lot : GetLots()) {
			float weight = buildingFactory.GetPower(id, lot->GetArea());
			if (weight > 0.f) lot->AddCandidate(id, weight);
		}
	}

	// 园区内部建筑：每个spec单独new一个独占mod实例。
	for (auto& [name, zone] : zones) {
		if (!zone) continue;
		const vector<ZoneInternalBuildingSpec>& specs = zone->GetMod()->internalBuildings;
		const vector<Road*>& zoneInternalRoads = zone->GetInternalRoads();
		for (const ZoneInternalBuildingSpec& spec : specs) {
			BuildingMod* mod = buildingFactory.CreateBuilding(spec.type);
			if (!mod) continue;
			Building* building = PlaceZoneInternalBuilding(zone, spec, zoneInternalRoads, mod);
			BuildingNavResult navResult;
			building->Layout(spec.direction, buildingLayoutLibrary, roomFactory, componentFactory, navResult);
			// 用spec自己声明的朝向
			MergeBuildingNavigation(building, navResult);
			ForwardBuildingHatches(building);
			building->BuildDoors(*this);
			building->BuildCabins(*this);
			BuildBuildingScripts(building);
			if (AddBuilding(building)) {
				zone->AddInternalBuilding(building);
			}
			else {
				delete building; // ~Building()里factory->DestroyBuilding(mod)会跟着跑
			}
		}
	}

	// FillRemainder：randomAcreage/acreageMinMax直接转发BuildingFactory的static查询，不需要
	// scanners。真正产出一个结果才new一个独占mod实例。
	for (Lot* lot : GetLots()) {
		auto randomAcreage = [this](const string& type) -> float {
			return buildingFactory.RandomAcreage(type);
			};
		auto acreageMinMax = [this](const string& type) -> pair<float, float> {
			return { buildingFactory.GetAcreageMin(type), buildingFactory.GetAcreageMax(type) };
			};

		size_t linksBefore = lot->GetPathRoadLinks().size();
		auto results = lot->FillRemainder(pathSpec, randomAcreage, acreageMinMax);
		const auto& allLinks = lot->GetPathRoadLinks();
		for (size_t i = linksBefore; i < allLinks.size(); i++) {
			pendingPathRoadLinks.push_back(allLinks[i]);
		}

		for (auto& result : results) {
			BuildingMod* mod = buildingFactory.CreateBuilding(result.type);
			if (!mod) continue;
			Building* building = new Building(&buildingFactory, mod);
			building->SetPosition(result.footprint.GetPosX(), result.footprint.GetPosY(),
				result.footprint.GetSizeX(), result.footprint.GetSizeY());
			// Building::GetRotation()直接转发parentLot->GetRotation()，不需要另外调SetRotation。
			building->SetParentLot(lot);
			for (auto& [dir, road] : result.boundaryRoads) {
				building->SetBoundaryRoad(dir, road);
			}
			BuildingNavResult navResult;
			building->Layout(-1, buildingLayoutLibrary, roomFactory, componentFactory, navResult);
			// FillRemainder落地没有direction概念，传-1(mod可能兜底选方向，见building_mod.h)
			MergeBuildingNavigation(building, navResult);
			ForwardBuildingHatches(building);
			building->BuildDoors(*this);
			building->BuildCabins(*this);
			BuildBuildingScripts(building);
			if (!AddBuilding(building)) delete building;
		}

		lot->ClearCandidates();
	}

	// 三段落地循环全部跑完后才统一按物理顺序接入小路(不能循环内就地ConnectPathRoad)；必须先于FlushPendingBuildingRoadAccess，
	// 因为后者的BreakThroughLine会读/改throughLines。见map.md"ConnectPathRoad"。
	FlushPendingPathRoadLinks();

	// 同理，building的outside端点也统一按物理顺序接入，见map.md"Building内部布局"。
	FlushPendingBuildingRoadAccess();
}

float Map::ProjectPointOntoRoad(Road* road, float px, float py) {
	if (road->GetControls().empty()) {
		// 直线：Start到End，点到线段投影公式，O(1)精确——井字路网里绝大多数路段都是这种情况。
		Node start = road->GetStart(), end = road->GetEnd();
		float ax = start.GetX(), ay = start.GetY(), bx = end.GetX(), by = end.GetY();
		float dx = bx - ax, dy = by - ay;
		float lenSq = dx * dx + dy * dy;
		if (lenSq <= 0.f) return 0.f;
		float t = ((px - ax) * dx + (py - ay) * dy) / lenSq;
		return max(0.f, min(1.f, t));
	}

	// 曲线(比如隧道引道的S形下坡)：先粗采样定位大致区间，再反复局部细化收窄到最近点。
	constexpr int kCoarseSamples = 32;
	float bestT = 0.f;
	float bestDistSq = numeric_limits<float>::max();
	for (int i = 0; i <= kCoarseSamples; i++) {
		float t = static_cast<float>(i) / kCoarseSamples;
		Node p = road->GetPoint(t);
		float dx = p.GetX() - px, dy = p.GetY() - py;
		float d = dx * dx + dy * dy;
		if (d < bestDistSq) { bestDistSq = d; bestT = t; }
	}
	float span = 1.f / kCoarseSamples;
	constexpr int kFineSamples = 16;
	for (int iter = 0; iter < 4; iter++) {
		float lo = max(0.f, bestT - span), hi = min(1.f, bestT + span);
		for (int i = 0; i <= kFineSamples; i++) {
			float t = lo + (hi - lo) * static_cast<float>(i) / kFineSamples;
			Node p = road->GetPoint(t);
			float dx = p.GetX() - px, dy = p.GetY() - py;
			float d = dx * dx + dy * dy;
			if (d < bestDistSq) { bestDistSq = d; bestT = t; }
		}
		span = (hi - lo) / kFineSamples;
	}
	return bestT;
}

void Map::MergeBuildingNavigation(Building* building, const BuildingNavResult& result) {
	for (Node* node : result.nodes) {
		navAnchorNodes.push_back(node);
	}
	for (Node* node : result.vehicleNodes) {
		navAnchorNodes.push_back(node);
	}

	for (Connection* conn : result.connections) {
		Node start = conn->GetStart(), end = conn->GetEnd();
		pedestrianNavGraph[start.GetId()].emplace_back(end.GetId(), conn);
		pedestrianNavGraph[end.GetId()].emplace_back(start.GetId(), conn);
	}
	// 车行图只按Connection的Start->End单向插入(方向由Building::BuildVehicleNavigation算好，Map不重新判断)。
	for (Connection* conn : result.vehicleConnections) {
		Node start = conn->GetStart(), end = conn->GetEnd();
		vehicleNavGraph[start.GetId()].emplace_back(end.GetId(), conn);
	}

	Road* road = building->GetBoundaryRoad(building->GetDirection());

	// building在Road哪一侧+对应弧长比例t——行人/车辆的outside端点接路网都要用这同一套算法，
	// 拆成一个lambda共用。
	auto computeRoadSide = [&](Node* outsideNode, float& outT, bool& outUseForwardSide) {
		outT = ProjectPointOntoRoad(road, outsideNode->GetX(), outsideNode->GetY());
		Node basePoint = road->GetPoint(outT);
		float tdx, tdy, tdz;
		road->GetTangent(outT, tdx, tdy, tdz);
		float tlen = sqrtf(tdx * tdx + tdy * tdy);
		if (tlen < 1e-6f) tlen = 1.f;
			// 右手边是(-tdy,tdx)：本项目+Y=南，见ComputeLaneAnchorPosition实现处。
		float perp0X = -tdy / tlen, perp0Y = tdx / tlen;
		float dx = outsideNode->GetX() - basePoint.GetX(), dy = outsideNode->GetY() - basePoint.GetY();
		// building在Road哪一侧：比较building中心相对Road中心线在t处的法向偏移符号
		// (和RoadJunction::Build/resolveAnchor同一套perp0=右手垂线约定，见roadnet.md
		// "车道居中"一节)——偏移同号(>=0)就是side0(右手边)，useForwardSide=true。
		outUseForwardSide = (dx * perp0X + dy * perp0Y) >= 0.f;
		};

	for (Node* outsideNode : result.outsideNodes) {
		// 不在这里push进navAnchorNodes：outsideNode本来就在result.nodes里，再push会在~Map()时double free。
		if (!road) continue; // 没有边界Road(mod没能选出方向)：只留在result.nodes里，不连路网

		float t; bool useForwardSide;
		computeRoadSide(outsideNode, t, useForwardSide);

		// 只记下来，不立即AddRoadAccessNode：同一条路上的断开必须按物理位置(t)排序，见FlushPendingBuildingRoadAccess。
		pendingBuildingRoadAccess.push_back({ road, t, useForwardSide, outsideNode, false, false });
	}

	// 显式标记的入口/出口("entrance"/"exit"端点)：方向直接由清单决定，同一个node可以同时
	// 在两个清单里（各产生一个接路网的开口）。
	if (!road) return;
	for (int pass = 0; pass < 2; pass++) {
		bool isExit = pass == 1;
		const vector<Node*>& markedNodes = isExit ? result.vehicleExitNodes : result.vehicleEntranceNodes;
		for (Node* markedNode : markedNodes) {
			float t; bool useForwardSide;
			computeRoadSide(markedNode, t, useForwardSide);
			pendingBuildingRoadAccess.push_back({ road, t, useForwardSide, markedNode, true, isExit });
		}
	}
}

void Map::ForwardBuildingHatches(Building* building) {
	if (!building || building->GetBasementCount() <= 0) return;

	const Floor* floor = building->GetFloor(-1);
	if (!floor) return;

	float rotation = building->GetRotation();
	for (const Hatch& hatch : floor->GetHatches()) {
		auto [wx, wy] = building->LocalToWorld(hatch.GetPosX(), hatch.GetPosY());
		Quad q;
		q.SetPosition(wx, wy, hatch.GetSizeX(), hatch.GetSizeY());
		AddHatch(q, rotation);
	}
}

void Map::InitVegetation() {
	auto canPlace = [this](float x, float y) { return CanPlaceTree(x, y); };

	// 三个宿主都支持单棵/一排/一片三种请求(TreeRequests，见feature_spec.h)，各自只提供参考系的换算
	// (TreeFrame)，展开逻辑统一在Vegetation::ExpandRequests。
	int roadTrees = 0;
	if (roadnet) {
		roadnet->LayoutTrees();
		// 路网的参考系是整张地图：ratio相对地图宽高，不旋转，局部坐标就是世界坐标。
		TreeFrame frame;
		frame.toLocal = [this](const PointParams& p) {
			return std::make_pair(p[0] * width + p[1], p[2] * height + p[3]);
			};
		frame.localToWorld = [](float x, float y) { return std::make_pair(x, y); };
		roadTrees = vegetation.ExpandRequests(roadnet->GetTrees(), frame, canPlace);
	}

	int buildingTrees = 0;
	for (auto& [name, building] : buildings) {
		BuildingMod* mod = building ? building->GetMod() : nullptr;
		if (!mod) continue;
		mod->LayoutTrees(*building, building->GetBoundaryRoads());
		Building* host = building;
		TreeFrame frame;
		frame.toLocal = [host](const PointParams& p) { return host->QuadNorthPointToLocal(p); };
		frame.localToWorld = [host](float x, float y) { return host->QuadLocalToWorld(x, y); };
		frame.rotation = building->GetRotation();
		buildingTrees += vegetation.ExpandRequests(mod->trees, frame, canPlace);
	}

	int zoneTrees = 0;
	for (auto& [name, zone] : zones) {
		ZoneMod* mod = zone ? zone->GetMod() : nullptr;
		if (!mod) continue;
		mod->LayoutTrees(zone->GetDirection(), *zone, zone->GetBoundaryRoads());
		Zone* host = zone;
		TreeFrame frame;
		frame.toLocal = [host](const PointParams& p) { return host->NorthPointToLocal(p); };
		frame.localToWorld = [host](float x, float y) { return host->LocalToWorld(x, y); };
		frame.rotation = zone->GetRotation();
		zoneTrees += vegetation.ExpandRequests(mod->trees, frame, canPlace);
	}

	debugf("Log: Vegetation road trees=%d, building trees=%d, zone trees=%d, total=%d, meshes=%d.\n", roadTrees, buildingTrees, zoneTrees,
		(int)vegetation.GetTrees().size(), (int)vegetation.GetMeshPaths().size());
}

const Vegetation& Map::GetVegetation() const {
	return vegetation;
}

void Map::InitLamps() {
	int roadLamps = 0;
	if (roadnet) {
		roadnet->LayoutLamps();
		// 路网的参考系是整张地图：同InitVegetation，ratio相对地图宽高，不旋转。
		TreeFrame frame;
		frame.toLocal = [this](const PointParams& p) {
			return std::make_pair(p[0] * width + p[1], p[2] * height + p[3]);
			};
		frame.localToWorld = [](float x, float y) { return std::make_pair(x, y); };
		roadLamps = lamps.ExpandRequests(roadnet->GetLamps(), frame, [this](float x, float y) { return CanPlaceTree(x, y); });
	}
	debugf("Log: Lamps road lamps=%d, total=%d, blueprints=%d.\n", roadLamps,
		(int)lamps.GetLamps().size(), (int)lamps.GetBlueprintPaths().size());
}

const Lamps& Map::GetLamps() const {
	return lamps;
}

bool Map::CanPlaceTree(float x, float y) const {
	int ex = (int)floorf(x), ey = (int)floorf(y);
	if (!CheckXY(ex, ey)) return false;
	const Element& element = At(ex, ey);
	return !element.water.first && element.hatches.empty();
}

void Map::FlushPendingBuildingRoadAccess() {
	struct Entry {
		bool isVehicle;
		int side;
		int laneIndex;
		float t;
		Node* outsideNode;
		bool isExit; // 仅isVehicle==true时有意义
		bool useForwardSide; // 原请求的朝向，原样传给AddRoadAccessNode(单行道回退到对侧时不能由解析后的side反推)
	};
	unordered_map<Road*, vector<Entry>> grouped;
	for (const PendingRoadAccess& pending : pendingBuildingRoadAccess) {
		int side, laneIndex;
		if (!ResolveAccessLane(pending.road, pending.isVehicle, pending.useForwardSide, side, laneIndex)) continue;
		grouped[pending.road].push_back({ pending.isVehicle, side, laneIndex, pending.t, pending.outsideNode, pending.isExit, pending.useForwardSide });
	}
	pendingBuildingRoadAccess.clear();

	constexpr float kAccessOpeningWidth = 1.f; // 门洞大致宽度(地图单位)，供Forever层挖空，固定值不按具体门精细对应
	for (auto& [road, entries] : grouped) {
		// 同一条车道内按通行方向排序(side0 t升序，side1 t降序)，匹配BreakThroughLine"fromAnchor往通行方向前进"的约定；
		// 前三个键只是让车行/人行、各side、各车道的entry分组。
		sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
			if (a.isVehicle != b.isVehicle) return a.isVehicle < b.isVehicle;
			if (a.side != b.side) return a.side < b.side;
			if (a.laneIndex != b.laneIndex) return a.laneIndex < b.laneIndex;
			return a.side == 0 ? (a.t < b.t) : (a.t > b.t);
			});

		for (Entry& entry : entries) {
			Node* accessNode = AddRoadAccessNode(road, entry.t, entry.isVehicle,
				entry.useForwardSide, kAccessOpeningWidth);
			if (!accessNode) continue; // 这条Road两侧都没有对应类别车道，没法接，outsideNode保持孤立。

			if (!entry.isVehicle) {
				Connection* conn = new Connection(*entry.outsideNode, *accessNode);
				pedestrianNavGraph[entry.outsideNode->GetId()].emplace_back(accessNode->GetId(), conn);
				pedestrianNavGraph[accessNode->GetId()].emplace_back(entry.outsideNode->GetId(), conn);
				continue;
			}

			// 车行单向：isExit(入度为0)方向"outsideNode->accessNode"
			// (车辆从building开到路上)；否则(出度为0，入口)方向"accessNode->outsideNode"
			// (车辆从路开进building)。
			Connection* conn = entry.isExit
				? new Connection(*entry.outsideNode, *accessNode)
				: new Connection(*accessNode, *entry.outsideNode);
			vehicleNavGraph[conn->GetStart().GetId()].emplace_back(conn->GetEnd().GetId(), conn);
		}
	}
}

void Map::FlushPendingPathRoadLinks() {
	struct Touch {
		Road* road;
		int side;
		float t;
		size_t linkIndex;
	};
	vector<Touch> touches;
	for (size_t i = 0; i < pendingPathRoadLinks.size(); i++) {
		const PathRoadLink& link = pendingPathRoadLinks[i];
		if (link.endRoad1) {
			touches.push_back({ link.endRoad1, ResolveNearSide(link.road, true, link.endRoad1, link.endT1), link.endT1, i });
		}
		if (link.endRoad2) {
			touches.push_back({ link.endRoad2, ResolveNearSide(link.road, false, link.endRoad2, link.endT2), link.endT2, i });
		}
	}

	// 按(road,side)分组，组内按这条贯通线自己的通行方向排序：side0沿Start->End(t升序)，
	// side1沿End->Start(t降序)——和FlushPendingBuildingRoadAccess同一个规则。
	map<pair<Road*, int>, vector<Touch>> groups;
	for (const Touch& touch : touches) groups[{touch.road, touch.side}].push_back(touch);
	for (auto& [key, group] : groups) {
		int side = key.second;
		sort(group.begin(), group.end(), [side](const Touch& a, const Touch& b) {
			return side == 0 ? a.t < b.t : a.t > b.t;
			});
	}

	// 一条link占两个touch(可能落在不同host road上)，两端必须在同一次ConnectPathRoad里处理，不能把touch当独立工作项排序，
	// 所以用Kahn拓扑排序：每组内相邻touch形成"前者所属link先处理"的依赖边。出现环(只有很反常的路网几何)时剩余部分按原始顺序追加，不阻塞。
	size_t n = pendingPathRoadLinks.size();
	vector<vector<size_t>> adj(n);
	vector<int> indegree(n, 0);
	for (auto& [key, group] : groups) {
		for (size_t i = 1; i < group.size(); i++) {
			size_t from = group[i - 1].linkIndex;
			size_t to = group[i].linkIndex;
			if (from == to) continue;
			adj[from].push_back(to);
			indegree[to]++;
		}
	}

	// 小路接在另一条小路上时，被接的小路自己的贯通线（throughLines）要先建好，否则BreakThroughLine找不到线段返回nullptr，
	// 这一端退化成孤立锚点、整条小路及其上的建筑从行人图里脱开。所以host小路的link必须先于挂在它上面的link。
	unordered_map<Road*, size_t> linkOfRoad;
	for (size_t i = 0; i < n; i++) linkOfRoad[pendingPathRoadLinks[i].road] = i;
	for (size_t i = 0; i < n; i++) {
		const PathRoadLink& link = pendingPathRoadLinks[i];
		for (Road* host : { link.endRoad1, link.endRoad2 }) {
			if (!host || !host->IsPathRoad()) continue;
			auto it = linkOfRoad.find(host);
			if (it == linkOfRoad.end() || it->second == i) continue;
			adj[it->second].push_back(i);
			indegree[i]++;
		}
	}

	vector<bool> done(n, false);
	queue<size_t> ready;
	for (size_t i = 0; i < n; i++) if (indegree[i] == 0) ready.push(i);
	vector<size_t> order;
	while (!ready.empty()) {
		size_t cur = ready.front();
		ready.pop();
		if (done[cur]) continue;
		done[cur] = true;
		order.push_back(cur);
		for (size_t next : adj[cur]) {
			if (--indegree[next] == 0) ready.push(next);
		}
	}
	for (size_t i = 0; i < n; i++) if (!done[i]) order.push_back(i); // 环兜底：按原始下标追加

	for (size_t idx : order) {
		ConnectPathRoad(pendingPathRoadLinks[idx]);
	}
	pendingPathRoadLinks.clear();
}

const unordered_map<string, Zone*>& Map::GetZones() const {
	return zones;
}

const unordered_map<string, Building*>& Map::GetBuildings() const {
	return buildings;
}

int Map::ComputeAccommodationTarget() const {
	int capacity = 0;
	int residentialRooms = 0;
	for (auto& [name, building] : buildings) {
		if (!building) continue;
		for (Room* room : building->GetRooms()) {
			if (room && room->IsResidential()) {
				capacity += room->ResidentialCapacity();
				residentialRooms++;
			}
		}
	}
	// 累计容量不到住宅房间数的两倍(房间普遍很小)时，按"每间住宅房间一个人"算，保证每间
	// 房至少有人住；否则维持累计容量的一半。
	if (capacity < residentialRooms * 2) return residentialRooms;
	return capacity / 2;
}

int Map::ComputeWorkCapacity() const {
	int capacity = 0;
	for (auto& [name, building] : buildings) {
		if (!building) continue;
		for (Room* room : building->GetRooms()) {
			if (room && room->IsWorkspace()) capacity += room->WorkspaceCapacity() * 2; // 容量=满编，最多可超编到2倍
		}
	}
	return capacity;
}

void Map::SettleCitizen(Citizen* citizen, Room* room) {
	if (!citizen || !room) return;
	Building* building = room->GetParentBuilding();
	citizen->SetRoom(room);
	citizen->SetCurrentRoom(room);
	citizen->SetBuilding(building);
	if (building) {
		citizen->SetZone(building->GetParentZone());
		citizen->SetLot(building->GetParentLot());
	}
	room->AddResident(citizen);
}

void Map::Checkin(const Populace& populace) {
	constexpr int kAdultAge = 18;
	constexpr int kProbabilityScale = 100;
	constexpr int kZoneOwnershipChance = 2; // zone整体归一个随机成年人私有的百分比概率
	constexpr int kBuildingOwnershipChance = 5; // building整体归一个随机成年人私有的百分比概率
	// 公有(政府拥有)的概率写在各自的mod里(ZoneMod/BuildingMod::stateOwnedChance，小数[0,1])，
	// 这里换算成百分数和上面的ownership chance在同一个刻度上比较。
	auto statedPercent = [](float chance) { return static_cast<int>(lround(chance * 100.f)); };

	int currentYear = populace.GetCurrentYear();

	vector<Citizen*> adults;
	for (Citizen* citizen : populace.GetCitizens()) {
		if (citizen && citizen->GetAge(currentYear) >= kAdultAge) {
			adults.push_back(citizen);
		}
	}
	if (adults.empty()) return;

	// 房产归属：Zone->Building->Room逐级下探，每级三选一(整体公有/整体私有/下探各自决定)，详见map.md"房产归属与住处分配"。
	auto resolveBuildingOwnership = [&adults, kProbabilityScale, kBuildingOwnershipChance,
		statedPercent](Building* building) {
		int kBuildingStatedChance = building->GetMod() ? statedPercent(building->GetMod()->stateOwnedChance) : 0;
		int roll = GetRandom(kProbabilityScale);
		if (roll < kBuildingStatedChance) {
			building->SetStated(true);
			for (Room* room : building->GetRooms()) {
				if (room) room->SetStated(true);
			}
		}
		else if (roll < kBuildingStatedChance + kBuildingOwnershipChance) {
			Citizen* buildingOwner = adults[GetRandom(static_cast<int>(adults.size()))];
			building->SetOwner(buildingOwner);
			for (Room* room : building->GetRooms()) {
				if (room) room->SetOwner(buildingOwner);
			}
		}
		else {
			// 整栋building没有统一归属，每个room各自独立随机分配——building/zone的
			// owner/stated保持默认值(nullptr/false)，不需要显式重置，见building.h/
			// zone.h的owner/stated设计说明。
			for (Room* room : building->GetRooms()) {
				if (room) room->SetOwner(adults[GetRandom(static_cast<int>(adults.size()))]);
			}
		}
	};

	for (auto& [zoneName, zone] : zones) {
		if (!zone) continue;
		int kZoneStatedChance = zone->GetMod() ? statedPercent(zone->GetMod()->stateOwnedChance) : 0;
		int roll = GetRandom(kProbabilityScale);
		if (roll < kZoneStatedChance) {
			zone->SetStated(true);
			for (Building* building : zone->GetInternalBuildings()) {
				if (!building) continue;
				building->SetStated(true);
				for (Room* room : building->GetRooms()) {
					if (room) room->SetStated(true);
				}
			}
		}
		else if (roll < kZoneStatedChance + kZoneOwnershipChance) {
			Citizen* zoneOwner = adults[GetRandom(static_cast<int>(adults.size()))];
			zone->SetOwner(zoneOwner);
			for (Building* building : zone->GetInternalBuildings()) {
				if (!building) continue;
				building->SetOwner(zoneOwner);
				for (Room* room : building->GetRooms()) {
					if (room) room->SetOwner(zoneOwner);
				}
			}
		}
		else {
			for (Building* building : zone->GetInternalBuildings()) {
				if (building) resolveBuildingOwnership(building);
			}
		}
	}

	// 不属于任何zone的独立building各走一遍building级归属；zone内的building已在上面处理过，不能再骰一次覆盖zone级归属。
	for (auto& [buildingName, building] : buildings) {
		if (!building || building->GetParentZone()) continue;
		resolveBuildingOwnership(building);
	}

	// 分配住处：和"房产归属"是两个独立的关注点——一个room的owner是谁、和谁实际住在
	// 里面(households/occupants)完全无关(可以理解成"租房")，所以这里单独重新扫一遍所有
	// 住宅room建名额池，不复用上面归属循环的中间状态。
	vector<Room*> pool;
	for (auto& [name, building] : buildings) {
		if (!building) continue;
		for (Room* room : building->GetRooms()) {
			if (room && room->IsResidential()) {
				pool.push_back(room);
			}
		}
	}

	// 人和房间的关系是3个独立概念(见room.h)：这里的"家"(GetRoom()/households)和"当前物理
	// 位置"(GetCurrentRoom()/occupants)对citizen来说初始状态天然重合(刚分配住处，人也
	// 就在那)，但存储上必须分开写，为将来"人在家但当前不在自己房间里"这类场景预留。
	auto moveIn = [this](Citizen* citizen, Room* room) { SettleCitizen(citizen, room); };

	// 只有成年citizen各自随机抽一个room名额(swap-remove)，一个家庭消费一个名额；配偶以90%概率(GetRandom(10)>0)跟着搬进同一间，
	// 未成年子女无条件一起搬。未成年citizen不独立抽房间。
	for (Citizen* citizen : adults) {
		if (citizen->GetRoom()) continue; // 已经被配偶那边带着分到房间了
		if (pool.empty()) break;

		int index = GetRandom(static_cast<int>(pool.size()));
		Room* room = pool[index];
		moveIn(citizen, room);

		Citizen* spouse = citizen->GetSpouse();
		if (spouse && !spouse->GetRoom() && GetRandom(10) > 0) {
			moveIn(spouse, room);
		}

			// 子女搬进同一间不依赖配偶是否同住，否则单亲家庭(或配偶没通过90%判定)的孩子会永远分不到房间。
		for (Citizen* child : citizen->GetChildren()) {
			if (child && !child->GetRoom() && child->GetAge(currentYear) < kAdultAge) {
				moveIn(child, room);
			}
		}

		pool[index] = pool.back();
		pool.pop_back();
	}
}

bool Map::AddZone(Zone* zone) {
	if (!zone) return false;
	if (zones.find(zone->GetName()) != zones.end()) {
		debugf("Warning: Duplicate zone name \"%s\", rejected.\n", zone->GetName().data());
		return false;
	}
	zones[zone->GetName()] = zone;
	return true;
}

bool Map::AddBuilding(Building* building) {
	if (!building) return false;
	if (buildings.find(building->GetName()) != buildings.end()) {
		debugf("Warning: Duplicate building name \"%s\", rejected.\n", building->GetName().data());
		return false;
	}
	buildings[building->GetName()] = building;
	return true;
}

Zone* Map::GetZone(const string& name) const {
	auto it = zones.find(name);
	return it != zones.end() ? it->second : nullptr;
}

Building* Map::GetBuilding(const string& name) const {
	auto it = buildings.find(name);
	return it != buildings.end() ? it->second : nullptr;
}

Zone* Map::LocateZone(const string& address) const {
	istringstream iss(address);
	string road;
	int index;
	string zoneName;
	if (!(iss >> road >> index >> zoneName)) return nullptr;
	Lot* lot = LocateLot(road, index);
	if (!lot) return nullptr;
	for (auto& [name, zone] : zones) {
		if (zone && zone->GetParentLot() == lot && name == zoneName) return zone;
	}
	return nullptr;
}

namespace {
	// 两个Lot是不是同一块地：指针相同，或者地址相同——自适应占位的子块沿用了顶层Lot的地址
	// (见InitBuildings)，building的parentLot是子块，LocateLot(road,index)返回的是顶层Lot。
	bool SameBlock(const Lot* a, const Lot* b) {
		if (a == b) return true;
		if (!a || !b) return false;
		const auto& x = a->GetAddresses();
		const auto& y = b->GetAddresses();
		return !x.empty() && !y.empty() && x.front() == y.front();
	}
}

Building* Map::LocateBuilding(const string& address) const {
	istringstream iss(address);
	string road;
	int index;
	if (!(iss >> road >> index)) return nullptr;
	vector<string> rest;
	string token;
	while (iss >> token) rest.push_back(token);
	if (rest.empty()) return nullptr;
	Lot* lot = LocateLot(road, index);
	if (!lot) return nullptr;

	if (rest.size() == 1) {
		// "<road> <index> <buildingName>"：直接落在这个lot上、没有parentZone的building
		const string& buildingName = rest[0];
		for (auto& [name, building] : buildings) {
			if (building && SameBlock(building->GetParentLot(), lot) && !building->GetParentZone()
				&& name == buildingName) {
				return building;
			}
		}
		return nullptr;
	}
	// "<road> <index> <zoneName> <buildingName>"：先定位zone，再在这个zone里找building
	Zone* zone = nullptr;
	for (auto& [name, z] : zones) {
		if (z && z->GetParentLot() == lot && name == rest[0]) {
			zone = z;
			break;
		}
	}
	if (!zone) return nullptr;
	const string& buildingName = rest[1];
	for (Building* building : zone->GetInternalBuildings()) {
		if (building && building->GetName() == buildingName) return building;
	}
	return nullptr;
}

Room* Map::LocateRoom(const string& address) const {
	istringstream iss(address);
	vector<string> tokens;
	string token;
	while (iss >> token) tokens.push_back(token);
	if (tokens.size() < 3) return nullptr; // 最短"<road> <index> <number>"都凑不齐

	const string& number = tokens.back();
	ostringstream buildingAddress;
	for (size_t i = 0; i + 1 < tokens.size(); i++) {
		if (i > 0) buildingAddress << ' ';
		buildingAddress << tokens[i];
	}
	Building* building = LocateBuilding(buildingAddress.str());
	if (!building) return nullptr;

	for (Room* room : building->GetRooms()) {
		if (room && room->GetNumber() == number) return room;
	}
	return nullptr;
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

bool Map::ResolveAccessLane(Road* road, bool isVehicle, bool useForwardSide, int& outSide, int& outLaneIndex) {
	if (!road) return false;

	int requestedSide = useForwardSide ? 0 : 1;
	const vector<float>& requestedLanes = isVehicle ? road->GetVehicleLanes(requestedSide) : road->GetPedestrianLanes(requestedSide);
	const vector<float>& oppositeLanes = isVehicle ? road->GetVehicleLanes(1 - requestedSide) : road->GetPedestrianLanes(1 - requestedSide);

		// 单行道：请求的side没有对应类别车道但对侧有时，useForwardSide改按"要最靠右(true)/最靠左(false)的车道"重新解释，
		// 实际车道从对侧取，见map.md"导航图与贯通线"。两侧都没有车道返回false。
	int side;
	if (!requestedLanes.empty()) {
		side = requestedSide;
	}
	else if (!oppositeLanes.empty()) {
		side = 1 - requestedSide;
	}
	else {
		return false;
	}
	const vector<float>& lanes = isVehicle ? road->GetVehicleLanes(side) : road->GetPedestrianLanes(side);
	const vector<float>& otherSideLanes = isVehicle ? road->GetVehicleLanes(1 - side) : road->GetPedestrianLanes(1 - side);
	bool oneWay = otherSideLanes.empty();

	float sideSign = (side == 0) ? 1.f : -1.f;
	float shift = (road->GetSideWidth(0) - road->GetSideWidth(1)) * 0.5f;

	// 车道选择：双向路固定最外侧车道(新访问点代表外侧车道)；单行路按useForwardSide选居中后(见roadnet.md"车道居中")
	// 有符号偏移最大(true，最靠右)/最小(false，最靠左)的车道。
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

	outSide = side;
	outLaneIndex = laneIndex;
	return true;
}

Node* Map::AddRoadAccessNode(Road* road, float t, bool isVehicle, bool useForwardSide, float openingWidth) {
	if (!road) return nullptr;

	int side, laneIndex;
	if (!ResolveAccessLane(road, isVehicle, useForwardSide, side, laneIndex)) return nullptr;

	// 世界坐标必须用ComputeLaneAnchorPosition算：人行道在同侧车行+停车道外侧，不能直接从中轴线量。
	auto [nx, ny] = ComputeLaneAnchorPosition(road, t, isVehicle, side, laneIndex);
	Node basePoint = road->GetPoint(t);

	Node* newNode = BreakThroughLine(road, isVehicle, side, laneIndex, nx, ny, basePoint.GetZ());
	if (!newNode) return nullptr;

	RoadOpening opening;
	opening.t = t;
	opening.width = openingWidth;
	opening.forwardSide = useForwardSide;
	opening.isVehicle = isVehicle;
	road->AddOpening(opening);

	return newNode;
}

Node* Map::BreakThroughLine(Road* road, bool isVehicle, int side, int laneIndex, float worldX, float worldY, float worldZ) {
	if (!road || laneIndex < 0) return nullptr;

	// kThroughLineEps是t比较的浮点容差；kThroughLineReuseDistSq：断点离既有端点不到约10cm时直接复用该node，不新建重合的node。
	constexpr float kThroughLineEps = 1e-4f;
	constexpr float kThroughLineReuseDistSq = 0.0001f;

	float breakT = ProjectPointOntoRoad(road, worldX, worldY);
	int idx = (isVehicle ? 0 : 2) + side;
	vector<ThroughLine>& lines = throughLines[road][idx];

	// 找laneIndex匹配且[tLo,tHi]跨过breakT的那一段，只认t范围，与调用顺序无关，见map.md"导航图与贯通线"。
	int foundIdx = -1;
	for (int i = 0; i < static_cast<int>(lines.size()); i++) {
		if (lines[i].laneIndex != laneIndex) continue;
		if (breakT >= lines[i].tLo - kThroughLineEps && breakT <= lines[i].tHi + kThroughLineEps) {
			foundIdx = i;
			break;
		}
	}
	if (foundIdx < 0) return nullptr; // laneIndex存在的话理论上应该总有一段覆盖[0,1]

	ThroughLine seg = lines[foundIdx];
	Node* nodeAtLo = (side == 0) ? seg.fromAnchor : seg.toAnchor;
	Node* nodeAtHi = (side == 0) ? seg.toAnchor : seg.fromAnchor;

	auto closeEnough = [&](Node* n) {
		float dx = n->GetX() - worldX, dy = n->GetY() - worldY;
		return dx * dx + dy * dy < kThroughLineReuseDistSq;
		};
	if (closeEnough(nodeAtLo)) return nodeAtLo;
	if (closeEnough(nodeAtHi)) return nodeAtHi;

	// 真正拆分：删掉这一段的旧edge（连图上的边一起删），从vector里移除这一段。
	lines.erase(lines.begin() + foundIdx);
	auto& graph = isVehicle ? vehicleNavGraph : pedestrianNavGraph;
	if (seg.edge) {
		RemoveGraphEdgeOneWay(graph, seg.fromAnchor->GetId(), seg.toAnchor->GetId());
		if (!isVehicle) {
			RemoveGraphEdgeOneWay(graph, seg.toAnchor->GetId(), seg.fromAnchor->GetId());
		}
		delete seg.edge;
	}

	Node* newNode = new Node(isVehicle ? "vehicle" : "pedestrian", worldX, worldY, worldZ);
	navAnchorNodes.push_back(newNode);

	// 按tLo->tHi方向建两条新Connection（side==0方向沿t递增，fromAnchor在tLo端；side==1
	// 反过来，fromAnchor在tHi端），同时登记进lines，构成这条车道链上更细的两段。
	auto makeSeg = [&](Node* from, Node* to, float tA, float tB) {
		Connection* c = new Connection(*from, *to);
		graph[from->GetId()].emplace_back(to->GetId(), c);
		if (!isVehicle) graph[to->GetId()].emplace_back(from->GetId(), c);
		lines.push_back({ c, from, to, laneIndex, tA, tB });
		};
	if (side == 0) {
		makeSeg(nodeAtLo, newNode, seg.tLo, breakT);
		makeSeg(newNode, nodeAtHi, breakT, seg.tHi);
	}
	else {
		makeSeg(nodeAtHi, newNode, breakT, seg.tHi);
		makeSeg(newNode, nodeAtLo, seg.tLo, breakT);
	}

	return newNode;
}

Node* Map::MakeIsolatedAnchor(float worldX, float worldY, float worldZ, const char* category) {
	Node* node = new Node(category, worldX, worldY, worldZ);
	navAnchorNodes.push_back(node);
	return node;
}

pair<float, float> Map::ComputeLaneAnchorPosition(Road* road, float t, bool isVehicle, int side, int laneIndex) const {
	Node basePoint = road->GetPoint(t);
	float tdx, tdy, tdz;
	road->GetTangent(t, tdx, tdy, tdz);
	float tlen = sqrtf(tdx * tdx + tdy * tdy);
	if (tlen < 1e-6f) tlen = 1.f;
	// 本项目世界坐标+Y=南，右手边公式是(-tdy,tdx)，不是+Y=北时的(tdy,-tdx)(朝南(0,1)时右手边是西(-1,0))。
	// map.cpp/roadnet.cpp里所有用到perp0的地方必须保持一致，否则路口/zone出入口/小路旁路和车道锚点会对不上。
	float perp0X = -tdy / tlen, perp0Y = tdx / tlen;

	float shift = (road->GetSideWidth(0) - road->GetSideWidth(1)) * 0.5f;
	float sideSign = (side == 0) ? 1.f : -1.f;

	float offsetDist;
	if (isVehicle) {
		offsetDist = LaneCenterOffset(road->GetVehicleLanes(side), laneIndex);
	}
	else {
		// 人行道在同侧车行+停车道外侧，基准要先加上那两类车道的总宽度，不是直接从中轴线量
		// （和RoadJunction::Build里pedestrianSide锚点的算法一致）。
		offsetDist = SumWidths(road->GetVehicleLanes(side)) + SumWidths(road->GetParkingLanes(side))
			+ LaneCenterOffset(road->GetPedestrianLanes(side), laneIndex);
	}
	float signedOffset = offsetDist * sideSign - shift;

	return { basePoint.GetX() + perp0X * signedOffset, basePoint.GetY() + perp0Y * signedOffset };
}

void Map::ResolvePathEndAnchors(Road* path, bool isStartEnd, Road* hostRoad, float hostT,
	array<Node*, 2>& outVeh, array<Node*, 2>& outPed) {

	Node baseNode = isStartEnd ? path->GetStart() : path->GetEnd();
	Node otherNode = isStartEnd ? path->GetEnd() : path->GetStart();

	float dirX = otherNode.GetX() - baseNode.GetX();
	float dirY = otherNode.GetY() - baseNode.GetY();
	float dirLen = sqrtf(dirX * dirX + dirY * dirY);
	if (dirLen < 1e-6f) dirLen = 1.f;
	dirX /= dirLen;
	dirY /= dirLen;

	// 小路自己的横断面：车行道中心在±vehOffset，人行道中心在±pedOffset(车行道外侧)——两侧
	// 宽度对称(PathLaneSpec保证)，取side0的值就行，side0为空时(理论上不会)退化用side1。
	const vector<float>& pathVeh0 = path->GetVehicleLanes(0);
	const vector<float>& pathVeh1 = path->GetVehicleLanes(1);
	const vector<float>& pathPed0 = path->GetPedestrianLanes(0);
	const vector<float>& pathPed1 = path->GetPedestrianLanes(1);
	float vehWidth = !pathVeh0.empty() ? pathVeh0[0] : (!pathVeh1.empty() ? pathVeh1[0] : 0.f);
	float pedWidth = !pathPed0.empty() ? pathPed0[0] : (!pathPed1.empty() ? pathPed1[0] : 0.f);
	float vehOffset = vehWidth * 0.5f;
	float pedOffset = vehWidth + pedWidth * 0.5f;

	// 小路自身固定的perp0：必须用Road自己Start->End方向，不能用dirX/dirY(它在Start/End端相反，side0/side1会在两端对应物理上相反的
	// 两侧，连线交叉)。side0/side1的物理含义要和SplitWithPath铺的车道数据一致。
	Node pathStart = path->GetStart();
	Node pathEnd = path->GetEnd();
	float gdx = pathEnd.GetX() - pathStart.GetX();
	float gdy = pathEnd.GetY() - pathStart.GetY();
	float glen = sqrtf(gdx * gdx + gdy * gdy);
	if (glen < 1e-6f) glen = 1.f;
	// 右手边修正，见Map::ComputeLaneAnchorPosition实现处的说明：+Y=南，公式是(-gdy,gdx)。
	float pathPerp0X = -gdy / glen, pathPerp0Y = gdx / glen;

	// 小路side0沿Start->End走，side1沿End->Start走：Start端离开(entering)的是side0、
	// 到达(exiting)的是side1；End端相反。
	int enteringSide = isStartEnd ? 0 : 1;
	int exitingSide = isStartEnd ? 1 : 0;

	// host分支里同一断点处断两次得到的Nin/Nout(及PnA/PnB等)若坐标相同，小路自己的side0/side1贯通线会重合成一条线。
	// 所以沿小路自身pathPerp0方向按车道/人行道半宽各偏移一点，让两个断点落在小路两条车道各自的延长线上；
	// enterSign控制entering侧的符号，exiting取反。
	float enterSign = (enteringSide == 0) ? 1.f : -1.f;

	if (!hostRoad) {
		float isolatedZ = baseNode.GetZ();
		outVeh[0] = MakeIsolatedAnchor(baseNode.GetX() + pathPerp0X * vehOffset, baseNode.GetY() + pathPerp0Y * vehOffset, isolatedZ, "vehicle");
		outVeh[1] = MakeIsolatedAnchor(baseNode.GetX() - pathPerp0X * vehOffset, baseNode.GetY() - pathPerp0Y * vehOffset, isolatedZ, "vehicle");
		outPed[0] = MakeIsolatedAnchor(baseNode.GetX() + pathPerp0X * pedOffset, baseNode.GetY() + pathPerp0Y * pedOffset, isolatedZ, "pedestrian");
		outPed[1] = MakeIsolatedAnchor(baseNode.GetX() - pathPerp0X * pedOffset, baseNode.GetY() - pathPerp0Y * pedOffset, isolatedZ, "pedestrian");
		return;
	}

	Node hostPoint = hostRoad->GetPoint(hostT);
	float hostZ = hostPoint.GetZ();
	float hdx, hdy, hdz;
	hostRoad->GetTangent(hostT, hdx, hdy, hdz);
	float hlen = sqrtf(hdx * hdx + hdy * hdy);
	if (hlen < 1e-6f) hlen = 1.f;
	float hostFwdX = hdx / hlen, hostFwdY = hdy / hlen;
	// 右手边修正，见Map::ComputeLaneAnchorPosition实现处的说明：+Y=南，公式是(-hostFwdY,hostFwdX)。
	float hostPerp0X = -hostFwdY, hostPerp0Y = hostFwdX;

	// 近侧判定：小路离开连接点、伸向自己另一端的方向，和host的perp0点积>=0就是host的side0，
	// 否则side1——小路总是往它所属Lot的空闲空间那一侧延伸，这个方向天然指向近侧所在的半边。
	float dot = dirX * hostPerp0X + dirY * hostPerp0Y;
	int nearSide = (dot >= 0.f) ? 0 : 1;
	int farSide = 1 - nearSide;

	auto pickLaneIndex = [](const vector<float>& lanes) -> int {
		return lanes.empty() ? -1 : static_cast<int>(lanes.size()) - 1;
		};
	// 近侧没有车道(单行道只在farSide铺车道)而退化用farSide时，要选紧贴中心线的下标0，不是outermost——此时小路紧贴中心线这一侧；
	// 若仍用outermost，所有退化到同一farSide的小路会挤到同一条车道上。
	auto pickFallbackLaneIndex = [](const vector<float>& lanes) -> int {
		return lanes.empty() ? -1 : 0;
		};

	if (!hostRoad->IsPathRoad()) {
		// 大路：只处理近侧，某一类车道近侧没有就退化用远侧(和AddRoadAccessNode单行道回退
		// 逻辑同样的精神——两侧都没有就彻底放弃、退化成孤立锚点)。
		int vehSide = nearSide;
		int vehLaneIndex = pickLaneIndex(hostRoad->GetVehicleLanes(vehSide));
		if (vehLaneIndex < 0) {
			vehSide = farSide;
			vehLaneIndex = pickFallbackLaneIndex(hostRoad->GetVehicleLanes(vehSide));
		}
		Node* Nin = nullptr;
		Node* Nout = nullptr;
		if (vehLaneIndex >= 0) {
			auto [vx, vy] = ComputeLaneAnchorPosition(hostRoad, hostT, true, vehSide, vehLaneIndex);
			Nin = BreakThroughLine(hostRoad, true, vehSide, vehLaneIndex,
				vx + pathPerp0X * vehOffset * enterSign, vy + pathPerp0Y * vehOffset * enterSign, hostZ);
			Nout = BreakThroughLine(hostRoad, true, vehSide, vehLaneIndex,
				vx - pathPerp0X * vehOffset * enterSign, vy - pathPerp0Y * vehOffset * enterSign, hostZ);
		}
		if (Nin && Nout) {
			outVeh[enteringSide] = Nin;
			outVeh[exitingSide] = Nout;
		}
		else {
			outVeh[0] = MakeIsolatedAnchor(hostPoint.GetX() + pathPerp0X * vehOffset, hostPoint.GetY() + pathPerp0Y * vehOffset, hostZ, "vehicle");
			outVeh[1] = MakeIsolatedAnchor(hostPoint.GetX() - pathPerp0X * vehOffset, hostPoint.GetY() - pathPerp0Y * vehOffset, hostZ, "vehicle");
		}

		int pedSide = nearSide;
		int pedLaneIndex = pickLaneIndex(hostRoad->GetPedestrianLanes(pedSide));
		if (pedLaneIndex < 0) {
			pedSide = farSide;
			pedLaneIndex = pickFallbackLaneIndex(hostRoad->GetPedestrianLanes(pedSide));
		}
		Node* Pa = nullptr;
		Node* Pb = nullptr;
		if (pedLaneIndex >= 0) {
			auto [px, py] = ComputeLaneAnchorPosition(hostRoad, hostT, false, pedSide, pedLaneIndex);
			Pa = BreakThroughLine(hostRoad, false, pedSide, pedLaneIndex,
				px + pathPerp0X * pedOffset, py + pathPerp0Y * pedOffset, hostZ);
			Pb = BreakThroughLine(hostRoad, false, pedSide, pedLaneIndex,
				px - pathPerp0X * pedOffset, py - pathPerp0Y * pedOffset, hostZ);
		}
		if (Pa && Pb) {
			outPed[0] = Pa;
			outPed[1] = Pb;
		}
		else {
			outPed[0] = MakeIsolatedAnchor(hostPoint.GetX() + pathPerp0X * pedOffset, hostPoint.GetY() + pathPerp0Y * pedOffset, hostZ, "pedestrian");
			outPed[1] = MakeIsolatedAnchor(hostPoint.GetX() - pathPerp0X * pedOffset, hostPoint.GetY() - pathPerp0Y * pedOffset, hostZ, "pedestrian");
		}
	}
	else {
		// 小路接小路：近侧+远侧车行道/人行道各断2个node，远侧单向搭桥到近侧对应node，近侧
		// 的node才是实际接新小路的锚点。host本身也是小路，两侧车道/人行道都由PathLaneSpec
		// 保证非空，不需要像大路那样处理"某侧没有车道"的退化分支。
		int nearVehLaneIndex = pickLaneIndex(hostRoad->GetVehicleLanes(nearSide));
		int farVehLaneIndex = pickLaneIndex(hostRoad->GetVehicleLanes(farSide));

		Node* NnIn = nullptr;
		Node* NnOut = nullptr;
		if (nearVehLaneIndex >= 0) {
			auto [nvx, nvy] = ComputeLaneAnchorPosition(hostRoad, hostT, true, nearSide, nearVehLaneIndex);
			NnIn = BreakThroughLine(hostRoad, true, nearSide, nearVehLaneIndex,
				nvx + pathPerp0X * vehOffset * enterSign, nvy + pathPerp0Y * vehOffset * enterSign, hostZ);
			NnOut = BreakThroughLine(hostRoad, true, nearSide, nearVehLaneIndex,
				nvx - pathPerp0X * vehOffset * enterSign, nvy - pathPerp0Y * vehOffset * enterSign, hostZ);
		}
		if (farVehLaneIndex >= 0) {
			auto [fvx, fvy] = ComputeLaneAnchorPosition(hostRoad, hostT, true, farSide, farVehLaneIndex);
			Node* NfIn = BreakThroughLine(hostRoad, true, farSide, farVehLaneIndex,
				fvx + pathPerp0X * vehOffset * enterSign, fvy + pathPerp0Y * vehOffset * enterSign, hostZ);
			Node* NfOut = BreakThroughLine(hostRoad, true, farSide, farVehLaneIndex,
				fvx - pathPerp0X * vehOffset * enterSign, fvy - pathPerp0Y * vehOffset * enterSign, hostZ);
			if (NfIn && NnIn) {
				Connection* bridgeIn = new Connection(*NfIn, *NnIn);
				vehicleNavGraph[NfIn->GetId()].emplace_back(NnIn->GetId(), bridgeIn);
			}
			if (NnOut && NfOut) {
				Connection* bridgeOut = new Connection(*NnOut, *NfOut);
				vehicleNavGraph[NnOut->GetId()].emplace_back(NfOut->GetId(), bridgeOut);
			}
		}
		if (NnIn && NnOut) {
			outVeh[enteringSide] = NnIn;
			outVeh[exitingSide] = NnOut;
		}
		else {
			outVeh[0] = MakeIsolatedAnchor(hostPoint.GetX() + pathPerp0X * vehOffset, hostPoint.GetY() + pathPerp0Y * vehOffset, hostZ, "vehicle");
			outVeh[1] = MakeIsolatedAnchor(hostPoint.GetX() - pathPerp0X * vehOffset, hostPoint.GetY() - pathPerp0Y * vehOffset, hostZ, "vehicle");
		}

		int nearPedLaneIndex = pickLaneIndex(hostRoad->GetPedestrianLanes(nearSide));
		int farPedLaneIndex = pickLaneIndex(hostRoad->GetPedestrianLanes(farSide));

		Node* PnA = nullptr;
		Node* PnB = nullptr;
		if (nearPedLaneIndex >= 0) {
			auto [npx, npy] = ComputeLaneAnchorPosition(hostRoad, hostT, false, nearSide, nearPedLaneIndex);
			PnA = BreakThroughLine(hostRoad, false, nearSide, nearPedLaneIndex,
				npx + pathPerp0X * pedOffset, npy + pathPerp0Y * pedOffset, hostZ);
			PnB = BreakThroughLine(hostRoad, false, nearSide, nearPedLaneIndex,
				npx - pathPerp0X * pedOffset, npy - pathPerp0Y * pedOffset, hostZ);
		}
		if (farPedLaneIndex >= 0 && PnA && PnB) {
			auto [fpx, fpy] = ComputeLaneAnchorPosition(hostRoad, hostT, false, farSide, farPedLaneIndex);
			Node* PfA = BreakThroughLine(hostRoad, false, farSide, farPedLaneIndex,
				fpx + pathPerp0X * pedOffset, fpy + pathPerp0Y * pedOffset, hostZ);
			Node* PfB = BreakThroughLine(hostRoad, false, farSide, farPedLaneIndex,
				fpx - pathPerp0X * pedOffset, fpy - pathPerp0Y * pedOffset, hostZ);
			if (PfA) {
				Connection* c = new Connection(*PfA, *PnA);
				pedestrianNavGraph[PfA->GetId()].emplace_back(PnA->GetId(), c);
				pedestrianNavGraph[PnA->GetId()].emplace_back(PfA->GetId(), c);
			}
			if (PfB) {
				Connection* c = new Connection(*PfB, *PnB);
				pedestrianNavGraph[PfB->GetId()].emplace_back(PnB->GetId(), c);
				pedestrianNavGraph[PnB->GetId()].emplace_back(PfB->GetId(), c);
			}
		}
		if (PnA && PnB) {
			outPed[0] = PnA;
			outPed[1] = PnB;
		}
		else {
			outPed[0] = MakeIsolatedAnchor(hostPoint.GetX() + pathPerp0X * pedOffset, hostPoint.GetY() + pathPerp0Y * pedOffset, hostZ, "pedestrian");
			outPed[1] = MakeIsolatedAnchor(hostPoint.GetX() - pathPerp0X * pedOffset, hostPoint.GetY() - pathPerp0Y * pedOffset, hostZ, "pedestrian");
		}
	}
}

void Map::ConnectPathRoad(const PathRoadLink& link) {
	Road* path = link.road;
	if (!path) return;

	array<Node*, 2> startVeh{ nullptr, nullptr };
	array<Node*, 2> startPed{ nullptr, nullptr };
	ResolvePathEndAnchors(path, true, link.endRoad1, link.endT1, startVeh, startPed);

	array<Node*, 2> endVeh{ nullptr, nullptr };
	array<Node*, 2> endPed{ nullptr, nullptr };
	ResolvePathEndAnchors(path, false, link.endRoad2, link.endT2, endVeh, endPed);

	// 小路自己的4条贯通线：车行side0沿Start->End、side1沿End->Start(和InitRoadnet对
	// 普通Road的建图规则一致)；人行两侧各自双向。同时登记进throughLines[path]，保持和
	// 普通Road一样的基础设施(万一以后小路自己也要被AddRoadAccessNode打开口)。
	Connection* veh0 = new Connection(*startVeh[0], *endVeh[0]);
	vehicleNavGraph[startVeh[0]->GetId()].emplace_back(endVeh[0]->GetId(), veh0);
	throughLines[path][0].push_back({ veh0, startVeh[0], endVeh[0], 0, 0.f, 1.f });

	Connection* veh1 = new Connection(*endVeh[1], *startVeh[1]);
	vehicleNavGraph[endVeh[1]->GetId()].emplace_back(startVeh[1]->GetId(), veh1);
	throughLines[path][1].push_back({ veh1, endVeh[1], startVeh[1], 0, 0.f, 1.f });

	Connection* ped0 = new Connection(*startPed[0], *endPed[0]);
	pedestrianNavGraph[startPed[0]->GetId()].emplace_back(endPed[0]->GetId(), ped0);
	pedestrianNavGraph[endPed[0]->GetId()].emplace_back(startPed[0]->GetId(), ped0);
	throughLines[path][2].push_back({ ped0, startPed[0], endPed[0], 0, 0.f, 1.f });

	Connection* ped1 = new Connection(*startPed[1], *endPed[1]);
	pedestrianNavGraph[startPed[1]->GetId()].emplace_back(endPed[1]->GetId(), ped1);
	pedestrianNavGraph[endPed[1]->GetId()].emplace_back(startPed[1]->GetId(), ped1);
	throughLines[path][3].push_back({ ped1, startPed[1], endPed[1], 0, 0.f, 1.f });
}

pair<float, float> Map::ZoneLocalToWorld(const Zone* zone, float x, float y) const {
	float rot = zone->GetRotation();
	float cosR = cosf(rot), sinR = sinf(rot);
	return { zone->GetPosX() + x * cosR - y * sinR, zone->GetPosY() + x * sinR + y * cosR };
}

Node* Map::ConnectZoneAccessPoint(Zone* zone, float x, float y, float width, bool isVehicle, bool isEntry,
	vector<tuple<float, float, bool, Node*>>& anchorCache) {

	// 比较到zone四条边(局部坐标下，原点在矩形中心)的距离，找最近的一条，和FACE_DIRECTION
	// 一一对应——和Lot局部坐标系(原点在WEST-NORTH角)的WEST=x最小/EAST=x最大/NORTH=y最小/
	// SOUTH=y最大是同一套轴向约定，只是这里的"最小/最大"是相对矩形中心的±半尺寸。
	float halfX = zone->GetSizeX() * 0.5f;
	float halfY = zone->GetSizeY() * 0.5f;
	struct EdgeCandidate { int direction; float dist; };
	EdgeCandidate candidates[4] = {
		{ FACE_WEST, x + halfX }, { FACE_EAST, halfX - x },
		{ FACE_NORTH, y + halfY }, { FACE_SOUTH, halfY - y }
	};
	int direction = candidates[0].direction;
	float best = candidates[0].dist;
	for (int i = 1; i < 4; i++) {
		if (candidates[i].dist < best) { best = candidates[i].dist; direction = candidates[i].direction; }
	}

	Road* hostRoad = zone->GetBoundaryRoad(direction);
	if (!hostRoad) return nullptr;

	auto [worldX, worldY] = ZoneLocalToWorld(zone, x, y);

	// 投影到hostRoad中心线求弧长比例t(曲线host也准确)，和BreakThroughLine内部同一套算法。
	float t = ProjectPointOntoRoad(hostRoad, worldX, worldY);

	Node hostPoint = hostRoad->GetPoint(t);
	float tdx, tdy, tdz;
	hostRoad->GetTangent(t, tdx, tdy, tdz);
	float tLen = sqrtf(tdx * tdx + tdy * tdy);
	if (tLen < 1e-6f) tLen = 1.f;
	// 右手边修正，见Map::ComputeLaneAnchorPosition实现处的说明：+Y=南，公式是(-tdy,tdx)。
	float hostPerp0X = -tdy / tLen, hostPerp0Y = tdx / tLen;

	// zone中心相对hostPoint在perp0方向的点积>=0就是host的side0，否则side1——和
	// ResolvePathEndAnchors里"小路伸向哪一端"的判断同一个方法，这里换成"zone中心在哪一侧"。
	float dot = (zone->GetPosX() - hostPoint.GetX()) * hostPerp0X + (zone->GetPosY() - hostPoint.GetY()) * hostPerp0Y;
	int side = (dot >= 0.f) ? 0 : 1;

	const vector<float>& lanes = isVehicle ? hostRoad->GetVehicleLanes(side) : hostRoad->GetPedestrianLanes(side);
	if (lanes.empty()) return nullptr;
	int laneIndex = static_cast<int>(lanes.size()) - 1;

	auto [lx, ly] = ComputeLaneAnchorPosition(hostRoad, t, isVehicle, side, laneIndex);
	Node* roadNode = BreakThroughLine(hostRoad, isVehicle, side, laneIndex, lx, ly, hostPoint.GetZ());
	if (!roadNode) return nullptr;

	RoadOpening opening;
	opening.t = t;
	opening.width = width;
	opening.forwardSide = true;
	opening.isVehicle = isVehicle;
	hostRoad->AddOpening(opening);

	Node* zoneNode = MakeIsolatedAnchor(worldX, worldY, hostPoint.GetZ(), isVehicle ? "vehicle" : "pedestrian");
	anchorCache.emplace_back(worldX, worldY, isVehicle, zoneNode);

	Node* fromNode = isEntry ? roadNode : zoneNode;
	Node* toNode = isEntry ? zoneNode : roadNode;
	Connection* edge = new Connection(*fromNode, *toNode);
	if (isVehicle) {
		vehicleNavGraph[fromNode->GetId()].emplace_back(toNode->GetId(), edge);
	}
	else {
		pedestrianNavGraph[fromNode->GetId()].emplace_back(toNode->GetId(), edge);
		pedestrianNavGraph[toNode->GetId()].emplace_back(fromNode->GetId(), edge);
	}

	return zoneNode;
}

Road* Map::ConnectZoneInternalRoad(Zone* zone, const ZoneInternalRoadSpec& spec,
	vector<tuple<float, float, bool, Node*>>& anchorCache) {

	auto [wx1, wy1] = ZoneLocalToWorld(zone, spec.x1, spec.y1);
	auto [wx2, wy2] = ZoneLocalToWorld(zone, spec.x2, spec.y2);

	Road* road = new Road("zone_internal", Node("road", wx1, wy1, 0.f), Node("road", wx2, wy2, 0.f), "", 0.f);
	// 只能是"一条车辆单行道"或"一条人行道"二选一(和vehicleEntries/vehicleExits/
	// pedestrianAccess分开指定的模型保持一致，不支持多车道/双向车行道混在一个spec里，见
	// ZoneInternalRoadSpec注释)，固定用side0——这是唯一一条车道，没有"另一侧"需要区分。
	if (spec.isVehicle) {
		road->AddVehicleLane(0, spec.width);
	}
	else {
		road->AddPedestrianLane(0, spec.width);
	}

	// 端点世界坐标+类别在ZONE_ANCHOR_MERGE_RADIUS容差(1地图单位)内重合就复用anchorCache里的node，否则新建并登记。
	// 容差要大于车道偏移量级(0.15~0.4)，才能让出入口锚点(mod声明的原始坐标)与带偏移的车道锚点自动桥接；又远小于zone尺寸，不会误合并。
	constexpr float ZONE_ANCHOR_MERGE_RADIUS_SQ = 1.f;
	auto findOrCreate = [&](float wx, float wy, bool wantVehicle) -> Node* {
		for (auto& [cx, cy, cIsVehicle, node] : anchorCache) {
			if (cIsVehicle != wantVehicle) continue;
			float dx = cx - wx, dy = cy - wy;
			if (dx * dx + dy * dy < ZONE_ANCHOR_MERGE_RADIUS_SQ) return node;
		}
		Node* n = MakeIsolatedAnchor(wx, wy, 0.f, wantVehicle ? "vehicle" : "pedestrian");
		anchorCache.emplace_back(wx, wy, wantVehicle, n);
		return n;
		};

	// 锚点位置用ComputeLaneAnchorPosition算(t=0/1，side=0)，与大路/小路同一套写法；单车道时偏移恰为0，等价于中轴线坐标。
	if (spec.isVehicle) {
		auto [fx, fy] = ComputeLaneAnchorPosition(road, 0.f, true, 0, 0);
		auto [tx, ty] = ComputeLaneAnchorPosition(road, 1.f, true, 0, 0);
		Node* fromAnchor = findOrCreate(fx, fy, true);
		Node* toAnchor = findOrCreate(tx, ty, true);
		Connection* edge = new Connection(*fromAnchor, *toAnchor);
		vehicleNavGraph[fromAnchor->GetId()].emplace_back(toAnchor->GetId(), edge);
		throughLines[road][0].push_back({ edge, fromAnchor, toAnchor, 0, 0.f, 1.f });
	}
	else {
		auto [sx, sy] = ComputeLaneAnchorPosition(road, 0.f, false, 0, 0);
		auto [ex, ey] = ComputeLaneAnchorPosition(road, 1.f, false, 0, 0);
		Node* startAnchor = findOrCreate(sx, sy, false);
		Node* endAnchor = findOrCreate(ex, ey, false);
		Connection* edge = new Connection(*startAnchor, *endAnchor);
		pedestrianNavGraph[startAnchor->GetId()].emplace_back(endAnchor->GetId(), edge);
		pedestrianNavGraph[endAnchor->GetId()].emplace_back(startAnchor->GetId(), edge);
		throughLines[road][2].push_back({ edge, startAnchor, endAnchor, 0, 0.f, 1.f });
	}

	return road;
}

Building* Map::PlaceZoneInternalBuilding(Zone* zone, const ZoneInternalBuildingSpec& spec,
	const vector<Road*>& builtInternalRoads, BuildingMod* mod) {

	Building* building = new Building(&buildingFactory, mod);
	auto [wx, wy] = ZoneLocalToWorld(zone, spec.x, spec.y);
	building->SetPosition(wx, wy, spec.sizeX, spec.sizeY);
	building->SetParentZone(zone);
	building->SetParentLot(zone->GetParentLot(), spec.relativeRotation);

	for (auto& [direction, roadIndex] : spec.roadIndices) {
		if (roadIndex >= 0 && roadIndex < static_cast<int>(builtInternalRoads.size())) {
			building->SetBoundaryRoad(direction, builtInternalRoads[roadIndex]);
		}
	}

	return building;
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

const string& Map::GetTerrainRef(int x, int y) const {
	static const string kEmpty;
	if (!CheckXY(x, y)) return kEmpty;
	return At(x, y).terrain;
}

string Map::GetTerrain(int x, int y) const {
	if (!CheckXY(x, y)) {
		debugf("Warning: Invalid coordinates (%d, %d) for map.\n", x, y);
		return "";
	}
	return At(x, y).terrain;
}

bool Map::SetTerrain(int x, int y, const string& terrain) {
	if (!CheckXY(x, y)) {
		debugf("Warning: Invalid coordinates (%d, %d) for map.\n", x, y);
		return false;
	}
	At(x, y).terrain = terrain;
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
	// 格点(x,y)是这4块细化patch共用的角点，粗高度变了它们的角点就过期了。
	for (int dy = -1; dy <= 0; dy++)
		for (int dx = -1; dx <= 0; dx++)
			if (CheckXY(x + dx, y + dy)) At(x + dx, y + dy).fineHeights.reset();
	return true;
}

const float* Map::GetFineHeights(int x, int y) const {
	if (!CheckXY(x, y)) {
		debugf("Warning: Invalid coordinates (%d, %d) for map.\n", x, y);
		return nullptr;
	}
	const auto& fine = At(x, y).fineHeights;
	return fine ? fine->data() : nullptr;
}

bool Map::SetFineHeights(int x, int y, const float* data) {
	if (!CheckXY(x, y)) {
		debugf("Warning: Invalid coordinates (%d, %d) for map.\n", x, y);
		return false;
	}
	auto& fine = At(x, y).fineHeights;
	if (!data) {
		fine.reset();
		return true;
	}
	if (!fine) fine = make_unique<array<float, TERRAIN_FINE_SIZE * TERRAIN_FINE_SIZE>>();
	copy(data, data + TERRAIN_FINE_SIZE * TERRAIN_FINE_SIZE, fine->begin());
	return true;
}

float Map::SampleHeight(float x, float y) const {
	if (width <= 0 || height <= 0) return 0.f;
	int ex0 = max(0, min(width - 1, (int)floorf(x)));
	int ey0 = max(0, min(height - 1, (int)floorf(y)));
	float tx = max(0.f, min(1.f, x - ex0));
	float ty = max(0.f, min(1.f, y - ey0));

	const auto& fine = At(ex0, ey0).fineHeights;
	if (fine) {
		// LOD顶点的坐标是2的幂分数，fx/fy正好是整数，下面的权重退化成0/1，等于直接查表。
		float fx = tx * TERRAIN_FINE_SUBDIV, fy = ty * TERRAIN_FINE_SUBDIV;
		int ix = min(TERRAIN_FINE_SUBDIV - 1, (int)fx);
		int iy = min(TERRAIN_FINE_SUBDIV - 1, (int)fy);
		float sx = fx - ix, sy = fy - iy;
		const float* row0 = fine->data() + iy * TERRAIN_FINE_SIZE;
		const float* row1 = row0 + TERRAIN_FINE_SIZE;
		return row0[ix] * (1 - sx) * (1 - sy) + row0[ix + 1] * sx * (1 - sy) +
			row1[ix] * (1 - sx) * sy + row1[ix + 1] * sx * sy;
	}

	int ex1 = min(ex0 + 1, width - 1);
	int ey1 = min(ey0 + 1, height - 1);
	return At(ex0, ey0).height * (1 - tx) * (1 - ty) +
		At(ex1, ey0).height * tx * (1 - ty) +
		At(ex0, ey1).height * (1 - tx) * ty +
		At(ex1, ey1).height * tx * ty;
}

pair<bool, float> Map::GetWater(int x, int y) const {
	if (!CheckXY(x, y)) {
		debugf("Warning: Invalid coordinates (%d, %d) for map.\n", x, y);
		return { false, 0.f };
	}
	return At(x, y).water;
}

bool Map::SetWater(int x, int y, bool hasWater, float height) {
	if (!CheckXY(x, y)) {
		debugf("Warning: Invalid coordinates (%d, %d) for map.\n", x, y);
		return false;
	}
	At(x, y).water = { hasWater, height };
	return true;
}

const vector<TerrainHatch>& Map::GetHatches(int x, int y) const {
	static const vector<TerrainHatch> empty;
	if (!CheckXY(x, y)) {
		debugf("Warning: Invalid coordinates (%d, %d) for map.\n", x, y);
		return empty;
	}
	return At(x, y).hatches;
}

void Map::ApplyTerrainFits(const vector<TerrainFitRequest>& fits) {
	if (fits.empty()) return;

	const int sub = TERRAIN_FINE_SUBDIV;
	const int size1 = TERRAIN_FINE_SIZE;

	// 每个被碰到的element一份"贴合权重/目标高度"累加器，按它自己的17x17格点存(整数边界上的格点会
	// 同时出现在相邻最多4个element里，每个element各存一份，算出来的值是同一个)。
	struct ElementFit {
		array<float, TERRAIN_FINE_SIZE * TERRAIN_FINE_SIZE> weight;
		array<float, TERRAIN_FINE_SIZE * TERRAIN_FINE_SIZE> z;
		array<float, TERRAIN_FINE_SIZE * TERRAIN_FINE_SIZE> dist; // 命中这个格点的最近折线距离，同权重时用来挑最近的那一段
		ElementFit() { weight.fill(0.f); z.fill(0.f); dist.fill(FLT_MAX); }
	};
	unordered_map<long long, unique_ptr<ElementFit>> accumulators;
	auto getAccumulator = [&](int ex, int ey) -> ElementFit& {
		long long key = static_cast<long long>(ey) * width + ex;
		auto& slot = accumulators[key];
		if (!slot) slot = make_unique<ElementFit>();
		return *slot;
	};

	// 1. 收集：每条请求的每一段折线，枚举它外扩(半宽+falloff)范围内的细化格点，取到折线的最近距离算权重，
	//    同一格点被多段/多请求命中时取权重最大的(也就是离路面最近的)，所以和遍历顺序无关。
	const int maxGx = width * sub, maxGy = height * sub;
	for (const TerrainFitRequest& fit : fits) {
		if (fit.centerline.empty() || fit.width <= 0.f) continue;
		float halfWidth = fit.width * 0.5f;
		float falloff = max(fit.falloff, 0.f);
		float reach = halfWidth + falloff;

		size_t segmentCount = max<size_t>(1, fit.centerline.size() - 1);
		for (size_t s = 0; s < segmentCount; s++) {
			const TerrainFitSample& a = fit.centerline[s];
			const TerrainFitSample& b = fit.centerline[min(s + 1, fit.centerline.size() - 1)];

			int gx0 = max(0, static_cast<int>(floorf((min(a.x, b.x) - reach) * sub)));
			int gx1 = min(maxGx, static_cast<int>(ceilf((max(a.x, b.x) + reach) * sub)));
			int gy0 = max(0, static_cast<int>(floorf((min(a.y, b.y) - reach) * sub)));
			int gy1 = min(maxGy, static_cast<int>(ceilf((max(a.y, b.y) + reach) * sub)));

			float abx = b.x - a.x, aby = b.y - a.y;
			float len2 = abx * abx + aby * aby;

			for (int gy = gy0; gy <= gy1; gy++) {
				for (int gx = gx0; gx <= gx1; gx++) {
					float px = static_cast<float>(gx) / sub, py = static_cast<float>(gy) / sub;
					float t = 0.f;
					if (len2 > 1e-12f) t = max(0.f, min(1.f, ((px - a.x) * abx + (py - a.y) * aby) / len2));
					float dx = px - (a.x + abx * t), dy = py - (a.y + aby * t);
					float d = sqrtf(dx * dx + dy * dy);

					float w;
					if (d <= halfWidth) w = 1.f;
					else if (d < reach) {
						float u = (d - halfWidth) / falloff;
						w = 1.f - u * u * (3.f - 2.f * u);
					}
					else continue;
					float roadZ = a.z + (b.z - a.z) * t;

					// 这个格点属于哪些element：落在整数边界上时还属于左/下一个element的末尾行/列。
					int exMain = gx / sub, exSub = gx % sub;
					int eyMain = gy / sub, eySub = gy % sub;
					for (int oy = 0; oy < 2; oy++) {
						int ey = oy == 0 ? eyMain : eyMain - 1;
						int j = oy == 0 ? eySub : sub;
						if (oy == 1 && eySub != 0) continue;
						if (ey < 0 || ey >= height) continue;
						for (int ox = 0; ox < 2; ox++) {
							int ex = ox == 0 ? exMain : exMain - 1;
							int i = ox == 0 ? exSub : sub;
							if (ox == 1 && exSub != 0) continue;
							if (ex < 0 || ex >= width) continue;
							ElementFit& acc = getAccumulator(ex, ey);
							int idx = j * size1 + i;
							// 取权重大的；权重相同(路面宽度以内权重恒为1，几段折线都命中)时取离折线最近的那
							// 一段的路面高度——只比权重会让"最先遍历到的那一段"的高度占住整片路面，地形
							// 就变成一级一级的台阶(下坡时高出路面、上坡时低于路面)。
							if (w > acc.weight[idx] || (w == acc.weight[idx] && d < acc.dist[idx])) {
								acc.weight[idx] = w;
								acc.z[idx] = roadZ;
								acc.dist[idx] = d;
							}
						}
					}
				}
			}
		}
	}
	if (accumulators.empty()) return;

	// 2. 细分：被碰到的element还没有细化数组的，先按当前地形采样建出17x17——必须在写入任何高度之前
	//    全部建完，建数组读的SampleHeight才不会读到别的element写了一半的结果。
	for (const auto& [key, acc] : accumulators) {
		int ex = static_cast<int>(key % width), ey = static_cast<int>(key / width);
		if (At(ex, ey).fineHeights) continue;
		// 先算进局部数组再挂到element上：SampleHeight一看到element有fineHeights就查表，如果先挂一个
		// 还没填完的数组再边填边采样，读到的就是零初始化的0，整个element（路面以外的部分）都会变成0。
		auto fine = make_unique<array<float, TERRAIN_FINE_SIZE * TERRAIN_FINE_SIZE>>();
		for (int j = 0; j < size1; j++) {
			for (int i = 0; i < size1; i++) {
				(*fine)[j * size1 + i] = SampleHeight(static_cast<float>(ex) + static_cast<float>(i) / sub,
					static_cast<float>(ey) + static_cast<float>(j) / sub);
			}
		}
		At(ex, ey).fineHeights = std::move(fine);
	}

	// 3. 写入：权重>0的格点 新高度=lerp(原高度, 路面高度, 权重)；element自己的左下角整数格点同时更新
	//    粗高度(直接写，不经SetHeight)，让粗高度和细化数组角点保持一致。
	for (const auto& [key, acc] : accumulators) {
		int ex = static_cast<int>(key % width), ey = static_cast<int>(key / width);
		auto& fine = *At(ex, ey).fineHeights;
		for (int idx = 0; idx < size1 * size1; idx++) {
			float w = acc->weight[idx];
			if (w <= 0.f) continue;
			fine[idx] += (acc->z[idx] - fine[idx]) * w;
		}
		if (acc->weight[0] > 0.f) At(ex, ey).height = fine[0];
	}
}

void Map::AddHatch(Quad q, float rotation) {
	TerrainHatch hatch;
	hatch.quad = q;
	hatch.rotation = rotation;
	AddHatch(hatch);
}

std::vector<TerrainHatch> Map::GetTunnelWallHatches() const {
	vector<TerrainHatch> result;
	if (!roadnet) return result;
	for (const TerrainHatch& hatch : roadnet->GetHatches()) {
		if (hatch.wall) result.push_back(hatch);
	}
	return result;
}

void Map::AddHatch(const TerrainHatch& hatch) {
	const Quad& q = hatch.quad;
	float rotation = hatch.rotation;
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
			At(cx, cy).hatches.push_back(hatch);
		}
	}
}

const unordered_map<string, Map::TerrainTextureInfo>& Map::GetTerrainTextures() const {
	return terrainTextures;
}

vector<int> Map::FindPath(const unordered_map<int, vector<pair<int, Connection*>>>& graph,
	int fromNodeId, int toNodeId, const unordered_map<int, const Node*>* coords) const {
	if (fromNodeId == toNodeId) return { fromNodeId };

	// A*启发：到终点的水平直线距离。取不到坐标就退化成0（等价Dijkstra）。
	float goalX = 0.f, goalY = 0.f;
	bool useHeuristic = false;
	if (coords) {
		auto goalIt = coords->find(toNodeId);
		if (goalIt != coords->end()) {
			goalX = goalIt->second->GetX();
			goalY = goalIt->second->GetY();
			useHeuristic = true;
		}
	}
	auto heuristic = [&](int id) -> float {
		if (!useHeuristic) return 0.f;
		auto it = coords->find(id);
		if (it == coords->end()) return 0.f;
		float dx = it->second->GetX() - goalX, dy = it->second->GetY() - goalY;
		return sqrtf(dx * dx + dy * dy);
	};

	unordered_map<int, float> dist;
	unordered_map<int, int> prev;
	unordered_set<int> visited;
	using QueueEntry = pair<float, int>; // (优先级=已走距离+启发值, nodeId)，最小堆
	priority_queue<QueueEntry, vector<QueueEntry>, greater<QueueEntry>> queue;

	dist[fromNodeId] = 0.f;
	queue.push({ heuristic(fromNodeId), fromNodeId });

	while (!queue.empty()) {
		auto [priority, id] = queue.top();
		queue.pop();
		if (visited.count(id)) continue;
		visited.insert(id);
		if (id == toNodeId) break;
		float d = dist[id];

		auto it = graph.find(id);
		if (it == graph.end()) continue;
		for (const auto& [neighborId, connection] : it->second) {
			if (!connection || visited.count(neighborId)) continue;
			float weight = connection->CalcDistance();
			float newDist = d + weight;
			auto distIt = dist.find(neighborId);
			if (distIt == dist.end() || newDist < distIt->second) {
				dist[neighborId] = newDist;
				prev[neighborId] = id;
				queue.push({ newDist + heuristic(neighborId), neighborId });
			}
		}
	}

	if (!dist.count(toNodeId)) return {}; // 图不连通，找不到路径

	vector<int> idPath;
	for (int id = toNodeId; ; ) {
		idPath.push_back(id);
		if (id == fromNodeId) break;
		auto it = prev.find(id);
		if (it == prev.end()) return {}; // 理论不会发生(dist已确认可达)，防御性兜底
		id = it->second;
	}
	reverse(idPath.begin(), idPath.end());
	return idPath;
}

vector<const Node*> Map::FindPedestrianPath(int fromNodeId, int toNodeId) const {
	// id -> 拥有生命周期的Node*，供按id序列反查真正的指针（Connection::GetStart()/GetEnd()
	// 返回的是Node副本，不能直接拿它们的地址）。缓存起来，节点数量变了才重建。
	// 只在检查/重建时持锁；之后多个线程并发只读nodeIndex（重建只会发生在地图生成阶段，那时还没有异步寻路）。
	{
		std::lock_guard<std::mutex> lock(nodeIndexMutex);
		const vector<Node*>& externs = GetExterns();
		if (nodeIndexAnchorCount != navAnchorNodes.size() || nodeIndexExternCount != externs.size()) {
			nodeIndex.clear();
			for (const Node* node : navAnchorNodes) nodeIndex[node->GetId()] = node;
			for (const Node* node : externs) nodeIndex[node->GetId()] = node;
			nodeIndexAnchorCount = navAnchorNodes.size();
			nodeIndexExternCount = externs.size();
		}
	}

	vector<int> idPath = FindPath(pedestrianNavGraph, fromNodeId, toNodeId, &nodeIndex);
	if (idPath.empty()) return {};

	vector<const Node*> result;
	for (int id : idPath) {
		auto it = nodeIndex.find(id);
		if (it != nodeIndex.end()) result.push_back(it->second);
	}
	return result;
}

vector<Connection*> Map::FindVehiclePath(int fromNodeId, int toNodeId) const {
	vector<int> idPath = FindPath(vehicleNavGraph, fromNodeId, toNodeId);
	if (idPath.size() < 2) return {}; // 起点终点相同(idPath=={fromNodeId})没有edge可拼，视为无路径

	vector<Connection*> result;
	for (size_t i = 0; i + 1 < idPath.size(); i++) {
		auto it = vehicleNavGraph.find(idPath[i]);
		if (it == vehicleNavGraph.end()) return {}; // 理论不会发生，防御性兜底
		Connection* edge = nullptr;
		for (const auto& [neighborId, connection] : it->second) {
			if (neighborId == idPath[i + 1]) { edge = connection; break; }
		}
		if (!edge) return {};
		result.push_back(edge);
	}
	return result;
}

bool Map::GetLaneSegmentAt(Road* road, float x, float y, float dirX, float dirY, LaneSegment& out) const {
	if (!road) return false;

	float t = ProjectPointOntoRoad(road, x, y);

	// 用dir和道路在t处切线的点积判断同向的那一侧(同ConnectPathRoad/ConnectZoneAccessPoint判断近侧的思路)。
	float tdx, tdy, tdz;
	road->GetTangent(t, tdx, tdy, tdz);
	bool useForwardSide = (dirX * tdx + dirY * tdy) >= 0.f;

	int side, laneIndex;
	if (!ResolveAccessLane(road, /*isVehicle=*/true, useForwardSide, side, laneIndex)) return false;

	auto it = throughLines.find(road);
	if (it == throughLines.end()) return false;
	for (const ThroughLine& line : it->second[side]) {
		if (line.laneIndex != laneIndex) continue;
		if (t < line.tLo || t > line.tHi) continue;

		out.edge = line.edge;
		out.fromAnchor = line.fromAnchor;
		out.toAnchor = line.toAnchor;
		out.t = t;
		auto [wx, wy] = ComputeLaneAnchorPosition(road, t, true, side, laneIndex);
		out.worldX = wx;
		out.worldY = wy;
		out.worldZ = road->GetPoint(t).GetZ();
		return true;
	}
	return false; // 理论不会发生：每条车道全长应该被throughLines的若干段完整覆盖
}

Map::VehicleRoutePlan Map::PlanPedestrianRoute(Room* startRoom, float x, float y, Room* destRoom) const {
	VehicleRoutePlan plan;
	Node* destNode = destRoom ? destRoom->GetNavigationNode() : nullptr;
	if (!destNode) return plan;

	// 起点候选：房间节点优先；否则按水平距离排序的行人图节点（有出边的才算），逐个试。
	vector<const Node*> candidates;
	Node* startNode = startRoom ? startRoom->GetNavigationNode() : nullptr;
	if (startNode && pedestrianNavGraph.count(startNode->GetId())) {
		candidates.push_back(startNode);
	}
	else {
		vector<pair<float, const Node*>> nearest;
		auto consider = [&](const Node* node) {
			if (!node || !pedestrianNavGraph.count(node->GetId())) return;
			float dx = node->GetX() - x, dy = node->GetY() - y;
			nearest.push_back({ dx * dx + dy * dy, node });
		};
		for (const Node* node : navAnchorNodes) consider(node);
		for (const Node* node : GetExterns()) consider(node);
		sort(nearest.begin(), nearest.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
		for (size_t i = 0; i < nearest.size() && i < 6; i++) candidates.push_back(nearest[i].second);
	}
	if (candidates.empty()) return plan;

	auto toPoint = [](const Node* node) { return NavPoint{ node->GetX(), node->GetY(), node->GetZ() }; };
	for (const Node* candidate : candidates) {
		if (candidate->GetId() == destNode->GetId()) {
			plan.status = VehicleRoutePlan::OK;
			plan.points = { toPoint(candidate) };
			return plan;
		}
		vector<const Node*> path = FindPedestrianPath(candidate->GetId(), destNode->GetId());
		if (path.empty()) continue;
		plan.status = VehicleRoutePlan::OK;
		for (const Node* node : path) plan.points.push_back(toPoint(node));
		return plan;
	}

	plan.status = VehicleRoutePlan::NO_PATH_TO_DESTINATION;
	plan.points = { toPoint(candidates.front()) };
	return plan;
}

namespace {
	// 沿Connection从起点到终点采样（车行边都是单向Start->End）；和上一段首尾重合的点不重复加。
	void AppendConnectionPoints(vector<Map::NavPoint>& points, Connection* connection) {
		int steps = std::clamp(static_cast<int>(ceilf(connection->CalcDistance() / 0.5f)), 2, 64);
		for (int i = points.empty() ? 0 : 1; i <= steps; i++) {
			Node p = connection->GetPoint(static_cast<float>(i) / steps);
			points.push_back({ p.GetX(), p.GetY(), p.GetZ() });
		}
	}

	float PathLength(const vector<Connection*>& edges) {
		float total = 0.f;
		for (Connection* edge : edges) total += edge->CalcDistance();
		return total;
	}
}

void Map::RegisterAuxNavNode(const AuxNavNode& aux) {
	if (!aux.node) return;
	auxNavNodes[aux.node->GetId()] = aux;
}

void Map::UnregisterAuxNavNode(int nodeId) {
	auxNavNodes.erase(nodeId);
}

const Map::AuxNavNode* Map::FindAuxNavNode(int nodeId) const {
	auto it = auxNavNodes.find(nodeId);
	return it != auxNavNodes.end() ? &it->second : nullptr;
}

bool Map::AppendLegPoints(int fromId, int toId, vector<NavPoint>& points) const {
	if (fromId == toId) return true;
	const AuxNavNode* fromAux = FindAuxNavNode(fromId);
	const AuxNavNode* toAux = FindAuxNavNode(toId);

	vector<NavPoint> leg = points;
	auto addNode = [&leg](const Node* node) {
		leg.push_back({ node->GetX(), node->GetY(), node->GetZ() });
	};

	// 同一段车道上，起点站在上游：直接走两个stationNode的直线弦。
	if (fromAux && toAux && fromAux->sourceEdge == toAux->sourceEdge && fromAux->t < toAux->t) {
		if (leg.empty()) addNode(fromAux->node);
		addNode(toAux->node);
		points = std::move(leg);
		return true;
	}

	int graphFrom = fromAux ? fromAux->laneTo->GetId() : fromId;
	int graphTo = toAux ? toAux->laneFrom->GetId() : toId;
	vector<Connection*> middle;
	if (graphFrom != graphTo) {
		middle = FindVehiclePath(graphFrom, graphTo);
		if (middle.empty()) return false;
	}

	if (fromAux) {
		if (leg.empty()) addNode(fromAux->node);
		AppendConnectionPoints(leg, fromAux->outEdge);
	}
	for (Connection* edge : middle) AppendConnectionPoints(leg, edge);
	if (toAux) AppendConnectionPoints(leg, toAux->inEdge);
	points = std::move(leg);
	return true;
}

Map::VehicleRoutePlan Map::PlanVehicleLeg(int fromNodeId, int toNodeId) const {
	VehicleRoutePlan plan;
	if (fromNodeId == toNodeId) {
		// 起点==终点：只给一个点（取辅助节点的坐标）。
		if (const AuxNavNode* aux = FindAuxNavNode(fromNodeId)) {
			plan.points.push_back({ aux->node->GetX(), aux->node->GetY(), aux->node->GetZ() });
			plan.status = VehicleRoutePlan::OK;
		}
		else {
			plan.status = VehicleRoutePlan::NO_PATH_TO_DESTINATION;
		}
		return plan;
	}
	plan.status = AppendLegPoints(fromNodeId, toNodeId, plan.points)
		? VehicleRoutePlan::OK : VehicleRoutePlan::NO_PATH_TO_DESTINATION;
	if (plan.status != VehicleRoutePlan::OK) plan.points.clear();
	return plan;
}

Map::VehicleRoutePlan Map::PlanVehicleRoute(Room* startRoom, float x, float y, Room* destRoom) const {
	Node* destNode = destRoom ? destRoom->GetNavigationNode() : nullptr;
	if (!destNode) {
		VehicleRoutePlan plan;
		plan.status = VehicleRoutePlan::NO_PATH_TO_DESTINATION;
		return plan;
	}
	return PlanVehicleRouteToNode(startRoom, x, y, destNode->GetId());
}

Map::VehicleRoutePlan Map::PlanVehicleRouteToNode(Room* startRoom, float x, float y, int destNodeId,
	bool useHeading, float headingX, float headingY) const {
	VehicleRoutePlan plan;
	const AuxNavNode* destAux = FindAuxNavNode(destNodeId);
	// 终点在车行图上的接入锚点：辅助节点（公交站）接在laneFrom，之后再走inEdge。
	int graphDestId = destAux ? destAux->laneFrom->GetId() : destNodeId;

	// 车在房间里：从房间自己的导航节点出发。
	if (startRoom) {
		Node* startNode = startRoom->GetNavigationNode();
		auto outIt = startNode ? vehicleNavGraph.find(startNode->GetId()) : vehicleNavGraph.end();
		if (outIt == vehicleNavGraph.end() || outIt->second.empty()) return plan; // NO_ROAD_CONNECTION

		if (startNode->GetId() == destNodeId) {
			plan.status = VehicleRoutePlan::OK; // 已经在目的房间
			return plan;
		}
		plan.points.push_back({ startNode->GetX(), startNode->GetY(), startNode->GetZ() });
		if (!AppendLegPoints(startNode->GetId(), destNodeId, plan.points)) {
			plan.status = VehicleRoutePlan::NO_PATH_TO_DESTINATION;
			plan.points.resize(1);
			AppendConnectionPoints(plan.points, outIt->second.front().second); // 只画连出去的第一条边
			return plan;
		}
		plan.status = VehicleRoutePlan::OK;
		return plan;
	}

	// 车不在房间里：收集所有能走车的路（大路+小路+Zone内部路），按垂足距离排序。
	vector<Road*> roads = GetRoads();
	for (Road* road : GetPathRoads()) roads.push_back(road);
	for (const auto& [zoneName, zone] : GetZones()) {
		if (!zone) continue;
		for (Road* road : zone->GetInternalRoads()) roads.push_back(road);
	}
	struct RoadCandidate { Road* road; float distSq; };
	vector<RoadCandidate> nearby;
	for (Road* road : roads) {
		if (!road) continue;
		float t = ProjectPointOntoRoad(road, x, y);
		Node foot = road->GetPoint(t);
		float dx = foot.GetX() - x, dy = foot.GetY() - y;
		nearby.push_back({ road, dx * dx + dy * dy });
	}
	sort(nearby.begin(), nearby.end(), [](const RoadCandidate& a, const RoadCandidate& b) { return a.distSq < b.distSq; });

	// 每条候选路按两个行驶方向各试一次，取总长最短的；只试最近的几条（有的路没有车行道/
	// 没接进图，GetLaneSegmentAt会失败）。找不到通路时退而求其次，用第一个拿到车道段的候选
	// 画"接到路上"的短折线。
	struct Pick {
		bool valid = false;
		LaneSegment segment;
		vector<Connection*> edges;
		bool reachable = false;
		bool direct = false; // 终点是同一段车道上、位于下游的公交站：垂足直接开到站
		float cost = FLT_MAX;
	};
	Pick best, fallback;
	int tried = 0;
	for (const RoadCandidate& candidate : nearby) {
		if (tried >= kEntryRoadCandidates) break;
		float t = ProjectPointOntoRoad(candidate.road, x, y);
		float tdx, tdy, tdz;
		candidate.road->GetTangent(t, tdx, tdy, tdz);
		bool anySegment = false;
		for (int direction = 0; direction < 2; direction++) {
			float sign = direction == 0 ? 1.f : -1.f;
			LaneSegment segment;
			if (!GetLaneSegmentAt(candidate.road, x, y, tdx * sign, tdy * sign, segment) || !segment.toAnchor) continue;
			anySegment = true;

				// 车头朝向已知：车道方向和车头的夹角换算成额外距离（180度 = kHeadingPenaltyMapUnits），
				// 窄路上掉不了头，8m外夹角小的车道要优先于2m外夹角大的车道。
				float headingPenalty = 0.f;
				if (useHeading) {
					float laneX = tdx * sign, laneY = tdy * sign;
					float laneLen = sqrtf(laneX * laneX + laneY * laneY), headingLen = sqrtf(headingX * headingX + headingY * headingY);
					if (laneLen > 1e-6f && headingLen > 1e-6f) {
						float cosAngle = std::clamp((laneX * headingX + laneY * headingY) / (laneLen * headingLen), -1.f, 1.f);
						headingPenalty = kHeadingPenaltyMapUnits * acosf(cosAngle) / 3.14159265f;
					}
				}

			float toFootX = segment.worldX - x, toFootY = segment.worldY - y;
			float toAnchorX = segment.toAnchor->GetX() - segment.worldX, toAnchorY = segment.toAnchor->GetY() - segment.worldY;
			float approach = sqrtf(toFootX * toFootX + toFootY * toFootY) + sqrtf(toAnchorX * toAnchorX + toAnchorY * toAnchorY);

			Pick pick;
			pick.valid = true;
			pick.segment = segment;
			if (destAux && segment.edge == destAux->sourceEdge && segment.t <= destAux->t) {
				float toStationX = destAux->node->GetX() - segment.worldX, toStationY = destAux->node->GetY() - segment.worldY;
				pick.direct = true;
				pick.reachable = true;
				pick.cost = sqrtf(toFootX * toFootX + toFootY * toFootY) + sqrtf(toStationX * toStationX + toStationY * toStationY);
			}
			else if (segment.toAnchor->GetId() == graphDestId) {
				pick.reachable = true;
				pick.cost = approach + (destAux ? destAux->inEdge->CalcDistance() : 0.f);
			}
			else {
				pick.edges = FindVehiclePath(segment.toAnchor->GetId(), graphDestId);
				pick.reachable = !pick.edges.empty();
				pick.cost = approach + PathLength(pick.edges) + (destAux ? destAux->inEdge->CalcDistance() : 0.f);
			}
				pick.cost += headingPenalty;
			if (!fallback.valid) fallback = pick;
			if (pick.reachable && pick.cost < best.cost) best = pick;
		}
		if (anySegment) tried++;
	}

	const Pick& chosen = best.valid ? best : fallback;
	if (!chosen.valid) return plan; // NO_ROAD_CONNECTION

	plan.points.push_back({ x, y, chosen.segment.worldZ });
	plan.points.push_back({ chosen.segment.worldX, chosen.segment.worldY, chosen.segment.worldZ });
	if (best.valid && chosen.direct) {
		plan.points.push_back({ destAux->node->GetX(), destAux->node->GetY(), destAux->node->GetZ() });
		plan.status = VehicleRoutePlan::OK;
		return plan;
	}
	plan.points.push_back({ chosen.segment.toAnchor->GetX(), chosen.segment.toAnchor->GetY(), chosen.segment.toAnchor->GetZ() });
	if (!best.valid) {
		plan.status = VehicleRoutePlan::NO_PATH_TO_DESTINATION;
		return plan;
	}
	for (Connection* edge : chosen.edges) AppendConnectionPoints(plan.points, edge);
	if (destAux) AppendConnectionPoints(plan.points, destAux->inEdge);
	plan.status = VehicleRoutePlan::OK;
	return plan;
}

vector<Component*> Map::GetAllComponents() const {
	vector<Component*> result;
	for (const auto& [name, building] : buildings) {
		if (!building) continue;
		for (Component* component : building->GetComponents()) {
			result.push_back(component);
		}
	}
	return result;
}

Door* Map::CreateDoor(DOOR_KIND_TYPE kind, Zone* zone, Building* building, Room* room,
	const string& tag, const DoorSpec& spec,
	float x, float y, float z, float width, float height, float yaw, int level, bool isVehicleGate) {

	string id = "Door" + to_string(doorCount++);

	// 所属对象名：Building/Zone的GetName()本身就全局唯一(mod自己的计数器保证)；Room的
	// GetName()是RoomMod类型标签("residential"这种，不是实例唯一的)，必须用GetAddress()
	// (含parentBuilding地址+房间号，全局唯一)，见door.md"tag是一类门，不是身份"一节。
	string ownerName;
	if (building) ownerName = building->GetName();
	else if (room) ownerName = room->GetAddress();
	else if (zone) ownerName = zone->GetName();

	string interactName;
	if (!spec.name.empty()) {
		string seqKey = ownerName + "\x01" + spec.name;
		int seq = ++doorNameSequence[seqKey];
		interactName = ownerName + " " + spec.name + to_string(seq);
	}

	bool flippedSide = (spec.leaves == 1 && spec.randomSide) ? (GetRandom(2) == 0) : false;

	Door* door = new Door(id, kind, tag, spec, zone, building, room, interactName, flippedSide,
		x, y, z, width, height, yaw, level, isVehicleGate, &scriptFactory);

	doorsById[id] = door;
	if (!interactName.empty()) doorsByInteractName[interactName] = door;
	allDoors.push_back(door);

	return door;
}

Cabin* Map::CreateCabin(Building* building, int specIndex, const ElevatorCabinSpec& spec) {
	if (!building) return nullptr;
	string interactName = building->GetName() + " 电梯" + to_string(building->GetCabinEntities().size() + 1);
	int initialLevel = std::clamp(0, spec.minFloor, spec.maxFloor);
	Cabin* cabin = new Cabin(building, specIndex, interactName, spec.shaftIndex, spec.minFloor, spec.maxFloor,
		initialLevel, building->GetFloorBaseZ(initialLevel), spec.scriptModName, spec.milestoneNames, &scriptFactory);
	cabinsByInteractName[interactName] = cabin;
	return cabin;
}

Asset* Map::FindAssetByName(const string& name) const {
	for (const auto& [buildingName, building] : buildings) {
		if (!building) continue;
		for (Room* room : building->GetRooms()) {
			if (!room) continue;
			const auto& assets = room->GetAssets();
			auto it = assets.find(name);
			if (it != assets.end()) return it->second;
			for (const auto& [key, asset] : assets) {
				if (Asset* found = asset->FindDescendant(name)) return found;
			}
		}
	}
	return nullptr;
}

namespace {
	// 按实体快照的scriptModName/milestoneNames建一个Script，写入name/address变量。milestoneNames为空
	// 返回nullptr(没有脚本)，和Door/Cabin同一个约定。
	Script* MakeEntityScript(ScriptFactory& factory, const string& modName, const vector<string>& milestoneNames,
		const string& name, const string& address) {
		if (milestoneNames.empty()) return nullptr;
		Script* script = new Script(&factory, modName.empty() ? string("script_empty") : modName);
		for (const string& milestoneName : milestoneNames) {
			script->ReadMilestones(Config::GetScriptPath(milestoneName));
		}
		script->SetValue("name", ValueType(name));
		script->SetValue("address", ValueType(address));
		return script;
	}
}

void Map::BuildZoneScript(Zone* zone) {
	if (!zone || zone->script) return;
	zone->script = MakeEntityScript(scriptFactory, zone->scriptModName, zone->scriptMilestones,
		zone->GetName(), zone->GetAddress());
}

void Map::BuildBuildingScripts(Building* building) {
	if (!building) return;
	if (!building->script) {
		building->script = MakeEntityScript(scriptFactory, building->scriptModName, building->scriptMilestones,
			building->GetName(), building->GetAddress());
	}
	for (Room* room : building->GetRooms()) {
		if (!room || room->script) continue;
		room->script = MakeEntityScript(scriptFactory, room->scriptModName, room->scriptMilestones,
			room->GetName(), room->GetAddress());
		if (room->script) room->script->SetValue("building", ValueType(building->GetName()));
	}
}

void Map::BuildAllEntityScripts() {
	for (auto& [name, zone] : zones) BuildZoneScript(zone);
	for (auto& [name, building] : buildings) BuildBuildingScripts(building);
}

Cabin* Map::FindCabinByName(const string& interactName) const {
	auto it = cabinsByInteractName.find(interactName);
	return it != cabinsByInteractName.end() ? it->second : nullptr;
}

namespace {
	// launch_elevator的command(通常是被点的选项文字，比如"前往3层"/"前往B1层")解析成层号：
	// B/b后面跟数字n -> -n(地下n层)；否则取第一个数字n -> n-1(n楼)。找不到数字返回false。
	bool ParseElevatorCommand(const string& command, int& outLevel) {
		for (size_t i = 0; i < command.size(); i++) {
			unsigned char c = static_cast<unsigned char>(command[i]);
			if ((c == 'B' || c == 'b') && i + 1 < command.size() && isdigit(static_cast<unsigned char>(command[i + 1]))) {
				outLevel = -atoi(command.c_str() + i + 1);
				return true;
			}
			if (isdigit(c)) {
				outLevel = atoi(command.c_str() + i) - 1;
				return true;
			}
		}
		return false;
	}
}

Door* Map::FindDoorByName(const string& interactName) const {
	auto it = doorsByInteractName.find(interactName);
	return it != doorsByInteractName.end() ? it->second : nullptr;
}

const vector<Door*>& Map::GetAllDoors() const { return allDoors; }

void Map::Tick(const Time& currentTime, bool crossedDay, PostHandle* post) {
	// 空实现，见map.md。
}

bool Map::ApplyChange(const Change* change, const ScriptContext& context) {
	// 门/电梯/资产的选项与门状态Change，全部按交互名查目标，找不到静默忽略(同Traffic::ApplyChange)。
	if (auto* addOption = dynamic_cast<const AddOptionChange*>(change)) {
		string name = ToString(EvaluateExpression(addOption->GetName(), context));
		string option = ToString(EvaluateExpression(addOption->GetOption(), context));
		if (Door* door = FindDoorByName(name)) door->AddOption(option);
		if (Cabin* cabin = FindCabinByName(name)) cabin->AddOption(option);
		if (Asset* asset = FindAssetByName(name)) asset->AddOption(option); // 房间里的资产
		return true;
	}
	if (auto* removeOption = dynamic_cast<const RemoveOptionChange*>(change)) {
		string name = ToString(EvaluateExpression(removeOption->GetName(), context));
		string option = ToString(EvaluateExpression(removeOption->GetOption(), context));
		if (Door* door = FindDoorByName(name)) door->RemoveOption(option);
		if (Cabin* cabin = FindCabinByName(name)) cabin->RemoveOption(option);
		if (Asset* asset = FindAssetByName(name)) asset->RemoveOption(option); // 房间里的资产
		return true;
	}
	if (auto* launch = dynamic_cast<const LaunchElevatorChange*>(change)) {
		// elevator是电梯的交互名(脚本里的$$self.name)；building只用于日志。command解析成层号，见
		// ParseElevatorCommand。真正的移动由UE层ABuildingElement::Tick按Cabin的行程状态推进。
		string elevator = ToString(EvaluateExpression(launch->GetElevator(), context));
		string command = ToString(EvaluateExpression(launch->GetCommand(), context));
		Cabin* cabin = FindCabinByName(elevator);
		int level = 0;
		if (!cabin) {
			debugf("Warning: launch_elevator: elevator %s not found.\n", elevator.c_str());
		}
		else if (!ParseElevatorCommand(command, level)) {
			debugf("Warning: launch_elevator: cannot parse floor from command \"%s\".\n", command.c_str());
		}
		else {
			cabin->Launch(level);
		}
		return true;
	}
	if (auto* setDoor = dynamic_cast<const SetDoorChange*>(change)) {
		// access是布尔表达式：true=常开，false=常锁。
		Door* target = FindDoorByName(ToString(EvaluateExpression(setDoor->GetName(), context)));
		if (target) target->SetAccess(EvaluateExpressionBool(setDoor->GetAccess(), context) ? DOOR_ACCESS_OPEN : DOOR_ACCESS_LOCKED);
		return true;
	}
	if (auto* record = dynamic_cast<const RecordObjectChange*>(change)) {
		// 候选地址按object类别收集(逻辑同PostImplement的list zones/buildings/rooms)，policy选一个写进
		// variable；变量名规则同set_value。unordered_map遍历顺序不稳定，first只保证"某一个"，全图唯一的
		// mod取它无碍。
		string object = ToString(EvaluateExpression(record->GetObject(), context));
		string kind = ToString(EvaluateExpression(record->GetKind(), context));
		string policy = ToString(EvaluateExpression(record->GetPolicy(), context));
		vector<string> candidates;
		if (object == "zone") {
			for (const auto& [name, zone] : zones) {
				if (zone && (kind.empty() || zone->GetType() == kind)) candidates.push_back(zone->GetAddress());
			}
		}
		else if (object == "building") {
			for (const auto& [name, building] : buildings) {
				if (building && (kind.empty() || building->GetType() == kind)) candidates.push_back(building->GetAddress());
			}
		}
		else if (object == "room") {
			for (const auto& [name, building] : buildings) {
				if (!building) continue;
				for (Room* room : building->GetRooms()) {
					if (room && (kind.empty() || room->GetType() == kind)) candidates.push_back(room->GetAddress());
				}
			}
		}
		else {
			debugf("Warning: record_object: unknown object \"%s\".\n", object.c_str());
		}

		string address;
		if (!candidates.empty()) {
			sort(candidates.begin(), candidates.end());
			address = policy == "random" ? candidates[GetRandom(static_cast<int>(candidates.size()))] : candidates.front();
		}
		else {
			debugf("Warning: record_object: no %s of kind \"%s\" found.\n", object.c_str(), kind.c_str());
		}

		string key;
		VARIABLE_SCOPE scope = ResolveVariableName(record->GetVariable(), key);
		Container* target = scope == VARIABLE_SCOPE_SELF ? context.self
			: scope == VARIABLE_SCOPE_GLOBAL ? context.global : nullptr;
		if (target) target->SetValue(key, ValueType(address));
		else debugf("Warning: record_object cannot write read-only variable %s.\n", record->GetVariable().c_str());
		return true;
	}
	if (auto* registerDoor = dynamic_cast<const RegisterDoorChange*>(change)) {
		// pass是布尔表达式，留空时EvaluateExpressionBool按true处理(允许通过)。
		Door* target = FindDoorByName(ToString(EvaluateExpression(registerDoor->GetName(), context)));
		if (target) {
			target->Register(ToString(EvaluateExpression(registerDoor->GetCitizen(), context)),
				EvaluateExpressionBool(registerDoor->GetPass(), context));
		}
		return true;
	}
	return false;
}
