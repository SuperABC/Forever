#include "map.h"

#include "common/config.h"
#include "common/utility.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
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

	// ZoneMod/BuildingMod::Assign()一次扫完全地图lot、通过PlacementEmitFunc回调把想要的
	// 显式占位请求交回来——这个函数体编译在Core这一侧，mod调它触发的push_back用的是Core自己
	// 的分配器，不会出现"mod分配、Core释放"的跨DLL问题，详见map.md"寻址"一节。
	void EmitPlacementRequest(void* context, const LotPlacementRequest& request) {
		static_cast<vector<LotPlacementRequest>*>(context)->push_back(request);
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

	for (auto& [name, z] : zones) delete z;
	for (auto& [name, b] : buildings) delete b;
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

	// 还有一类端点既不是RoadJunction也不是extern：某个mod把一条路自己拆成了好几段独立Road
	// (比如JingRoadnet的隧道引道/下坡/隧道内平路三段式，见roadnet_basic.md"隧道"一节)，中间
	// 的分段点(flatNode/splitNode)只是几何过渡、并不是真正的路口——不能把它们登记成
	// Intersection去走RoadJunction::Build那一套(会按setback裁剪+摆一个强制水平的路口平面，
	// PIE验证发现斜坡中间生出一个路口平面，渲染完全不对：路口本来就不该出现在斜坡上)。但每条
	// 车道/人行道仍然要按真实宽度摆开自己的锚点——不能像"没有真正分叉"的extern端点那样退化成
	// 所有车道共用一个点：这类分段点两端车道数/宽度配置完全一致(前后两段Road用同一套
	// configureLanesEx参数)，只是几何上直接续接，没有理由让车道在这里挤到一起。
	// passthroughAnchorCache按(端点id,车行/行人,side,laneIndex)缓存已经现算出来的锚点——
	// 前一段Road在这个端点的"终点锚点"和后一段Road在这个端点的"起点锚点"用的是同一个端点id、
	// 同一套side/laneIndex，第二次请求直接命中缓存，两条贯通线因此接到同一个Node*上；两段路
	// 在分段点处方向连续(引道/S形曲线的切线在flatNode/splitNode处完全一致，见roadnet_basic.md
	// "隧道"一节addControls的说明)，所以用哪一段路现算都是同一个结果，缓存本身只是为了保证
	// 两次现算返回的是同一个Node*，不是为了避免重复计算。key用位运算手工压缩(节点id实际规模
	// 远小于2^47，不会溢出)，避免为了一个4维小缓存另外引入<map>/<tuple>依赖。
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
		float perp0X = tdy / tlen, perp0Y = -tdx / tlen;

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

	// 一个本体独占一个mod实例：Distribute()/explicitPlacements改成static Assign()，一次调用
	// 扫完全地图的lot拿到这个类型想要的所有显式占位请求(不存在任何实例)，逐条尝试
	// RequestPlacement，只有真的成功了才CreateZone一次——不会再出现"构造了一个mod实例结果这块
	// 地不要了、白白析构"的情况，因为问的过程完全不需要实例。这样mod实例的所有数据(walls/gates/
	// internalBuildings等)从始至终只属于一个Zone，不需要再另外拷贝一份到Zone自己身上，
	// Map::InitBuildings()要用的时候直接问zone->GetMod()就行。
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
				ConnectPathRoad(allLinks[i]);
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
			zone->Layout(request.direction); // 内部自己调mod->Layout(...)，填好walls/gates/
				// 内部道路/内部建筑——放在SetPosition/SetBoundaryRoad之后调用

			// 出入口接图 + 内部道路：同一个zone在这次调用期间共用一份anchorCache，
			// 让内部道路端点能复用出入口已经建好的zone侧锚点(坐标+类别重合就是同一个点)。
			// 围墙/大门(mod->walls/mod->gates)不需要在这里搬运——Zone::GetWalls()/
			// GetGates()直接转发zone自己持有的这个mod，纯数据搬运不做任何几何/导航图
			// 计算，渲染细节全部下放到Forever层(ForeverZoneFrameworkComponent)。
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

			// 内部建筑不能在这里实例化——PlaceZoneInternalBuilding要new
			// Building(&buildingFactory, spec.type)，但buildingFactory的mod注册在
			// InitBuildings()里才做(InitBuildings()必须在InitZones()之后跑，要用到这里
			// 裁剪完的剩余空闲面积)，这时候buildingFactory还是空的，CreateBuilding会
			// 返回nullptr导致Building构造函数抛异常崩溃(PIE验证发现)。这次改成
			// InitBuildings()里遍历zones、直接读zone->GetMod()->internalBuildings，
			// 不需要Zone另外存一份"待实例化"的副本。

			if (!AddZone(zone)) {
				delete zone; // ~Zone()里factory->DestroyZone(mod)会跟着跑，重名时不留悬空引用
			}
		}
	}
}

void Map::InitBuildings() {
	vector<string> mods = Config::GetMods();
	buildingFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("building_mods")));
	modLoader.RegisterConcept<BuildingFactory>(mods, "RegisterModBuildings", "FinishModBuildings", &buildingFactory);

	PathLaneSpec pathSpec;

	// 显式占位：先用static Assign一次性扫完全部lot拿到这个类型想要的所有placement请求(不存在
	// 任何实例)，再逐条尝试RequestPlacement，只有真的成功了才CreateBuilding一次——不会再出现
	// "new了一个mod实例结果这块lot根本不要、白白构造又销毁"的情况，因为问的过程完全不需要实例。
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
				ConnectPathRoad(allLinks[i]);
			}
			if (!success) continue;

			BuildingMod* mod = buildingFactory.CreateBuilding(id); // 到这里才真正创建，唯一一次
			if (!mod) continue;
			Building* building = new Building(&buildingFactory, mod);
			building->SetPosition(placedQuad.GetPosX(), placedQuad.GetPosY(),
				placedQuad.GetSizeX(), placedQuad.GetSizeY());
			building->SetParentLot(lot);
			for (auto& [dir, road] : boundaryRoads) {
				building->SetBoundaryRoad(dir, road);
			}
			building->Layout(request.direction); // 显式占位有真实direction；内部自己调
				// mod->Layout(...)+解析footprint/楼层/lodMaterial
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
			building->Layout(spec.direction); // 用spec自己声明的朝向；
				// PlaceZoneInternalBuilding内部已经SetPosition/SetBoundaryRoad完
			if (AddBuilding(building)) {
				zone->AddInternalBuilding(building);
			} else {
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
			ConnectPathRoad(allLinks[i]);
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
			building->Layout(-1); // 权重CDF/FillRemainder落地，没有direction概念，传-1
			if (!AddBuilding(building)) delete building;
		}

		lot->ClearCandidates();
	}

	// 不再需要"scanners"表和函数末尾的统一销毁——每个mod实例的生命周期现在完全绑定它独占的
	// Building，跟着~Building()一起销毁。
}

const unordered_map<string, Zone*>& Map::GetZones() const {
	return zones;
}

const unordered_map<string, Building*>& Map::GetBuildings() const {
	return buildings;
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
			if (building && building->GetParentLot() == lot && !building->GetParentZone()
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

	int idx = (isVehicle ? 0 : 2) + side;
	vector<ThroughLine>& lines = throughLines[road][idx];
	ThroughLine* line = nullptr;
	for (ThroughLine& l : lines) {
		if (l.laneIndex == laneIndex) { line = &l; break; }
	}
	if (!line || !line->fromAnchor || !line->toAnchor) return nullptr;

	Node* newNode = new Node(isVehicle ? "vehicle" : "pedestrian", worldX, worldY, worldZ);
	navAnchorNodes.push_back(newNode);

	auto& graph = isVehicle ? vehicleNavGraph : pedestrianNavGraph;

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

	// 回写成"剩下还没断过的尾巴"：下次再断同一条车道(比如同一条临街大路被沿线好几条小路
	// 连续断开)，断的是N->原toAnchor这一截，不会和这次新插入的node脱节。
	line->fromAnchor = newNode;
	line->edge = seg2;

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
	float perp0X = tdy / tlen, perp0Y = -tdx / tlen;

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

	// 小路自身固定的perp0——**必须用Road自己Start->End方向算，不能用上面的dirX/dirY**：
	// dirX/dirY是"离开当前这一端、伸向小路另一端"的方向，在Start端和End端正好相反，如果拿它
	// 算perp0，side0/side1在两端会对应到物理上相反的两侧(西端的"+"是南侧，东端的"+"却是北侧)，
	// 连线时(ConnectPathRoad拿两端同一个下标拼成一条line)就会拧成交叉——西端南侧车道接到了
	// 东端北侧车道，PIE验证发现的bug。side0/side1的物理含义必须在小路全长上保持一致，
	// 和Road自己车道数据(GetVehicleLanes(0)/(1)，SplitWithPath建小路时就是用Start->End
	// 方向的perp0铺的)对齐，因此这里固定用path->GetStart()->GetEnd()方向，不随isStartEnd翻转。
	Node pathStart = path->GetStart();
	Node pathEnd = path->GetEnd();
	float gdx = pathEnd.GetX() - pathStart.GetX();
	float gdy = pathEnd.GetY() - pathStart.GetY();
	float glen = sqrtf(gdx * gdx + gdy * gdy);
	if (glen < 1e-6f) glen = 1.f;
	float pathPerp0X = gdy / glen, pathPerp0Y = -gdx / glen;

	// 小路side0沿Start->End走，side1沿End->Start走：Start端离开(entering)的是side0、
	// 到达(exiting)的是side1；End端相反。
	int enteringSide = isStartEnd ? 0 : 1;
	int exitingSide = isStartEnd ? 1 : 0;

	// **修复"车道/人行道都堆在中轴线上"的根因**：下面host分支里Nin/Nout(以及PnA/PnB等)如果
	// 直接用ComputeLaneAnchorPosition算出来的同一个(vx,vy)断两次，会在完全相同的坐标上产生
	// 两个不同的node——views上看不出区别，ConnectPathRoad拿它们连成的小路自己的side0/side1
	// 贯通线(veh0=start[0]->end[0]，veh1=end[1]->start[1])于是首尾都落在同一对坐标上，
	// 几何上重合成一条线，视觉上就是"只有中轴线一条车道"。这里在每个host断点上，沿小路自身
	// 的pathPerp0方向按小路自己的车道/人行道半宽各偏移一点，让entering/exiting(或near的
	// side0/side1)两个断点分别落在小路两条车道各自的延长线上，和小路自身中段的车道几何真正
	// 对齐，不再是同一个点。enterSign统一控制"entering那一侧偏移量的符号"，exiting/side1
	// 自动取反，两者始终关于host断点对称。
	float enterSign = (enteringSide == 0) ? 1.f : -1.f;

	if (!hostRoad) {
		outVeh[0] = MakeIsolatedAnchor(baseNode.GetX() + pathPerp0X * vehOffset, baseNode.GetY() + pathPerp0Y * vehOffset, baseNode.GetZ(), "vehicle");
		outVeh[1] = MakeIsolatedAnchor(baseNode.GetX() - pathPerp0X * vehOffset, baseNode.GetY() - pathPerp0Y * vehOffset, baseNode.GetZ(), "vehicle");
		outPed[0] = MakeIsolatedAnchor(baseNode.GetX() + pathPerp0X * pedOffset, baseNode.GetY() + pathPerp0Y * pedOffset, baseNode.GetZ(), "pedestrian");
		outPed[1] = MakeIsolatedAnchor(baseNode.GetX() - pathPerp0X * pedOffset, baseNode.GetY() - pathPerp0Y * pedOffset, baseNode.GetZ(), "pedestrian");
		return;
	}

	Node hostPoint = hostRoad->GetPoint(hostT);
	float hdx, hdy, hdz;
	hostRoad->GetTangent(hostT, hdx, hdy, hdz);
	float hlen = sqrtf(hdx * hdx + hdy * hdy);
	if (hlen < 1e-6f) hlen = 1.f;
	float hostFwdX = hdx / hlen, hostFwdY = hdy / hlen;
	float hostPerp0X = hostFwdY, hostPerp0Y = -hostFwdX;

	// 近侧判定：小路离开连接点、伸向自己另一端的方向，和host的perp0点积>=0就是host的side0，
	// 否则side1——小路总是往它所属Lot的空闲空间那一侧延伸，这个方向天然指向近侧所在的半边。
	float dot = dirX * hostPerp0X + dirY * hostPerp0Y;
	int nearSide = (dot >= 0.f) ? 0 : 1;
	int farSide = 1 - nearSide;

	auto pickLaneIndex = [](const vector<float>& lanes) -> int {
		return lanes.empty() ? -1 : static_cast<int>(lanes.size()) - 1;
		};
	// 单行道(近侧车道数组本身就是空的，比如"单行道双车道"只在farSide铺了2条车道)退化去用
	// farSide时，要选**最靠近路中心线**的那条(下标0)，不是outermost(下标size()-1)——outermost
	// 是给"这一侧本身就是near"的情况用的("离小路最近"=离curb最近=离中心线最远)，但退化到
	// farSide时小路其实紧贴着中心线这一侧(near侧车道数为空)，farSide车道里离小路最近的反而是
	// 下标0那条(挨着中心线)，不是下标size()-1那条(farSide自己的outermost、离小路最远那条)。
	// 原先两种情况都用pickLaneIndex(outermost)，导致所有退化到同一farSide的小路(不管near侧
	// 本来该是side0还是side1)全部挤到farSide同一条outermost车道上，就是"都连接到同一侧车道"
	// 这个bug。
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
				vx + pathPerp0X * vehOffset * enterSign, vy + pathPerp0Y * vehOffset * enterSign, hostPoint.GetZ());
			Nout = BreakThroughLine(hostRoad, true, vehSide, vehLaneIndex,
				vx - pathPerp0X * vehOffset * enterSign, vy - pathPerp0Y * vehOffset * enterSign, hostPoint.GetZ());
		}
		if (Nin && Nout) {
			outVeh[enteringSide] = Nin;
			outVeh[exitingSide] = Nout;
		}
		else {
			outVeh[0] = MakeIsolatedAnchor(hostPoint.GetX() + pathPerp0X * vehOffset, hostPoint.GetY() + pathPerp0Y * vehOffset, hostPoint.GetZ(), "vehicle");
			outVeh[1] = MakeIsolatedAnchor(hostPoint.GetX() - pathPerp0X * vehOffset, hostPoint.GetY() - pathPerp0Y * vehOffset, hostPoint.GetZ(), "vehicle");
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
				px + pathPerp0X * pedOffset, py + pathPerp0Y * pedOffset, hostPoint.GetZ());
			Pb = BreakThroughLine(hostRoad, false, pedSide, pedLaneIndex,
				px - pathPerp0X * pedOffset, py - pathPerp0Y * pedOffset, hostPoint.GetZ());
		}
		if (Pa && Pb) {
			outPed[0] = Pa;
			outPed[1] = Pb;
		}
		else {
			outPed[0] = MakeIsolatedAnchor(hostPoint.GetX() + pathPerp0X * pedOffset, hostPoint.GetY() + pathPerp0Y * pedOffset, hostPoint.GetZ(), "pedestrian");
			outPed[1] = MakeIsolatedAnchor(hostPoint.GetX() - pathPerp0X * pedOffset, hostPoint.GetY() - pathPerp0Y * pedOffset, hostPoint.GetZ(), "pedestrian");
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
				nvx + pathPerp0X * vehOffset * enterSign, nvy + pathPerp0Y * vehOffset * enterSign, hostPoint.GetZ());
			NnOut = BreakThroughLine(hostRoad, true, nearSide, nearVehLaneIndex,
				nvx - pathPerp0X * vehOffset * enterSign, nvy - pathPerp0Y * vehOffset * enterSign, hostPoint.GetZ());
		}
		if (farVehLaneIndex >= 0) {
			auto [fvx, fvy] = ComputeLaneAnchorPosition(hostRoad, hostT, true, farSide, farVehLaneIndex);
			Node* NfIn = BreakThroughLine(hostRoad, true, farSide, farVehLaneIndex,
				fvx + pathPerp0X * vehOffset * enterSign, fvy + pathPerp0Y * vehOffset * enterSign, hostPoint.GetZ());
			Node* NfOut = BreakThroughLine(hostRoad, true, farSide, farVehLaneIndex,
				fvx - pathPerp0X * vehOffset * enterSign, fvy - pathPerp0Y * vehOffset * enterSign, hostPoint.GetZ());
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
			outVeh[0] = MakeIsolatedAnchor(hostPoint.GetX() + pathPerp0X * vehOffset, hostPoint.GetY() + pathPerp0Y * vehOffset, hostPoint.GetZ(), "vehicle");
			outVeh[1] = MakeIsolatedAnchor(hostPoint.GetX() - pathPerp0X * vehOffset, hostPoint.GetY() - pathPerp0Y * vehOffset, hostPoint.GetZ(), "vehicle");
		}

		int nearPedLaneIndex = pickLaneIndex(hostRoad->GetPedestrianLanes(nearSide));
		int farPedLaneIndex = pickLaneIndex(hostRoad->GetPedestrianLanes(farSide));

		Node* PnA = nullptr;
		Node* PnB = nullptr;
		if (nearPedLaneIndex >= 0) {
			auto [npx, npy] = ComputeLaneAnchorPosition(hostRoad, hostT, false, nearSide, nearPedLaneIndex);
			PnA = BreakThroughLine(hostRoad, false, nearSide, nearPedLaneIndex,
				npx + pathPerp0X * pedOffset, npy + pathPerp0Y * pedOffset, hostPoint.GetZ());
			PnB = BreakThroughLine(hostRoad, false, nearSide, nearPedLaneIndex,
				npx - pathPerp0X * pedOffset, npy - pathPerp0Y * pedOffset, hostPoint.GetZ());
		}
		if (farPedLaneIndex >= 0 && PnA && PnB) {
			auto [fpx, fpy] = ComputeLaneAnchorPosition(hostRoad, hostT, false, farSide, farPedLaneIndex);
			Node* PfA = BreakThroughLine(hostRoad, false, farSide, farPedLaneIndex,
				fpx + pathPerp0X * pedOffset, fpy + pathPerp0Y * pedOffset, hostPoint.GetZ());
			Node* PfB = BreakThroughLine(hostRoad, false, farSide, farPedLaneIndex,
				fpx - pathPerp0X * pedOffset, fpy - pathPerp0Y * pedOffset, hostPoint.GetZ());
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
			outPed[0] = MakeIsolatedAnchor(hostPoint.GetX() + pathPerp0X * pedOffset, hostPoint.GetY() + pathPerp0Y * pedOffset, hostPoint.GetZ(), "pedestrian");
			outPed[1] = MakeIsolatedAnchor(hostPoint.GetX() - pathPerp0X * pedOffset, hostPoint.GetY() - pathPerp0Y * pedOffset, hostPoint.GetZ(), "pedestrian");
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
	throughLines[path][0].push_back({ veh0, startVeh[0], endVeh[0], 0 });

	Connection* veh1 = new Connection(*endVeh[1], *startVeh[1]);
	vehicleNavGraph[endVeh[1]->GetId()].emplace_back(startVeh[1]->GetId(), veh1);
	throughLines[path][1].push_back({ veh1, endVeh[1], startVeh[1], 0 });

	Connection* ped0 = new Connection(*startPed[0], *endPed[0]);
	pedestrianNavGraph[startPed[0]->GetId()].emplace_back(endPed[0]->GetId(), ped0);
	pedestrianNavGraph[endPed[0]->GetId()].emplace_back(startPed[0]->GetId(), ped0);
	throughLines[path][2].push_back({ ped0, startPed[0], endPed[0], 0 });

	Connection* ped1 = new Connection(*startPed[1], *endPed[1]);
	pedestrianNavGraph[startPed[1]->GetId()].emplace_back(endPed[1]->GetId(), ped1);
	pedestrianNavGraph[endPed[1]->GetId()].emplace_back(startPed[1]->GetId(), ped1);
	throughLines[path][3].push_back({ ped1, startPed[1], endPed[1], 0 });
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

	// 直线投影近似算hostRoad上的弧长比例t，和geometry.cpp里ProjectT同样的做法。
	Node hostStart = hostRoad->GetStart();
	Node hostEnd = hostRoad->GetEnd();
	float hdx = hostEnd.GetX() - hostStart.GetX();
	float hdy = hostEnd.GetY() - hostStart.GetY();
	float hLenSq = hdx * hdx + hdy * hdy;
	float t = (hLenSq < 1e-9f) ? 0.f
		: max(0.f, min(1.f, ((worldX - hostStart.GetX()) * hdx + (worldY - hostStart.GetY()) * hdy) / hLenSq));

	Node hostPoint = hostRoad->GetPoint(t);
	float tdx, tdy, tdz;
	hostRoad->GetTangent(t, tdx, tdy, tdz);
	float tLen = sqrtf(tdx * tdx + tdy * tdy);
	if (tLen < 1e-6f) tLen = 1.f;
	float hostPerp0X = tdy / tLen, hostPerp0Y = -tdx / tLen;

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

	// 端点世界坐标+类别(车行/行人)在ZONE_ANCHOR_MERGE_RADIUS容差内重合就复用anchorCache里
	// 已有的node(可能是出入口锚点，也可能是同一个zone里另一条内部道路的端点)，否则新建孤立
	// 锚点并登记进cache。容差取1个地图单位——比车道偏移量级(0.15~0.4)大得多，保证出入口
	// 锚点(落在zone声明的原始坐标上，没有车道偏移)和下面按ComputeLaneAnchorPosition算出来的
	// 车道锚点(带偏移)还能被判定成"同一个位置"自动桥接；比zone自身尺寸(测试场景约14个单位)
	// 小得多，不会误合并本不相关的两个点。
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

	// 锚点位置必须用ComputeLaneAnchorPosition算(t=0/1对应Start/End，side固定0，和
	// InitRoadnet/ResolvePathEndAnchors同一套公式)，不能直接用road两端裸的中轴线坐标——单一
	// 车道时这个公式算出来的偏移正好是0(shift和offsetDist抵消，见实现)，等价于直接用中轴线，
	// 但保留这个调用是为了和大路/小路的锚点算法保持同一套写法，不需要因为"只有一条车道"另开
	// 一套特例公式。
	if (spec.isVehicle) {
		auto [fx, fy] = ComputeLaneAnchorPosition(road, 0.f, true, 0, 0);
		auto [tx, ty] = ComputeLaneAnchorPosition(road, 1.f, true, 0, 0);
		Node* fromAnchor = findOrCreate(fx, fy, true);
		Node* toAnchor = findOrCreate(tx, ty, true);
		Connection* edge = new Connection(*fromAnchor, *toAnchor);
		vehicleNavGraph[fromAnchor->GetId()].emplace_back(toAnchor->GetId(), edge);
		throughLines[road][0].push_back({ edge, fromAnchor, toAnchor, 0 });
	}
	else {
		auto [sx, sy] = ComputeLaneAnchorPosition(road, 0.f, false, 0, 0);
		auto [ex, ey] = ComputeLaneAnchorPosition(road, 1.f, false, 0, 0);
		Node* startAnchor = findOrCreate(sx, sy, false);
		Node* endAnchor = findOrCreate(ex, ey, false);
		Connection* edge = new Connection(*startAnchor, *endAnchor);
		pedestrianNavGraph[startAnchor->GetId()].emplace_back(endAnchor->GetId(), edge);
		pedestrianNavGraph[endAnchor->GetId()].emplace_back(startAnchor->GetId(), edge);
		throughLines[road][2].push_back({ edge, startAnchor, endAnchor, 0 });
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
