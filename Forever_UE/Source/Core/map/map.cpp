#include "map.h"

#include "common/utility.h"

#include "common/config.h"
#include "common/registry.h"
#include "map/room.h"
#include "populace/populace.h"
#include "populace/citizen.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <queue>
#include <sstream>
#include <unordered_set>


using namespace std;

namespace {
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
	terrainFactory(Registry::Get().GetTerrainFactory()),
	roadnetFactory(Registry::Get().GetRoadnetFactory()),
	zoneFactory(Registry::Get().GetZoneFactory()),
	buildingFactory(Registry::Get().GetBuildingFactory()),
	roomFactory(Registry::Get().GetRoomFactory()),
	componentFactory(Registry::Get().GetComponentFactory()),
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
		float hostPerp0X = hdy / hlen, hostPerp0Y = -hdx / hlen;

		float dot = dirX * hostPerp0X + dirY * hostPerp0Y;
		return (dot >= 0.f) ? 0 : 1;
	}
}

void Map::InitZones() {
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
	buildingLayoutLibrary.ReadTemplates(Config::GetLayouts());

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
				pendingPathRoadLinks.push_back(allLinks[i]);
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
			BuildingNavResult navResult;
			building->Layout(request.direction, buildingLayoutLibrary, roomFactory, componentFactory, navResult);
			// 显式占位有真实direction；内部自己调mod->Layout(...)+解析footprint/楼层/
			// lodMaterial+实例化楼层/房间/组合+构建行人内部导航图
			MergeBuildingNavigation(building, navResult);
			ForwardBuildingHatches(building);
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
			// 用spec自己声明的朝向；PlaceZoneInternalBuilding内部已经SetPosition/
			// SetBoundaryRoad完
			MergeBuildingNavigation(building, navResult);
			ForwardBuildingHatches(building);
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
			// 权重CDF/FillRemainder落地，没有direction概念，传-1(mod自己可能兜底选一个
			// 真实方向，见building_mod.h)
			MergeBuildingNavigation(building, navResult);
			ForwardBuildingHatches(building);
			if (!AddBuilding(building)) delete building;
		}

		lot->ClearCandidates();
	}

	// 三段落地循环全部跑完、这次InitZones()+InitBuildings()涉及到的所有PathRoadLink都已经
	// 收进pendingPathRoadLinks之后，才统一按物理顺序真正接入道路网——同样不能在循环内部
	// 就地调用ConnectPathRoad，见FlushPendingPathRoadLinks()注释。必须先于
	// FlushPendingBuildingRoadAccess()：后者依赖的BreakThroughLine一样会读/改
	// throughLines当前的"剩余尾巴"状态，小路先把自己那部分接好，语义上更接近原本内联调用
	// ConnectPathRoad时的相对顺序(小路在同一次循环体内先于building的导航合并发生)。
	FlushPendingPathRoadLinks();

	// 三段落地循环全部跑完、这次InitBuildings()涉及到的所有building的outside端点都已经
	// 收进pendingBuildingRoadAccess之后，才统一按物理顺序真正断开道路网——不能在上面任何
	// 一段循环内部就地调用，见FlushPendingBuildingRoadAccess()注释。
	FlushPendingBuildingRoadAccess();

	// 不再需要"scanners"表和函数末尾的统一销毁——每个mod实例的生命周期现在完全绑定它独占的
	// Building，跟着~Building()一起销毁。
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
	// 车行图不能照抄行人"两个方向都插入"——车道本来就有方向性，这里只按Connection自己的
	// Start->End方向单向插入，方向由Building::BuildVehicleNavigation()按模板数据(upstair/
	// downstair贪心匹配、或line/node连接顺序)算好，Map不重新判断。
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
		float perp0X = tdy / tlen, perp0Y = -tdx / tlen;
		float dx = outsideNode->GetX() - basePoint.GetX(), dy = outsideNode->GetY() - basePoint.GetY();
		// building在Road哪一侧：比较building中心相对Road中心线在t处的法向偏移符号
		// (和RoadJunction::Build/resolveAnchor同一套perp0=右手垂线约定，见roadnet.md
		// "车道居中"一节)——偏移同号(>=0)就是side0(右手边)，useForwardSide=true。
		outUseForwardSide = (dx * perp0X + dy * perp0Y) >= 0.f;
		};

	for (Node* outsideNode : result.outsideNodes) {
		// 不在这里push进navAnchorNodes——outsideNode本来就是resolveEndpoint()解出的某个
		// "node"/"line"锚点或Room自己的导航节点，这几种节点在Building::BuildPedestrianNavigation()
		// 里创建/收集时已经无条件push进了navOut.nodes(newNodes)，上面的result.nodes循环
		// 已经登记过一次；这里如果再push一次，同一个Node*就会在navAnchorNodes里出现两次，
		// ~Map()清理时对它delete两次——退出游戏崩溃的根因(PIE验证发现，double free)。
		if (!road) continue; // building没有可用的边界Road(mod没能兜底选出方向)，这个"outside"
			// 端点只留在navOut.nodes里(已经在上面登记过)，不连道路网，见building.md"行人导航"一节。

		float t; bool useForwardSide;
		computeRoadSide(outsideNode, t, useForwardSide);

		// 不在这里立即AddRoadAccessNode——同一条路上可能有好几栋building各自贡献一个
		// outside端点，真正断开的先后顺序必须按它们在road上的物理位置(t)排列，不能是这里
		// 处理building的顺序(哪个building先跑到这一步纯粹取决于三段落地循环各自的遍历
		// 顺序，和物理位置无关)，否则BreakThroughLine"每次都断当前剩余尾巴"这个假设会被
		// 打破，见FlushPendingBuildingRoadAccess()注释。这里只记下来，交给InitBuildings()
		// 最后统一调用FlushPendingBuildingRoadAccess()处理。
		pendingBuildingRoadAccess.push_back({ road, t, useForwardSide, outsideNode, false, false });
	}

	// 车辆outside端点：方向(入口/出口)按这个node在result.vehicleConnections里的入度/出度
	// 判定(用户明确要求的规则)——入度为0(只有出边)是出口，车辆从building开到路上，最终连接
	// 方向"outsideNode->access node"；出度为0(只有入边)是入口，车辆从路开进building，方向
	// "access node->outsideNode"；入度出度都不为0(这个node在building内部图里本来就双向都
	// 在用)或都为0(孤立点)则拒绝接路网，不产生pending项(仍然通过上面result.vehicleNodes
	// 那次循环留在navAnchorNodes里)。
	for (Node* outsideNode : result.vehicleOutsideNodes) {
		if (!road) continue;

		int inDegree = 0, outDegree = 0;
		for (Connection* conn : result.vehicleConnections) {
			if (conn->GetEnd().GetId() == outsideNode->GetId()) inDegree++;
			if (conn->GetStart().GetId() == outsideNode->GetId()) outDegree++;
		}
		bool isExit = (inDegree == 0 && outDegree > 0);
		bool isEntrance = (outDegree == 0 && inDegree > 0);
		if (!isExit && !isEntrance) continue; // 都不为0(内部双向都用) 或 都为0(孤立点)，拒绝

		float t; bool useForwardSide;
		computeRoadSide(outsideNode, t, useForwardSide);
		pendingBuildingRoadAccess.push_back({ road, t, useForwardSide, outsideNode, true, isExit });
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

void Map::FlushPendingBuildingRoadAccess() {
	struct Entry {
		bool isVehicle;
		int side;
		int laneIndex;
		float t;
		Node* outsideNode;
		bool isExit; // 仅isVehicle==true时有意义
	};
	unordered_map<Road*, vector<Entry>> grouped;
	for (const PendingRoadAccess& pending : pendingBuildingRoadAccess) {
		int side, laneIndex;
		if (!ResolveAccessLane(pending.road, pending.isVehicle, pending.useForwardSide, side, laneIndex)) continue;
		grouped[pending.road].push_back({ pending.isVehicle, side, laneIndex, pending.t, pending.outsideNode, pending.isExit });
	}
	pendingBuildingRoadAccess.clear();

	constexpr float kAccessOpeningWidth = 1.f; // 门洞大致宽度(地图单位)，供Forever层挖空
		// 对应长度的人行道/车行道，这次先用一个固定值，不按具体门的开口宽度精细对应。
	for (auto& [road, entries] : grouped) {
		// 同一条车道(同isVehicle同side同laneIndex)内部按它自己的实际通行方向排序：side0沿
		// Start->End(t升序)，side1沿End->Start(t降序)——和ThroughLine/BreakThroughLine
		// "fromAnchor每次都往通行方向前进"的约定对齐，见map.h的FlushPendingBuildingRoadAccess()
		// 注释。isVehicle作为第一优先级排序键，保证车行/人行两类entry不会被打乱顺序地混在一起
		// 处理(虽然两者本来就走各自独立的ThroughLine数组、混着处理也不会互相污染状态，这里
		// 单纯是为了同一辆road的日志/调试顺序更直观)。
		sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
			if (a.isVehicle != b.isVehicle) return a.isVehicle < b.isVehicle;
			if (a.side != b.side) return a.side < b.side;
			if (a.laneIndex != b.laneIndex) return a.laneIndex < b.laneIndex;
			return a.side == 0 ? (a.t < b.t) : (a.t > b.t);
			});

		for (Entry& entry : entries) {
			Node* accessNode = AddRoadAccessNode(road, entry.t, entry.isVehicle,
				entry.side == 0, kAccessOpeningWidth);
			if (!accessNode) continue; // 这条Road两侧都没有对应类别车道，没法接，outsideNode保持孤立。

			if (!entry.isVehicle) {
				Connection* conn = new Connection(*entry.outsideNode, *accessNode);
				pedestrianNavGraph[entry.outsideNode->GetId()].emplace_back(accessNode->GetId(), conn);
				pedestrianNavGraph[accessNode->GetId()].emplace_back(entry.outsideNode->GetId(), conn);
				continue;
			}

			// 车行单向：isExit(入度为0，用户确认的判定规则)方向"outsideNode->accessNode"
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

	// 一条link同时占两个touch(可能落在两条不同的host road上)，两端必须在同一次
	// ConnectPathRoad调用里处理，不能像building access那样把每个touch当独立工作项直接
	// 排序——改用Kahn拓扑排序：每个分组内相邻两个touch形成一条"前者所属link必须先处理"的
	// 依赖边，综合所有分组算出link之间的全局处理顺序。理论上只有很反常的路网几何才会在
	// 多条host道路之间形成排序环，出现环时剩余部分退化成按原始发现顺序处理，不阻塞整个
	// 流程(不会崩溃，只是环内那几条link之间仍可能有本文件描述的局部绕路，属已知兜底简化)。
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
	for (auto& [name, building] : buildings) {
		if (!building) continue;
		for (Room* room : building->GetRooms()) {
			if (room && room->IsResidential()) {
				capacity += room->ResidentialCapacity();
			}
		}
	}
	return capacity / 2;
}

void Map::Checkin(const Populace& populace) {
	constexpr int kAdultAge = 18; // 照抄老工程ADULT_AGE
	constexpr int kProbabilityScale = 100; // 照抄老工程PROBABILITY_SCALE
	constexpr int kZoneOwnershipChance = 2; // 照抄老工程ZONE_OWNERSHIP_CHANCE
	constexpr int kBuildingOwnershipChance = 5; // 照抄老工程BUILDING_OWNERSHIP_CHANCE
	// 老工程"公有(stated)"这条分支只在某个zone/building已经被别的机制预先标记时才会
	// 触发(GetStated()是纯查询，老工程Map::Checkin本身从没有随机骰出过stated——搜了
	// 整个老工程也没有任何mod会在初始化时SetStated(true))，等于老工程自带的内容里这条
	// 分支从来不会被撞上。这次要求"保留公有资产的逻辑"，所以自己加两个小概率，让公有
	// 真的能在随机分配里出现——具体数值是这次新定的，不是老工程的值，量级上比照
	// ownership chance给，可以按需调。
	constexpr int kZoneStatedChance = 2;
	constexpr int kBuildingStatedChance = 3;

	int currentYear = populace.GetCurrentYear();

	vector<Citizen*> adults;
	for (Citizen* citizen : populace.GetCitizens()) {
		if (citizen && citizen->GetAge(currentYear) >= kAdultAge) {
			adults.push_back(citizen);
		}
	}
	if (adults.empty()) return;

	// 房产归属：按老工程Map::Checkin"Zone→Building→Room逐级下探"的算法分配owner/stated，
	// 每一级都是"要么整体公有，要么整体归一个citizen私有，要么下探到下一级各自独立决定"
	// 三选一，详见Source/Core/populace/populace.md"房产归属"一节。
	auto resolveBuildingOwnership = [&adults, kProbabilityScale, kBuildingStatedChance,
		kBuildingOwnershipChance](Building* building) {
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

	// 不属于任何zone的独立building各自走一遍同一套building级归属逻辑——只处理
	// GetParentZone()为空的，属于某个zone的building已经在上面的zone循环里处理过了
	// (老工程这里是无条件遍历Map::buildings，会把zone内部building的归属重新独立骰
	// 一次、覆盖掉刚设好的zone级归属，这次判定是老工程的疏漏，不逐字复刻，见populace.md)。
	for (auto& [buildingName, building] : buildings) {
		if (!building || building->GetParentZone()) continue;
		resolveBuildingOwnership(building);
	}

	// 分配住处：和"房产归属"是两个独立的关注点——一个room的owner是谁、和谁实际住在
	// 里面(tenants/occupants)完全无关(可以理解成"租房")，所以这里单独重新扫一遍所有
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

	// 人和房间的关系是3个独立概念(见room.h)：这里的"家"(GetRoom()/tenants)和"当前物理
	// 位置"(GetCurrentRoom()/occupants)对citizen来说初始状态天然重合(刚分配住处，人也
	// 就在那)，但存储上必须分开写，为将来"人在家但当前不在自己房间里"这类场景预留。
	auto moveIn = [](Citizen* citizen, Room* room) {
		Building* building = room->GetParentBuilding();
		citizen->SetRoom(room);
		citizen->SetCurrentRoom(room);
		citizen->SetBuilding(building);
		if (building) {
			citizen->SetZone(building->GetParentZone());
			citizen->SetLot(building->GetParentLot());
		}
		room->AddTenant(citizen);
		room->AddOccupant(citizen);
	};

	// 只有成年citizen各自去随机抽一个room名额（GetRandom(pool.size())+swap-remove，照抄
	// 老工程Map::Checkin同一套"一个家庭消费一个room名额"逻辑，不是"一人一间"）；配偶
	// （GetSpouse()非空且还没房间）以90%概率（GetRandom(10)>0，老工程同款）跟着搬进同一间，
	// 未成年子女（GetChildren()里年龄<18且还没房间的）无条件一起搬进同一间。未成年citizen
	// 不会独立抽房间——只能通过父母这边被带进去，和老工程"只遍历adults"效果一致。
	for (Citizen* citizen : adults) {
		if (citizen->GetRoom()) continue; // 已经被配偶那边带着分到房间了
		if (pool.empty()) break;

		int index = GetRandom(static_cast<int>(pool.size()));
		Room* room = pool[index];
		moveIn(citizen, room);

		Citizen* spouse = citizen->GetSpouse();
		if (spouse && !spouse->GetRoom() && GetRandom(10) > 0) {
			moveIn(spouse, room);
			for (Citizen* child : citizen->GetChildren()) {
				if (child && !child->GetRoom() && child->GetAge(currentYear) < kAdultAge) {
					moveIn(child, room);
				}
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

	// 单行道特殊情况：请求的side本身没有对应类别车道，但对侧有——说明这是单行道，
	// useForwardSide不再表示"正向/反向"，改按"要最靠右(true)还是最靠左(false)的车道"重新
	// 解释，实际车道从对侧取，见map.h的AddRoadAccessNode注释。两侧都没有车道就是真的没有
	// 对应类别的通行空间，返回false。
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

	outSide = side;
	outLaneIndex = laneIndex;
	return true;
}

Node* Map::AddRoadAccessNode(Road* road, float t, bool isVehicle, bool useForwardSide, float openingWidth) {
	if (!road) return nullptr;

	int side, laneIndex;
	if (!ResolveAccessLane(road, isVehicle, useForwardSide, side, laneIndex)) return nullptr;

	// 车道横断面世界坐标必须用ComputeLaneAnchorPosition算——之前这里自己内联了一份只适用于
	// 车行道的公式(offsetDist=LaneCenterOffset(lanes,laneIndex)，直接从道路中心线量)，对
	// 行人道(isVehicle=false)漏加了同侧车行道+停车道的总宽度，导致算出来的新访问点世界坐标
	// 落在车行道范围内而不是真正的人行道位置——building的outside端点接路网时第一次真正
	// 触发这条行人分支(之前从没有真实调用方)，PIE验证发现新断出的行人访问点被画在了车道上。
	// ComputeLaneAnchorPosition对isVehicle=false会先加上SumWidths(车行道)+SumWidths(停车道)
	// 再叠加人行道自己的居中偏移，和RoadJunction::Build里pedestrianSide锚点用的是同一套算法。
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

vector<const Node*> Map::FindPedestrianPath(int fromNodeId, int toNodeId) const {
	if (fromNodeId == toNodeId) {
		for (const Node* node : navAnchorNodes) {
			if (node->GetId() == fromNodeId) return { node };
		}
		for (const Node* node : GetExterns()) {
			if (node->GetId() == fromNodeId) return { node };
		}
		return {};
	}

	// id -> 拥有生命周期的Node*，供最后按id序列反查真正的指针（Dijkstra内部只按id
	// 运算，Connection::GetStart()/GetEnd()返回的是Node副本，不能直接拿它们的地址）。
	unordered_map<int, const Node*> nodesById;
	for (const Node* node : navAnchorNodes) nodesById[node->GetId()] = node;
	for (const Node* node : GetExterns()) nodesById[node->GetId()] = node;

	unordered_map<int, float> dist;
	unordered_map<int, int> prev;
	unordered_set<int> visited;
	using QueueEntry = pair<float, int>; // (distance, nodeId)，最小堆
	priority_queue<QueueEntry, vector<QueueEntry>, greater<QueueEntry>> queue;

	dist[fromNodeId] = 0.f;
	queue.push({ 0.f, fromNodeId });

	while (!queue.empty()) {
		auto [d, id] = queue.top();
		queue.pop();
		if (visited.count(id)) continue;
		visited.insert(id);
		if (id == toNodeId) break;

		auto it = pedestrianNavGraph.find(id);
		if (it == pedestrianNavGraph.end()) continue;
		for (const auto& [neighborId, connection] : it->second) {
			if (!connection || visited.count(neighborId)) continue;
			float weight = connection->CalcDistance();
			float newDist = d + weight;
			auto distIt = dist.find(neighborId);
			if (distIt == dist.end() || newDist < distIt->second) {
				dist[neighborId] = newDist;
				prev[neighborId] = id;
				queue.push({ newDist, neighborId });
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

	vector<const Node*> result;
	for (int id : idPath) {
		auto it = nodesById.find(id);
		if (it != nodesById.end()) result.push_back(it->second);
	}
	return result;
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

void Map::Tick(const Time& currentTime, bool crossedDay, PostHandle* post) {
	// 占位，等Map域真的有需要每帧处理的逻辑时再补，见map.h声明处注释。
}

void Map::ApplyChange(const Change* change, const ScriptContext& context) {
	// 占位，等Map域真的有需要处理的Change子类时再补，见map.h声明处注释。
}
