#include "map_save.h"

#include "map/map.h"
#include "map/room.h"
#include "map/component.h"
#include "map/door.h"

#include "common/save.h"
#include "common/save_refs.h"
#include "common/json.h"
#include "common/utility.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>


using namespace std;

namespace {
	// ---- 小工具：值类型/容器的读写。所有unordered_map都按key排序后写出，保证同一份数据每次
	// 写出的字节完全一致(存档→读档→再存档可以逐字节比对，见map_save.md"往返一致性")。 ----

	void WriteQuad(const Quad& q, ByteWriter& w) {
		w.F32(q.GetPosX()); w.F32(q.GetPosY()); w.F32(q.GetSizeX()); w.F32(q.GetSizeY()); w.F32(q.GetAcreage());
	}
	void ReadQuad(Quad& q, ByteReader& r) {
		float x = r.F32(), y = r.F32(), sx = r.F32(), sy = r.F32(), acreage = r.F32();
		q.SetPosition(x, y, sx, sy);
		q.SetAcreage(acreage);
	}

	template <size_t N>
	void WriteFloats(const array<float, N>& a, ByteWriter& w) { for (float v : a) w.F32(v); }
	template <size_t N>
	void ReadFloats(array<float, N>& a, ByteReader& r) { for (float& v : a) v = r.F32(); }

	void WriteFloatVector(const vector<float>& v, ByteWriter& w) {
		w.U32(static_cast<uint32_t>(v.size()));
		for (float f : v) w.F32(f);
	}
	vector<float> ReadFloatVector(ByteReader& r) {
		vector<float> v(r.U32());
		for (float& f : v) f = r.F32();
		return v;
	}

	void WriteStrings(const vector<string>& v, ByteWriter& w) {
		w.U32(static_cast<uint32_t>(v.size()));
		for (const string& s : v) w.Str(s);
	}
	vector<string> ReadStrings(ByteReader& r) {
		vector<string> v(r.U32());
		for (string& s : v) s = r.Str();
		return v;
	}

	template <typename K, typename V>
	vector<K> SortedKeys(const unordered_map<K, V>& m) {
		vector<K> keys;
		keys.reserve(m.size());
		for (auto& [k, _] : m) keys.push_back(k);
		sort(keys.begin(), keys.end());
		return keys;
	}

	void WriteWallHole(const WallHole& holes, ByteWriter& w) {
		w.U32(static_cast<uint32_t>(holes.size()));
		for (int dir : SortedKeys(holes)) {
			const vector<RectParams>& rects = holes.at(dir);
			w.I32(dir);
			w.U32(static_cast<uint32_t>(rects.size()));
			for (const RectParams& rect : rects) WriteFloats(rect, w);
		}
	}
	WallHole ReadWallHole(ByteReader& r) {
		WallHole holes;
		uint32_t n = r.U32();
		for (uint32_t i = 0; i < n && !r.Failed(); i++) {
			int dir = r.I32();
			vector<RectParams>& rects = holes[dir];
			rects.resize(r.U32());
			for (RectParams& rect : rects) ReadFloats(rect, r);
		}
		return holes;
	}

	void WriteDoorTags(const DoorTags& tags, ByteWriter& w) {
		w.U32(static_cast<uint32_t>(tags.size()));
		for (int dir : SortedKeys(tags)) {
			w.I32(dir);
			WriteStrings(tags.at(dir), w);
		}
	}
	DoorTags ReadDoorTags(ByteReader& r) {
		DoorTags tags;
		uint32_t n = r.U32();
		for (uint32_t i = 0; i < n && !r.Failed(); i++) {
			int dir = r.I32();
			tags[dir] = ReadStrings(r);
		}
		return tags;
	}

	void WriteDoorSpec(const DoorSpec& s, ByteWriter& w) {
		w.Str(s.mesh); w.Str(s.frameMesh); w.Str(s.name); w.Str(s.scriptModName);
		WriteStrings(s.milestoneNames, w);
		w.I32(s.access); w.I32(s.style); w.I32(s.leaves);
		w.F32(s.openAmount); w.F32(s.openSeconds); w.U8(s.randomSide ? 1 : 0);
	}
	DoorSpec ReadDoorSpec(ByteReader& r) {
		DoorSpec s;
		s.mesh = r.Str(); s.frameMesh = r.Str(); s.name = r.Str(); s.scriptModName = r.Str();
		s.milestoneNames = ReadStrings(r);
		s.access = r.I32(); s.style = r.I32(); s.leaves = r.I32();
		s.openAmount = r.F32(); s.openSeconds = r.F32(); s.randomSide = r.U8() != 0;
		return s;
	}

	void WriteDecorations(const vector<DecorationInstance>& v, ByteWriter& w) {
		w.U32(static_cast<uint32_t>(v.size()));
		for (const DecorationInstance& d : v) {
			w.Str(d.meshPath);
			w.F32(d.x); w.F32(d.y); w.F32(d.z);
			w.F32(d.yaw); w.F32(d.pitch); w.F32(d.roll);
			w.F32(d.scaleX); w.F32(d.scaleY); w.F32(d.scaleZ);
		}
	}
	vector<DecorationInstance> ReadDecorations(ByteReader& r) {
		vector<DecorationInstance> v(r.U32());
		for (DecorationInstance& d : v) {
			d.meshPath = r.Str();
			d.x = r.F32(); d.y = r.F32(); d.z = r.F32();
			d.yaw = r.F32(); d.pitch = r.F32(); d.roll = r.F32();
			d.scaleX = r.F32(); d.scaleY = r.F32(); d.scaleZ = r.F32();
		}
		return v;
	}

	void WriteHatches(const vector<TerrainHatch>& v, ByteWriter& w) {
		w.U32(static_cast<uint32_t>(v.size()));
		for (const TerrainHatch& hatch : v) {
			WriteQuad(hatch.quad, w);
			w.F32(hatch.rotation);
			w.U8(hatch.wall ? 1 : 0);
			w.F32(hatch.floorStart);
			w.F32(hatch.floorEnd);
		}
	}
	vector<TerrainHatch> ReadHatches(ByteReader& r) {
		vector<TerrainHatch> v(r.U32());
		for (TerrainHatch& hatch : v) {
			ReadQuad(hatch.quad, r);
			hatch.rotation = r.F32();
			hatch.wall = r.U8() != 0;
			hatch.floorStart = r.F32();
			hatch.floorEnd = r.F32();
		}
		return v;
	}

	using SaveRefs::WriteModState;
	using SaveRefs::ReadModState;

	using SaveRefs::LiveIds;
	using SaveRefs::LookupId;

	enum RoadOwner : uint8_t {
		ROAD_OWNER_ROADNET = 0,      // Roadnet::roads
		ROAD_OWNER_LOT_BOUNDARY = 1, // 只被顶层Lot的边界引用的独立拷贝(Roadnet析构时去重释放)
		ROAD_OWNER_PATH = 2,         // 小路，由持有那条PathRoadLink的Lot释放
		ROAD_OWNER_ZONE = 3,         // 园区内部道路，由Zone释放
		ROAD_OWNER_NONE = 4,         // 其它引用到的路(不应该出现)，读档后没有持有者
	};

	// 这次读档读回来的最大Node id——读档结束时宿主的Node::count要恢复成存档值，但不能小于它。
	int gMaxLoadedNodeId = -1;

	enum LotOwner : uint8_t {
		LOT_OWNER_ROADNET = 0, // Roadnet::lots顶层Lot
		LOT_OWNER_PARENT = 1,  // 父Lot的freeLots
		LOT_OWNER_ORPHAN = 2,  // 游离子块，读档后由Map::orphanLots持有
	};
}

// ======== 编号表 ========

struct MapSaveCodec::SaveTables {
	vector<const Road*> roads;
	vector<uint8_t> roadOwners;
	unordered_map<const Road*, int> roadIndex;

	vector<const Lot*> lots;
	vector<uint8_t> lotOwners;
	vector<int> lotParents;
	unordered_map<const Lot*, int> lotIndex;

	vector<const Connection*> edges;
	unordered_map<const Connection*, int> edgeIndex;

	unordered_map<int, const Node*> anchorsById;

	vector<const Zone*> zones;
	unordered_map<const Zone*, int> zoneIndex;
	vector<const Building*> buildings;
	unordered_map<const Building*, int> buildingIndex;
	unordered_map<const Room*, pair<int, int>> roomRef; // (building下标, 房间下标)
	unordered_map<const Door*, int> doorIndex;

	mutable int unresolved = 0;

	void AddRoad(const Road* road, uint8_t owner) {
		if (!road || roadIndex.count(road)) return;
		roadIndex[road] = static_cast<int>(roads.size());
		roads.push_back(road);
		roadOwners.push_back(owner);
	}
	int RoadRef(const Road* road) const {
		if (!road) return -1;
		auto it = roadIndex.find(road);
		if (it == roadIndex.end()) { unresolved++; return -1; }
		return it->second;
	}
	int LotRef(const Lot* lot) const {
		if (!lot) return -1;
		auto it = lotIndex.find(lot);
		if (it == lotIndex.end()) { unresolved++; return -1; }
		return it->second;
	}
	int EdgeRef(const Connection* edge) const {
		if (!edge) return -1;
		auto it = edgeIndex.find(edge);
		if (it == edgeIndex.end()) { unresolved++; return -1; }
		return it->second;
	}
	// 导航锚点按Node id引用：id在锚点表里必须对应同一个对象，否则读档会接到别的点上。
	int AnchorRef(const Node* node) const {
		if (!node) return -1;
		auto it = anchorsById.find(node->GetId());
		if (it == anchorsById.end() || it->second != node) { unresolved++; return -1; }
		return node->GetId();
	}
	int ZoneRef(const Zone* zone) const {
		if (!zone) return -1;
		auto it = zoneIndex.find(zone);
		return it != zoneIndex.end() ? it->second : -1;
	}
	int BuildingRef(const Building* building) const {
		if (!building) return -1;
		auto it = buildingIndex.find(building);
		return it != buildingIndex.end() ? it->second : -1;
	}
	int DoorRef(const Door* door) const {
		auto it = doorIndex.find(door);
		if (it == doorIndex.end()) { unresolved++; return -1; }
		return it->second;
	}
};

struct MapSaveCodec::LoadTables {
	vector<Road*> roads;
	vector<Lot*> lots;
	vector<Connection*> edges;
	unordered_map<int, Node*> anchorsById;
	vector<Zone*> zones;
	vector<Building*> buildings;
	vector<Door*> doors;
	int unresolved = 0;

	Road* RoadAt(int index) {
		if (index < 0) return nullptr;
		if (index >= static_cast<int>(roads.size())) { unresolved++; return nullptr; }
		return roads[index];
	}
	Lot* LotAt(int index) {
		if (index < 0) return nullptr;
		if (index >= static_cast<int>(lots.size())) { unresolved++; return nullptr; }
		return lots[index];
	}
	Connection* EdgeAt(int index) {
		if (index < 0) return nullptr;
		if (index >= static_cast<int>(edges.size())) { unresolved++; return nullptr; }
		return edges[index];
	}
	Node* AnchorAt(int id) {
		if (id < 0) return nullptr;
		auto it = anchorsById.find(id);
		if (it == anchorsById.end()) { unresolved++; return nullptr; }
		return it->second;
	}
	Zone* ZoneAt(int index) {
		return (index >= 0 && index < static_cast<int>(zones.size())) ? zones[index] : nullptr;
	}
	Building* BuildingAt(int index) {
		return (index >= 0 && index < static_cast<int>(buildings.size())) ? buildings[index] : nullptr;
	}
	Door* DoorAt(int index) {
		if (index < 0 || index >= static_cast<int>(doors.size())) { unresolved++; return nullptr; }
		return doors[index];
	}
};

// ======== 基础几何 ========

void MapSaveCodec::WriteNode(const Node& node, ByteWriter& w) {
	w.I32(node.id);
	w.Str(node.category);
	w.F32(node.posX); w.F32(node.posY); w.F32(node.posZ);
}

Node MapSaveCodec::ReadNode(ByteReader& r) {
	int id = r.I32();
	string category = r.Str();
	float x = r.F32(), y = r.F32(), z = r.F32();
	Node node(category, x, y, z);
	node.id = id;
	gMaxLoadedNodeId = max(gMaxLoadedNodeId, id);
	return node;
}

void MapSaveCodec::WriteConnection(const Connection& c, ByteWriter& w) {
	WriteNode(*c.beginVertex, w);
	WriteNode(*c.endVertex, w);
	w.F32(c.begin);
	w.F32(c.end);
	w.U32(static_cast<uint32_t>(c.controlVertices.size()));
	for (const auto& [node, weight] : c.controlVertices) {
		WriteNode(*node, w);
		w.F32(weight);
	}
}

namespace {
	vector<pair<Node, float>> ReadControls(ByteReader& r, Node (*readNode)(ByteReader&)) {
		vector<pair<Node, float>> controls;
		uint32_t n = r.U32();
		controls.reserve(n);
		for (uint32_t i = 0; i < n && !r.Failed(); i++) {
			Node node = readNode(r);
			float weight = r.F32();
			controls.emplace_back(node, weight);
		}
		return controls;
	}
}

Connection* MapSaveCodec::ReadConnection(ByteReader& r) {
	Node begin = ReadNode(r);
	Node end = ReadNode(r);
	float beginT = r.F32(), endT = r.F32();
	vector<pair<Node, float>> controls = ReadControls(r, &MapSaveCodec::ReadNode);
	Connection* c = new Connection(begin, end, beginT, endT);
	if (!controls.empty()) c->AddControls(controls);
	return c;
}

void MapSaveCodec::WriteRoad(const Road& road, ByteWriter& w) {
	w.Str(road.name);
	w.Str(road.mesh);
	w.F32(road.unit);
	WriteConnection(road, w);
	for (int side = 0; side < 2; side++) {
		WriteFloatVector(road.vehicleLanes[side], w);
		WriteFloatVector(road.parkingLanes[side], w);
		WriteFloatVector(road.pedestrianLanes[side], w);
	}
	w.U32(static_cast<uint32_t>(road.openings.size()));
	for (const RoadOpening& o : road.openings) {
		w.F32(o.t); w.F32(o.width); w.U8(o.forwardSide ? 1 : 0); w.U8(o.isVehicle ? 1 : 0);
	}
	w.U8(road.isPathRoad ? 1 : 0);
}

Road* MapSaveCodec::ReadRoad(ByteReader& r) {
	string name = r.Str();
	string mesh = r.Str();
	float unit = r.F32();
	Node begin = ReadNode(r);
	Node end = ReadNode(r);
	float beginT = r.F32(), endT = r.F32();
	vector<pair<Node, float>> controls = ReadControls(r, &MapSaveCodec::ReadNode);
	Road* road = new Road(name, begin, end, mesh, unit, beginT, endT);
	if (!controls.empty()) road->AddControls(controls);
	for (int side = 0; side < 2; side++) {
		road->vehicleLanes[side] = ReadFloatVector(r);
		road->parkingLanes[side] = ReadFloatVector(r);
		road->pedestrianLanes[side] = ReadFloatVector(r);
	}
	uint32_t openingCount = r.U32();
	for (uint32_t i = 0; i < openingCount && !r.Failed(); i++) {
		RoadOpening o;
		o.t = r.F32(); o.width = r.F32(); o.forwardSide = r.U8() != 0; o.isVehicle = r.U8() != 0;
		road->openings.push_back(o);
	}
	road->isPathRoad = r.U8() != 0;
	return road;
}

// ======== 地形 ========

void MapSaveCodec::WriteTerrain(const Map& map, ByteWriter& w) {
	w.I32(map.width);
	w.I32(map.height);

	// 地形类型名调色板(按首次出现顺序)，每个格子只存下标。
	unordered_map<string, uint32_t> paletteIndex;
	vector<string> palette;
	for (const Element& e : map.elements) {
		if (paletteIndex.emplace(e.terrain, static_cast<uint32_t>(palette.size())).second) palette.push_back(e.terrain);
	}
	WriteStrings(palette, w);

	for (const Element& e : map.elements) {
		w.U32(paletteIndex[e.terrain]);
		w.F32(e.height);
		w.U8(e.water.first ? 1 : 0);
		w.F32(e.water.second);
		WriteHatches(e.hatches, w);
		w.U8(e.fineHeights ? 1 : 0);
		if (e.fineHeights) {
			for (float v : *e.fineHeights) w.F32(v);
		}
	}

	w.U32(static_cast<uint32_t>(map.terrainTextures.size()));
	for (const string& key : SortedKeys(map.terrainTextures)) {
		const Map::TerrainTextureInfo& info = map.terrainTextures.at(key);
		w.Str(key);
		w.I32(info.slice);
		w.Str(info.diffuse); w.Str(info.normal); w.Str(info.roughness);
	}
}

bool MapSaveCodec::ReadTerrain(Map& map, ByteReader& r, string& error) {
	// 宽高已经在Load里读过、用来构造Map，这里跳过。
	r.I32();
	r.I32();
	vector<string> palette = ReadStrings(r);
	for (Element& e : map.elements) {
		uint32_t index = r.U32();
		if (r.Failed()) break;
		if (index >= palette.size()) {
			error = "terrain palette index out of range";
			return false;
		}
		e.terrain = palette[index];
		e.height = r.F32();
		e.water.first = r.U8() != 0;
		e.water.second = r.F32();
		e.hatches = ReadHatches(r);
		if (r.U8() != 0) {
			e.fineHeights = make_unique<array<float, TERRAIN_FINE_SIZE * TERRAIN_FINE_SIZE>>();
			for (float& v : *e.fineHeights) v = r.F32();
		}
		else {
			e.fineHeights.reset();
		}
	}

	map.terrainTextures.clear();
	uint32_t textureCount = r.U32();
	for (uint32_t i = 0; i < textureCount && !r.Failed(); i++) {
		string key = r.Str();
		Map::TerrainTextureInfo info;
		info.slice = r.I32();
		info.diffuse = r.Str(); info.normal = r.Str(); info.roughness = r.Str();
		map.terrainTextures[key] = info;
	}
	if (r.Failed()) {
		error = "terrain section is truncated";
		return false;
	}
	return true;
}

// ======== 编号表(存档侧) ========

void MapSaveCodec::BuildSaveTables(const Map& map, SaveTables& t) {
	// Road：先按持有者登记，读档时同一张表按同样的持有关系重建。
	if (map.roadnet) {
		for (const Road* road : map.roadnet->roads) t.AddRoad(road, ROAD_OWNER_ROADNET);
		for (const Lot* lot : map.roadnet->lots) {
			for (int dir : SortedKeys(lot->boundaryRoads)) t.AddRoad(lot->boundaryRoads.at(dir), ROAD_OWNER_LOT_BOUNDARY);
		}
	}

	// Lot：顶层Lot前序遍历freeLots树，子块紧跟在父块后面。
	struct LotWalker {
		SaveTables& t;
		void Add(const Lot* lot, uint8_t owner, int parent) {
			if (!lot || t.lotIndex.count(lot)) return;
			int index = static_cast<int>(t.lots.size());
			t.lotIndex[lot] = index;
			t.lots.push_back(lot);
			t.lotOwners.push_back(owner);
			t.lotParents.push_back(parent);
			for (const PathRoadLink& link : lot->pathRoadLinks) t.AddRoad(link.road, ROAD_OWNER_PATH);
			for (const Lot* child : lot->freeLots) Add(child, LOT_OWNER_PARENT, index);
		}
	};
	LotWalker walker{ t };
	if (map.roadnet) {
		for (const Lot* lot : map.roadnet->lots) walker.Add(lot, LOT_OWNER_ROADNET, -1);
	}

	// Zone/Building按名字排序，保证写出顺序稳定。
	for (const string& name : SortedKeys(map.zones)) {
		const Zone* zone = map.zones.at(name);
		if (!zone) continue;
		t.zoneIndex[zone] = static_cast<int>(t.zones.size());
		t.zones.push_back(zone);
		for (const Road* road : zone->internalRoads) t.AddRoad(road, ROAD_OWNER_ZONE);
	}
	for (const string& name : SortedKeys(map.buildings)) {
		const Building* building = map.buildings.at(name);
		if (!building) continue;
		int index = static_cast<int>(t.buildings.size());
		t.buildingIndex[building] = index;
		t.buildings.push_back(building);
		for (size_t i = 0; i < building->rooms.size(); i++) {
			t.roomRef[building->rooms[i]] = { index, static_cast<int>(i) };
		}
	}

	// 游离Lot：Zone/Building的parentLot不在任何Lot树里(自适应占位用过的嵌套子块)。
	for (const Zone* zone : t.zones) walker.Add(zone->parentLot, LOT_OWNER_ORPHAN, -1);
	for (const Building* building : t.buildings) walker.Add(building->parentLot, LOT_OWNER_ORPHAN, -1);

	// 其余被引用、但没有登记过持有者的路(正常情况下不会有)：也编进表里，保证引用能解析。
	auto addLoose = [&](const Road* road) { t.AddRoad(road, ROAD_OWNER_NONE); };
	for (const Lot* lot : t.lots) {
		for (auto& [dir, road] : lot->boundaryRoads) addLoose(road);
		for (const PathRoadLink& link : lot->pathRoadLinks) { addLoose(link.endRoad1); addLoose(link.endRoad2); }
	}
	for (const Zone* zone : t.zones) for (auto& [dir, road] : zone->boundaryRoads) addLoose(road);
	for (const Building* building : t.buildings) for (auto& [dir, road] : building->boundaryRoads) addLoose(road);
	for (const RoadJunction* junction : map.junctions) {
		for (const RoadJunctionApproach& approach : junction->GetApproaches()) addLoose(approach.road);
	}
	vector<const Road*> throughRoads;
	for (auto& [road, lines] : map.throughLines) throughRoads.push_back(road);
	for (const Road* road : throughRoads) addLoose(road);
	int looseRoads = 0;
	for (uint8_t owner : t.roadOwners) if (owner == ROAD_OWNER_NONE) looseRoads++;
	if (looseRoads > 0) debugf("Warning: map save found %d referenced roads without an owner.\n", looseRoads);

	// 导航边：车行图、行人图按key排序遍历，去重编号(行人边双向各引用一次同一个Connection*)。
	auto addEdges = [&](const unordered_map<int, vector<pair<int, Connection*>>>& graph) {
		for (int from : SortedKeys(graph)) {
			for (auto& [to, edge] : graph.at(from)) {
				if (edge && !t.edgeIndex.count(edge)) {
					t.edgeIndex[edge] = static_cast<int>(t.edges.size());
					t.edges.push_back(edge);
				}
			}
		}
		};
	addEdges(map.vehicleNavGraph);
	addEdges(map.pedestrianNavGraph);

	// 导航锚点：Map自己持有的锚点 + 路网边缘残端(extern端点直接被当成锚点用)。
	auto addAnchor = [&](const Node* node) {
		if (!node) return;
		auto [it, inserted] = t.anchorsById.emplace(node->GetId(), node);
		if (!inserted && it->second != node) {
			debugf("Warning: map save found two navigation anchors sharing node id %d.\n", node->GetId());
		}
		};
	for (const Node* node : map.navAnchorNodes) addAnchor(node);
	if (map.roadnet) for (const Node* node : map.roadnet->externs) addAnchor(node);

	for (size_t i = 0; i < map.allDoors.size(); i++) t.doorIndex[map.allDoors[i]] = static_cast<int>(i);
}

// ======== 路网 + 导航图 ========

void MapSaveCodec::WriteRoadnet(const Map& map, const SaveTables& t, ByteWriter& w) {
	w.I32(Node::GetCount());

	const Roadnet* roadnet = map.roadnet;
	w.U8(roadnet ? 1 : 0);
	if (roadnet) {
		auto roadnetIds = LiveIds<RoadnetFactory, RoadnetMod>(map.roadnetFactory);
		w.Str(LookupId(roadnetIds, static_cast<const RoadnetMod*>(roadnet->mod)));
		w.Str(roadnet->type);
		w.Str(roadnet->name);
		w.Str(roadnet->pathRoadMaterial);
		w.U32(static_cast<uint32_t>(roadnet->externs.size()));
		for (const Node* node : roadnet->externs) WriteNode(*node, w);
		w.U32(static_cast<uint32_t>(roadnet->intersections.size()));
		for (const Intersection* node : roadnet->intersections) WriteNode(*node, w);
		WriteHatches(roadnet->hatches, w);
	}

	w.U32(static_cast<uint32_t>(t.roads.size()));
	for (size_t i = 0; i < t.roads.size(); i++) {
		w.U8(t.roadOwners[i]);
		WriteRoad(*t.roads[i], w);
	}

	w.U32(static_cast<uint32_t>(t.lots.size()));
	for (size_t i = 0; i < t.lots.size(); i++) {
		const Lot* lot = t.lots[i];
		w.U8(t.lotOwners[i]);
		w.I32(t.lotParents[i]);
		WriteQuad(*lot, w);
		w.F32(lot->rotation);
		w.I32(static_cast<int>(lot->area));
		w.U32(static_cast<uint32_t>(lot->addresses.size()));
		for (auto& [road, index] : lot->addresses) { w.Str(road); w.I32(index); }
		w.U32(static_cast<uint32_t>(lot->boundaryRoads.size()));
		for (int dir : SortedKeys(lot->boundaryRoads)) { w.I32(dir); w.I32(t.RoadRef(lot->boundaryRoads.at(dir))); }
		w.U8(lot->freeLotsInitialized ? 1 : 0);
		w.U32(static_cast<uint32_t>(lot->candidates.size()));
		for (auto& [type, weight] : lot->candidates) { w.Str(type); w.F32(weight); }
		w.U32(static_cast<uint32_t>(lot->pathRoadLinks.size()));
		for (const PathRoadLink& link : lot->pathRoadLinks) {
			w.I32(t.RoadRef(link.road));
			w.I32(t.RoadRef(link.endRoad1)); w.F32(link.endT1);
			w.I32(t.RoadRef(link.endRoad2)); w.F32(link.endT2);
		}
	}

	if (roadnet) {
		w.U32(static_cast<uint32_t>(roadnet->addressesByRoad.size()));
		for (const string& road : SortedKeys(roadnet->addressesByRoad)) {
			const vector<Lot*>& lots = roadnet->addressesByRoad.at(road);
			w.Str(road);
			w.U32(static_cast<uint32_t>(lots.size()));
			for (const Lot* lot : lots) w.I32(t.LotRef(lot));
		}
	}

	// 路口：中心点按Intersection下标引用。
	unordered_map<const Intersection*, int> intersectionIndex;
	if (roadnet) {
		for (size_t i = 0; i < roadnet->intersections.size(); i++) intersectionIndex[roadnet->intersections[i]] = static_cast<int>(i);
	}
	w.U32(static_cast<uint32_t>(map.junctions.size()));
	for (const RoadJunction* junction : map.junctions) {
		auto it = intersectionIndex.find(junction->GetNode());
		w.I32(it != intersectionIndex.end() ? it->second : -1);
		const vector<RoadJunctionApproach>& approaches = junction->GetApproaches();
		w.U32(static_cast<uint32_t>(approaches.size()));
		for (const RoadJunctionApproach& a : approaches) {
			w.I32(t.RoadRef(a.road));
			w.U8(a.isStart ? 1 : 0);
			w.F32(a.angle);
			w.U32(static_cast<uint32_t>(a.vehicleInbound.size()));
			for (const Node* node : a.vehicleInbound) w.I32(t.AnchorRef(node));
			w.U32(static_cast<uint32_t>(a.vehicleOutbound.size()));
			for (const Node* node : a.vehicleOutbound) w.I32(t.AnchorRef(node));
			w.I32(t.AnchorRef(a.pedestrianSide[0]));
			w.I32(t.AnchorRef(a.pedestrianSide[1]));
			w.F32(a.curbLeft.first); w.F32(a.curbLeft.second);
			w.F32(a.curbRight.first); w.F32(a.curbRight.second);
			w.F32(a.curbZ);
			w.F32(a.setback);
		}
	}

	w.U32(static_cast<uint32_t>(map.navAnchorNodes.size()));
	for (const Node* node : map.navAnchorNodes) WriteNode(*node, w);

	w.U32(static_cast<uint32_t>(t.edges.size()));
	for (const Connection* edge : t.edges) WriteConnection(*edge, w);

	auto writeGraph = [&](const unordered_map<int, vector<pair<int, Connection*>>>& graph) {
		w.U32(static_cast<uint32_t>(graph.size()));
		for (int from : SortedKeys(graph)) {
			const auto& edges = graph.at(from);
			w.I32(from);
			w.U32(static_cast<uint32_t>(edges.size()));
			for (auto& [to, edge] : edges) { w.I32(to); w.I32(t.EdgeRef(edge)); }
		}
		};
	writeGraph(map.vehicleNavGraph);
	writeGraph(map.pedestrianNavGraph);

	// 贯通线：按Road表下标排序。
	vector<pair<int, const Road*>> throughRoads;
	for (auto& [road, lines] : map.throughLines) throughRoads.emplace_back(t.RoadRef(road), road);
	sort(throughRoads.begin(), throughRoads.end());
	w.U32(static_cast<uint32_t>(throughRoads.size()));
	for (auto& [roadIndex, road] : throughRoads) {
		w.I32(roadIndex);
		const auto& lines = map.throughLines.at(const_cast<Road*>(road));
		for (const auto& list : lines) {
			w.U32(static_cast<uint32_t>(list.size()));
			for (const Map::ThroughLine& line : list) {
				w.I32(t.EdgeRef(line.edge));
				w.I32(t.AnchorRef(line.fromAnchor));
				w.I32(t.AnchorRef(line.toAnchor));
				w.I32(line.laneIndex);
				w.F32(line.tLo);
				w.F32(line.tHi);
			}
		}
	}
}

bool MapSaveCodec::ReadRoadnet(Map& map, LoadTables& t, ByteReader& r, string& error) {
	int nodeCount = r.I32();

	bool hasRoadnet = r.U8() != 0;
	vector<Intersection*> intersections;
	if (hasRoadnet) {
		string id = r.Str();
		if (!map.roadnetFactory.CheckRegistered(id)) {
			error = "roadnet mod '" + id + "' is not available";
			return false;
		}
		map.roadnetFactory.SetConfig(id, true);
		map.roadnet = new Roadnet(&map.roadnetFactory, id);
		Roadnet* roadnet = map.roadnet;
		roadnet->type = r.Str();
		roadnet->name = r.Str();
		roadnet->pathRoadMaterial = r.Str();
		uint32_t externCount = r.U32();
		for (uint32_t i = 0; i < externCount && !r.Failed(); i++) roadnet->externs.push_back(new Node(ReadNode(r)));
		uint32_t intersectionCount = r.U32();
		for (uint32_t i = 0; i < intersectionCount && !r.Failed(); i++) roadnet->intersections.push_back(new Intersection(ReadNode(r)));
		roadnet->hatches = ReadHatches(r);
		intersections = roadnet->intersections;
	}

	uint32_t roadCount = r.U32();
	vector<uint8_t> roadOwners;
	for (uint32_t i = 0; i < roadCount && !r.Failed(); i++) {
		uint8_t owner = r.U8();
		Road* road = ReadRoad(r);
		t.roads.push_back(road);
		roadOwners.push_back(owner);
		if (owner == ROAD_OWNER_ROADNET && map.roadnet) map.roadnet->roads.push_back(road);
	}

	uint32_t lotCount = r.U32();
	vector<vector<tuple<int, int, float, int, float>>> pendingLinks;
	vector<vector<pair<int, int>>> pendingBoundaries;
	for (uint32_t i = 0; i < lotCount && !r.Failed(); i++) {
		uint8_t owner = r.U8();
		int parent = r.I32();
		Lot* lot = new Lot();
		ReadQuad(*lot, r);
		lot->rotation = r.F32();
		lot->area = static_cast<AREA_TYPE>(r.I32());
		uint32_t addressCount = r.U32();
		for (uint32_t j = 0; j < addressCount && !r.Failed(); j++) {
			string road = r.Str();
			int index = r.I32();
			lot->addresses.emplace_back(road, index);
		}
		vector<pair<int, int>> boundaries(r.U32());
		for (auto& [dir, roadIndex] : boundaries) { dir = r.I32(); roadIndex = r.I32(); }
		lot->freeLotsInitialized = r.U8() != 0;
		uint32_t candidateCount = r.U32();
		for (uint32_t j = 0; j < candidateCount && !r.Failed(); j++) {
			string type = r.Str();
			float weight = r.F32();
			lot->candidates.emplace_back(type, weight);
		}
		vector<tuple<int, int, float, int, float>> links(r.U32());
		for (auto& [roadIndex, end1, t1, end2, t2] : links) {
			roadIndex = r.I32(); end1 = r.I32(); t1 = r.F32(); end2 = r.I32(); t2 = r.F32();
		}

		t.lots.push_back(lot);
		pendingBoundaries.push_back(move(boundaries));
		pendingLinks.push_back(move(links));

		if (owner == LOT_OWNER_ROADNET && map.roadnet) map.roadnet->lots.push_back(lot);
		else if (owner == LOT_OWNER_PARENT && parent >= 0 && parent < static_cast<int>(t.lots.size()) - 1) t.lots[parent]->freeLots.push_back(lot);
		else map.orphanLots.push_back(lot);
	}
	// 连Lot里的Road引用(小路的持有权跟着PathRoadLink交给这个Lot)。
	for (size_t i = 0; i < t.lots.size() && i < pendingLinks.size(); i++) {
		Lot* lot = t.lots[i];
		for (auto& [dir, roadIndex] : pendingBoundaries[i]) {
			if (Road* road = t.RoadAt(roadIndex)) lot->boundaryRoads[dir] = road;
		}
		for (auto& [roadIndex, end1, t1, end2, t2] : pendingLinks[i]) {
			PathRoadLink link;
			link.road = t.RoadAt(roadIndex);
			link.endRoad1 = t.RoadAt(end1); link.endT1 = t1;
			link.endRoad2 = t.RoadAt(end2); link.endT2 = t2;
			if (link.road) lot->pathRoadLinks.push_back(link);
		}
	}

	if (map.roadnet) {
		uint32_t addressRoads = r.U32();
		for (uint32_t i = 0; i < addressRoads && !r.Failed(); i++) {
			string road = r.Str();
			vector<Lot*>& lots = map.roadnet->addressesByRoad[road];
			uint32_t n = r.U32();
			for (uint32_t j = 0; j < n && !r.Failed(); j++) lots.push_back(t.LotAt(r.I32()));
		}
	}

	// 锚点要先建好，路口/贯通线才能引用；锚点数据写在路口后面，所以路口先按id暂存。
	struct PendingApproach {
		RoadJunctionApproach approach;
		vector<int> inbound, outbound;
		int ped0 = -1, ped1 = -1;
	};
	vector<pair<int, vector<PendingApproach>>> pendingJunctions(r.U32());
	for (auto& [intersectionIndex, approaches] : pendingJunctions) {
		intersectionIndex = r.I32();
		approaches.resize(r.U32());
		for (PendingApproach& p : approaches) {
			p.approach.road = t.RoadAt(r.I32());
			p.approach.isStart = r.U8() != 0;
			p.approach.angle = r.F32();
			p.inbound.resize(r.U32());
			for (int& id : p.inbound) id = r.I32();
			p.outbound.resize(r.U32());
			for (int& id : p.outbound) id = r.I32();
			p.ped0 = r.I32();
			p.ped1 = r.I32();
			p.approach.curbLeft.first = r.F32(); p.approach.curbLeft.second = r.F32();
			p.approach.curbRight.first = r.F32(); p.approach.curbRight.second = r.F32();
			p.approach.curbZ = r.F32();
			p.approach.setback = r.F32();
			if (r.Failed()) break;
		}
		if (r.Failed()) break;
	}

	uint32_t anchorCount = r.U32();
	for (uint32_t i = 0; i < anchorCount && !r.Failed(); i++) {
		Node* node = new Node(ReadNode(r));
		map.navAnchorNodes.push_back(node);
		t.anchorsById[node->GetId()] = node;
	}
	if (map.roadnet) {
		for (Node* node : map.roadnet->externs) t.anchorsById.emplace(node->GetId(), node);
	}

	for (auto& [intersectionIndex, approaches] : pendingJunctions) {
		vector<RoadJunctionApproach> built;
		for (PendingApproach& p : approaches) {
			for (int id : p.inbound) p.approach.vehicleInbound.push_back(t.AnchorAt(id));
			for (int id : p.outbound) p.approach.vehicleOutbound.push_back(t.AnchorAt(id));
			p.approach.pedestrianSide[0] = t.AnchorAt(p.ped0);
			p.approach.pedestrianSide[1] = t.AnchorAt(p.ped1);
			built.push_back(p.approach);
		}
		Intersection* node = (intersectionIndex >= 0 && intersectionIndex < static_cast<int>(intersections.size()))
			? intersections[intersectionIndex] : nullptr;
		map.junctions.push_back(new RoadJunction(node, move(built)));
	}

	uint32_t edgeCount = r.U32();
	for (uint32_t i = 0; i < edgeCount && !r.Failed(); i++) t.edges.push_back(ReadConnection(r));

	auto readGraph = [&](unordered_map<int, vector<pair<int, Connection*>>>& graph) {
		uint32_t keys = r.U32();
		for (uint32_t i = 0; i < keys && !r.Failed(); i++) {
			int from = r.I32();
			auto& edges = graph[from];
			uint32_t n = r.U32();
			for (uint32_t j = 0; j < n && !r.Failed(); j++) {
				int to = r.I32();
				Connection* edge = t.EdgeAt(r.I32());
				if (edge) edges.emplace_back(to, edge);
			}
		}
		};
	readGraph(map.vehicleNavGraph);
	readGraph(map.pedestrianNavGraph);

	uint32_t throughCount = r.U32();
	for (uint32_t i = 0; i < throughCount && !r.Failed(); i++) {
		Road* road = t.RoadAt(r.I32());
		array<vector<Map::ThroughLine>, 4> lines;
		for (auto& list : lines) {
			uint32_t n = r.U32();
			for (uint32_t j = 0; j < n && !r.Failed(); j++) {
				Map::ThroughLine line;
				line.edge = t.EdgeAt(r.I32());
				line.fromAnchor = t.AnchorAt(r.I32());
				line.toAnchor = t.AnchorAt(r.I32());
				line.laneIndex = r.I32();
				line.tLo = r.F32();
				line.tHi = r.F32();
				list.push_back(line);
			}
		}
		if (road) map.throughLines[road] = move(lines);
	}

	if (r.Failed()) {
		error = "roadnet section is truncated";
		return false;
	}
	// 宿主这份Node::count恢复成存档时的值(之后新建的Node继续往后编号，不和读回来的id撞)。读档
	// 过程中构造Node会让计数器白涨，这里精确设回存档值，再存一次档时这个字段才能和原存档一致。
	Node::SetCount(max(nodeCount, gMaxLoadedNodeId + 1));
	return true;
}

// ======== 楼层 ========

void MapSaveCodec::WriteFloor(const Floor& floor, ByteWriter& w) {
	w.I32(floor.level);
	WriteQuad(floor, w);
	w.I32(floor.number);

	auto writeDirWalls = [&](const auto& list) {
		w.U32(static_cast<uint32_t>(list.size()));
		for (const auto& e : list) {
			WriteFloats(e.params, w);
			WriteQuad(e, w);
			w.I32(e.direction);
			for (bool wall : e.walls) w.U8(wall ? 1 : 0);
		}
		};
	auto writePlain = [&](const auto& list) {
		w.U32(static_cast<uint32_t>(list.size()));
		for (const auto& e : list) {
			WriteFloats(e.params, w);
			WriteQuad(e, w);
		}
		};
	auto writeOpenings = [&](const auto& list, bool hasWalls) {
		w.U32(static_cast<uint32_t>(list.size()));
		for (const auto& e : list) {
			WriteFloats(e.params, w);
			WriteQuad(e, w);
			WriteWallHole(e.doors, w);
			WriteDoorTags(e.doorTags, w);
			WriteWallHole(e.windows, w);
			(void)hasWalls;
		}
		};

	writeDirWalls(floor.stairs);
	writeDirWalls(floor.elevators);
	writeDirWalls(floor.ramps);
	writePlain(floor.ceilings);
	writePlain(floor.grounds);
	writePlain(floor.hatches);

	writeOpenings(floor.corridors, true);
	for (const Corridor& c : floor.corridors) for (bool wall : c.walls) w.U8(wall ? 1 : 0);
	writeOpenings(floor.singles, false);
	for (const Single& s : floor.singles) w.I32(s.direction);
	writeOpenings(floor.rows, false);
	for (const Row& row : floor.rows) w.I32(row.direction);
}

void MapSaveCodec::ReadFloor(Floor& floor, ByteReader& r) {
	floor.level = r.I32();
	ReadQuad(floor, r);
	floor.number = r.I32();

	auto readDirWalls = [&](auto& list) {
		using T = typename std::decay_t<decltype(list)>::value_type;
		uint32_t n = r.U32();
		for (uint32_t i = 0; i < n && !r.Failed(); i++) {
			RectParams params;
			ReadFloats(params, r);
			T e(params);
			ReadQuad(e, r);
			e.direction = r.I32();
			for (bool& wall : e.walls) wall = r.U8() != 0;
			list.push_back(e);
		}
		};
	auto readPlain = [&](auto& list) {
		using T = typename std::decay_t<decltype(list)>::value_type;
		uint32_t n = r.U32();
		for (uint32_t i = 0; i < n && !r.Failed(); i++) {
			RectParams params;
			ReadFloats(params, r);
			T e(params);
			ReadQuad(e, r);
			list.push_back(e);
		}
		};
	auto readOpenings = [&](auto& list) {
		using T = typename std::decay_t<decltype(list)>::value_type;
		uint32_t n = r.U32();
		for (uint32_t i = 0; i < n && !r.Failed(); i++) {
			RectParams params;
			ReadFloats(params, r);
			T e(params);
			ReadQuad(e, r);
			e.doors = ReadWallHole(r);
			e.doorTags = ReadDoorTags(r);
			e.windows = ReadWallHole(r);
			list.push_back(e);
		}
		};

	readDirWalls(floor.stairs);
	readDirWalls(floor.elevators);
	readDirWalls(floor.ramps);
	readPlain(floor.ceilings);
	readPlain(floor.grounds);
	readPlain(floor.hatches);

	readOpenings(floor.corridors);
	for (Corridor& c : floor.corridors) for (bool& wall : c.walls) wall = r.U8() != 0;
	readOpenings(floor.singles);
	for (Single& s : floor.singles) s.direction = r.I32();
	readOpenings(floor.rows);
	for (Row& row : floor.rows) row.direction = r.I32();
}

// ======== Zone / Building / Room / Component / Door ========

namespace {
	void WriteBoundary(const unordered_map<int, Road*>& boundary, ByteWriter& w, const function<int(const Road*)>& ref) {
		w.U32(static_cast<uint32_t>(boundary.size()));
		for (int dir : SortedKeys(boundary)) { w.I32(dir); w.I32(ref(boundary.at(dir))); }
	}
}

void MapSaveCodec::WriteStructures(const Map& map, const SaveTables& t, ByteWriter& w) {
	auto roadRef = [&](const Road* road) { return t.RoadRef(road); };

	w.I32(map.doorCount);
	w.U32(static_cast<uint32_t>(map.doorNameSequence.size()));
	for (const string& key : SortedKeys(map.doorNameSequence)) { w.Str(key); w.I32(map.doorNameSequence.at(key)); }

	auto zoneIds = LiveIds<ZoneFactory, ZoneMod>(map.zoneFactory);
	auto buildingIds = LiveIds<BuildingFactory, BuildingMod>(map.buildingFactory);
	auto roomIds = LiveIds<RoomFactory, RoomMod>(map.roomFactory);
	auto componentIds = LiveIds<ComponentFactory, ComponentMod>(map.componentFactory);

	w.U32(static_cast<uint32_t>(t.zones.size()));
	for (const Zone* zone : t.zones) {
		w.Str(LookupId(zoneIds, static_cast<const ZoneMod*>(zone->mod)));
		WriteModState(zone->mod, w);
		w.Str(zone->type);
		w.Str(zone->name);
		WriteQuad(*zone, w);
		w.I32(t.LotRef(zone->parentLot));
		w.I32(zone->direction);
		w.Str(zone->scriptModName); WriteStrings(zone->scriptMilestones, w);
		WriteDecorations(zone->decorations, w);
		WriteBoundary(zone->boundaryRoads, w, roadRef);
		w.U32(static_cast<uint32_t>(zone->internalRoads.size()));
		for (const Road* road : zone->internalRoads) w.I32(t.RoadRef(road));
		w.U32(static_cast<uint32_t>(zone->internalBuildings.size()));
		for (const Building* building : zone->internalBuildings) w.I32(t.BuildingRef(building));
		w.U8(zone->stated ? 1 : 0);
		w.U32(static_cast<uint32_t>(zone->walls.size()));
		for (const ZoneWallSpec& s : zone->walls) {
			w.I32(s.direction); w.F32(s.marginStart); w.F32(s.marginEnd); w.F32(s.depth);
			w.U8(s.depthInward ? 1 : 0); w.Str(s.mesh); w.F32(s.unit);
		}
		w.U32(static_cast<uint32_t>(zone->gates.size()));
		for (const ZoneGateSpec& s : zone->gates) {
			w.I32(s.direction); w.F32(s.marginStart); w.F32(s.marginEnd); w.F32(s.depth);
			w.U8(s.depthInward ? 1 : 0); WriteDoorSpec(s.door, w);
		}
	}

	w.U32(static_cast<uint32_t>(t.buildings.size()));
	for (const Building* building : t.buildings) {
		w.Str(LookupId(buildingIds, static_cast<const BuildingMod*>(building->mod)));
		WriteModState(building->mod, w);
		w.Str(building->type);
		w.Str(building->name);
		WriteQuad(*building, w);
		w.I32(t.LotRef(building->parentLot));
		w.I32(t.ZoneRef(building->parentZone));
		w.F32(building->relativeRotation);
		WriteBoundary(building->boundaryRoads, w, roadRef);
		w.I32(building->direction);
		w.F32(building->bodyOffsetX); w.F32(building->bodyOffsetY);
		w.F32(building->bodySizeX); w.F32(building->bodySizeY);
		w.I32(building->basements);
		w.I32(building->layers);
		WriteFloatVector(building->floorHeights, w);
		w.Str(building->lodMaterialPath);
		WriteDecorations(building->decorations, w);
		w.U32(static_cast<uint32_t>(building->floorSpecs.size()));
		for (int level : SortedKeys(building->floorSpecs)) {
			const FloorLayoutSpec& s = building->floorSpecs.at(level);
			w.I32(level);
			w.Str(s.templateName);
			w.I32(s.face);
			w.Str(s.assets.wallMaterial); w.Str(s.assets.floorMaterial); w.Str(s.assets.ceilingMaterial);
			w.Str(s.assets.stairMeshPath); w.U8(s.assets.buildShell ? 1 : 0); w.Str(s.assets.rampMeshPath);
		}
		w.U32(static_cast<uint32_t>(building->cabins.size()));
		for (const ElevatorCabinSpec& s : building->cabins) {
			w.I32(s.shaftIndex); w.I32(s.minFloor); w.I32(s.maxFloor); w.Str(s.cabinMeshPath);
			w.Str(s.scriptModName); WriteStrings(s.milestoneNames, w);
		}
		w.Str(building->stationMod);
		w.Str(building->scriptModName); WriteStrings(building->scriptMilestones, w);

		w.U32(static_cast<uint32_t>(building->floors.size()));
		for (const Floor& floor : building->floors) WriteFloor(floor, w);

		unordered_map<const Component*, int> componentIndex;
		w.U32(static_cast<uint32_t>(building->components.size()));
		for (size_t i = 0; i < building->components.size(); i++) {
			const Component* component = building->components[i];
			componentIndex[component] = static_cast<int>(i);
			w.Str(LookupId(componentIds, static_cast<const ComponentMod*>(component->mod)));
			WriteModState(component->mod, w);
			w.Str(component->type);
			w.Str(component->name);
		}

		unordered_map<const Room*, int> roomIndex;
		for (size_t i = 0; i < building->rooms.size(); i++) roomIndex[building->rooms[i]] = static_cast<int>(i);
		auto localRoom = [&](const Room* room) {
			auto it = roomIndex.find(room);
			return it != roomIndex.end() ? it->second : -1;
			};

		w.U32(static_cast<uint32_t>(building->rooms.size()));
		for (const Room* room : building->rooms) {
			w.Str(LookupId(roomIds, static_cast<const RoomMod*>(room->mod)));
			WriteModState(room->mod, w);
			w.Str(room->type);
			w.Str(room->name);
			WriteQuad(*room, w);
			auto ci = componentIndex.find(room->parentComponent);
			w.I32(ci != componentIndex.end() ? ci->second : -1);
			w.I32(room->layer);
			w.I32(room->direction);
			WriteWallHole(room->doors, w);
			WriteDoorTags(room->doorTags, w);
			WriteWallHole(room->windows, w);
			w.Str(room->number);
			// 房间导航点通常登记在Map的锚点表里(按id引用)；没登记的按值另存一份。
			const Node* nav = room->navigationNode;
			bool registered = nav && t.anchorsById.count(nav->GetId()) && t.anchorsById.at(nav->GetId()) == nav;
			w.U8(nav ? (registered ? 1 : 2) : 0);
			if (registered) w.I32(nav->GetId());
			else if (nav) WriteNode(*nav, w);
			const Room::Properties& p = room->properties;
			w.U8(p.isResidential ? 1 : 0); w.I32(p.residentialCapacity);
			w.U8(p.isWorkspace ? 1 : 0); w.I32(p.workspaceCapacity);
			w.U8(p.isStorage ? 1 : 0);
			w.U32(static_cast<uint32_t>(p.storageConfig.size()));
			for (const string& key : SortedKeys(p.storageConfig)) { w.Str(key); w.F32(p.storageConfig.at(key)); }
			w.U8(p.isManufacture ? 1 : 0);
			WriteStrings(p.manufactureTypes, w);
			w.U8(p.isParking ? 1 : 0);
			w.U32(static_cast<uint32_t>(p.parkingSpots.size()));
			for (const ParkingSpot& s : p.parkingSpots) { WriteFloats(s.position, w); w.F32(s.rotationDegrees); WriteStrings(s.categories, w); }
			w.U8(room->stated ? 1 : 0);
		}

		for (const Component* component : building->components) {
			w.U32(static_cast<uint32_t>(component->rooms.size()));
			for (const Room* room : component->rooms) w.I32(localRoom(room));
		}

		w.U32(static_cast<uint32_t>(building->singleRoomBySlot.size()));
		for (int level : SortedKeys(building->singleRoomBySlot)) {
			const auto& slots = building->singleRoomBySlot.at(level);
			w.I32(level);
			w.U32(static_cast<uint32_t>(slots.size()));
			for (int slot : SortedKeys(slots)) { w.I32(slot); w.I32(localRoom(slots.at(slot))); }
		}
		w.U32(static_cast<uint32_t>(building->rowRoomBySlot.size()));
		for (int level : SortedKeys(building->rowRoomBySlot)) {
			const auto& slots = building->rowRoomBySlot.at(level);
			w.I32(level);
			w.U32(static_cast<uint32_t>(slots.size()));
			for (int slot : SortedKeys(slots)) {
				const vector<Room*>& list = slots.at(slot);
				w.I32(slot);
				w.U32(static_cast<uint32_t>(list.size()));
				for (const Room* room : list) w.I32(localRoom(room));
			}
		}
		w.U8(building->stated ? 1 : 0);

		// 电梯：所在楼层和行程状态、交互选项。Script的状态由script_save存(键cabin:<交互名>)。
		w.U32(static_cast<uint32_t>(building->cabinEntities.size()));
		for (const Cabin* cabin : building->cabinEntities) {
			w.I32(cabin->specIndex);
			w.Str(cabin->interactName);
			w.I32(cabin->currentLevel);
			w.F32(cabin->currentZ);
			w.U8(cabin->moving ? 1 : 0);
			w.F32(cabin->fromZ);
			w.I32(cabin->targetLevel);
			w.F32(cabin->elapsed);
			WriteStrings(cabin->options, w);
		}
	}

	// 门：按Map::allDoors顺序；所属对象按下标引用。
	w.U32(static_cast<uint32_t>(map.allDoors.size()));
	for (const Door* door : map.allDoors) {
		w.Str(door->id);
		w.I32(static_cast<int>(door->kind));
		w.Str(door->tag);
		WriteDoorSpec(door->spec, w);
		w.I32(t.ZoneRef(door->zone));
		w.I32(t.BuildingRef(door->building));
		auto roomIt = door->room ? t.roomRef.find(door->room) : t.roomRef.end();
		w.I32(roomIt != t.roomRef.end() ? roomIt->second.first : -1);
		w.I32(roomIt != t.roomRef.end() ? roomIt->second.second : -1);
		w.Str(door->interactName);
		w.F32(door->x); w.F32(door->y); w.F32(door->z);
		w.F32(door->width); w.F32(door->height); w.F32(door->yaw);
		w.I32(door->level);
		w.U8(door->vehicleGate ? 1 : 0);
		w.U8(door->flippedSide ? 1 : 0);
		w.I32(static_cast<int>(door->access));
		// 按人登记的门禁例外(名字 + 是否允许)。
		vector<string> registered = SortedKeys(door->registrations);
		w.U32(static_cast<uint32_t>(registered.size()));
		for (const string& citizen : registered) {
			w.Str(citizen);
			w.U8(door->registrations.at(citizen) ? 1 : 0);
		}
		WriteStrings(door->options, w);
	}

	// 门的持有者(谁析构时delete它)：Zone/Building/Room各自的doorEntities。
	for (const Zone* zone : t.zones) {
		w.U32(static_cast<uint32_t>(zone->doorEntities.size()));
		for (const Door* door : zone->doorEntities) w.I32(t.DoorRef(door));
	}
	for (const Building* building : t.buildings) {
		w.U32(static_cast<uint32_t>(building->doorEntities.size()));
		for (const Door* door : building->doorEntities) w.I32(t.DoorRef(door));
		for (const Room* room : building->rooms) {
			w.U32(static_cast<uint32_t>(room->doorEntities.size()));
			for (const Door* door : room->doorEntities) w.I32(t.DoorRef(door));
		}
	}
}

bool MapSaveCodec::ReadStructures(Map& map, LoadTables& t, ByteReader& r, bool restoreOptions, string& error) {
	map.doorCount = r.I32();
	uint32_t sequenceCount = r.U32();
	for (uint32_t i = 0; i < sequenceCount && !r.Failed(); i++) {
		string key = r.Str();
		map.doorNameSequence[key] = r.I32();
	}

	auto readBoundary = [&](unordered_map<int, Road*>& boundary) {
		uint32_t n = r.U32();
		for (uint32_t i = 0; i < n && !r.Failed(); i++) {
			int dir = r.I32();
			if (Road* road = t.RoadAt(r.I32())) boundary[dir] = road;
		}
		};

	vector<vector<int>> zoneInternalBuildings;
	uint32_t zoneCount = r.U32();
	for (uint32_t i = 0; i < zoneCount && !r.Failed(); i++) {
		string id = r.Str();
		ZoneMod* mod = map.zoneFactory.CreateZone(id);
		if (!mod) {
			error = "zone mod '" + id + "' is not available";
			return false;
		}
		Zone* zone = new Zone(&map.zoneFactory, mod);
		t.zones.push_back(zone);
		if (!ReadModState(mod, r)) debugf("Warning: failed to parse saved state of zone mod %s.\n", id.c_str());
		zone->type = r.Str();
		zone->name = r.Str();
		ReadQuad(*zone, r);
		zone->parentLot = t.LotAt(r.I32());
		zone->direction = r.I32();
		zone->scriptModName = r.Str(); zone->scriptMilestones = ReadStrings(r);
		zone->decorations = ReadDecorations(r);
		readBoundary(zone->boundaryRoads);
		uint32_t internalRoads = r.U32();
		for (uint32_t j = 0; j < internalRoads && !r.Failed(); j++) {
			if (Road* road = t.RoadAt(r.I32())) zone->internalRoads.push_back(road);
		}
		vector<int> internalBuildings(r.U32());
		for (int& index : internalBuildings) index = r.I32();
		zoneInternalBuildings.push_back(move(internalBuildings));
		zone->stated = r.U8() != 0;
		zone->walls.resize(r.U32());
		for (ZoneWallSpec& s : zone->walls) {
			s.direction = r.I32(); s.marginStart = r.F32(); s.marginEnd = r.F32(); s.depth = r.F32();
			s.depthInward = r.U8() != 0; s.mesh = r.Str(); s.unit = r.F32();
		}
		zone->gates.resize(r.U32());
		for (ZoneGateSpec& s : zone->gates) {
			s.direction = r.I32(); s.marginStart = r.F32(); s.marginEnd = r.F32(); s.depth = r.F32();
			s.depthInward = r.U8() != 0; s.door = ReadDoorSpec(r);
		}
		map.zones[zone->name] = zone;
	}

	uint32_t buildingCount = r.U32();
	for (uint32_t i = 0; i < buildingCount && !r.Failed(); i++) {
		string id = r.Str();
		BuildingMod* mod = map.buildingFactory.CreateBuilding(id);
		if (!mod) {
			error = "building mod '" + id + "' is not available";
			return false;
		}
		Building* building = new Building(&map.buildingFactory, mod);
		t.buildings.push_back(building);
		if (!ReadModState(mod, r)) debugf("Warning: failed to parse saved state of building mod %s.\n", id.c_str());
		building->type = r.Str();
		building->name = r.Str();
		ReadQuad(*building, r);
		building->parentLot = t.LotAt(r.I32());
		building->parentZone = t.ZoneAt(r.I32());
		building->relativeRotation = r.F32();
		readBoundary(building->boundaryRoads);
		building->direction = r.I32();
		building->bodyOffsetX = r.F32(); building->bodyOffsetY = r.F32();
		building->bodySizeX = r.F32(); building->bodySizeY = r.F32();
		building->basements = r.I32();
		building->layers = r.I32();
		building->floorHeights = ReadFloatVector(r);
		building->lodMaterialPath = r.Str();
		building->decorations = ReadDecorations(r);
		uint32_t specCount = r.U32();
		for (uint32_t j = 0; j < specCount && !r.Failed(); j++) {
			int level = r.I32();
			FloorLayoutSpec& s = building->floorSpecs[level];
			s.templateName = r.Str();
			s.face = r.I32();
			s.assets.wallMaterial = r.Str(); s.assets.floorMaterial = r.Str(); s.assets.ceilingMaterial = r.Str();
			s.assets.stairMeshPath = r.Str(); s.assets.buildShell = r.U8() != 0; s.assets.rampMeshPath = r.Str();
		}
		building->cabins.resize(r.U32());
		for (ElevatorCabinSpec& s : building->cabins) {
			s.shaftIndex = r.I32(); s.minFloor = r.I32(); s.maxFloor = r.I32(); s.cabinMeshPath = r.Str();
			s.scriptModName = r.Str(); s.milestoneNames = ReadStrings(r);
		}
		building->stationMod = r.Str();
		building->scriptModName = r.Str(); building->scriptMilestones = ReadStrings(r);

		uint32_t floorCount = r.U32();
		for (uint32_t j = 0; j < floorCount && !r.Failed(); j++) {
			Floor floor(0, 0.f, 0.f);
			ReadFloor(floor, r);
			building->floors.push_back(move(floor));
		}

		uint32_t componentCount = r.U32();
		for (uint32_t j = 0; j < componentCount && !r.Failed(); j++) {
			string componentId = r.Str();
			ComponentMod* componentMod = map.componentFactory.CreateComponent(componentId);
			if (!componentMod) {
				error = "component mod '" + componentId + "' is not available";
				return false;
			}
			Component* component = new Component(&map.componentFactory, componentMod, building);
			building->components.push_back(component);
			if (!ReadModState(componentMod, r)) debugf("Warning: failed to parse saved state of component mod %s.\n", componentId.c_str());
			component->type = r.Str();
			component->name = r.Str();
		}

		uint32_t roomCount = r.U32();
		for (uint32_t j = 0; j < roomCount && !r.Failed(); j++) {
			string roomId = r.Str();
			RoomMod* roomMod = map.roomFactory.CreateRoom(roomId);
			if (!roomMod) {
				error = "room mod '" + roomId + "' is not available";
				return false;
			}
			int version = r.I32();
			string stateText = r.Str();
			string type = r.Str();
			string name = r.Str();
			Quad quad;
			ReadQuad(quad, r);
			int componentIndex = r.I32();
			Component* component = (componentIndex >= 0 && componentIndex < static_cast<int>(building->components.size()))
				? building->components[componentIndex] : nullptr;
			int layer = r.I32();
			Room* room = new Room(&map.roomFactory, roomMod, building, component, layer);
			building->rooms.push_back(room);
			if (!stateText.empty()) {
				JsonValue root;
				JsonReader reader;
				if (reader.Parse(stateText, root, false)) {
					JsonStateReader stateReader(root, version);
					roomMod->LoadState(stateReader);
				}
				else {
					debugf("Warning: failed to parse saved state of room mod %s.\n", roomId.c_str());
				}
			}
			room->type = type;
			room->name = name;
			room->SetPosition(quad.GetPosX(), quad.GetPosY(), quad.GetSizeX(), quad.GetSizeY());
			room->SetAcreage(quad.GetAcreage());
			room->direction = r.I32();
			room->doors = ReadWallHole(r);
			room->doorTags = ReadDoorTags(r);
			room->windows = ReadWallHole(r);
			room->number = r.Str();
			uint8_t navKind = r.U8();
			if (navKind == 1) {
				room->navigationNode = t.AnchorAt(r.I32());
			}
			else if (navKind == 2) {
				Node* nav = new Node(ReadNode(r));
				map.navAnchorNodes.push_back(nav);
				room->navigationNode = nav;
			}
			Room::Properties& p = room->properties;
			p.isResidential = r.U8() != 0; p.residentialCapacity = r.I32();
			p.isWorkspace = r.U8() != 0; p.workspaceCapacity = r.I32();
			p.isStorage = r.U8() != 0;
			p.storageConfig.clear();
			uint32_t storageCount = r.U32();
			for (uint32_t k = 0; k < storageCount && !r.Failed(); k++) {
				string key = r.Str();
				p.storageConfig[key] = r.F32();
			}
			p.isManufacture = r.U8() != 0;
			p.manufactureTypes = ReadStrings(r);
			p.isParking = r.U8() != 0;
			p.parkingSpots.resize(r.U32());
			for (ParkingSpot& s : p.parkingSpots) { ReadFloats(s.position, r); s.rotationDegrees = r.F32(); s.categories = ReadStrings(r); }
			room->stated = r.U8() != 0;
		}

		auto roomAt = [&](int index) -> Room* {
			return (index >= 0 && index < static_cast<int>(building->rooms.size())) ? building->rooms[index] : nullptr;
			};
		for (Component* component : building->components) {
			uint32_t n = r.U32();
			for (uint32_t k = 0; k < n && !r.Failed(); k++) {
				if (Room* room = roomAt(r.I32())) component->rooms.push_back(room);
			}
		}
		uint32_t singleLevels = r.U32();
		for (uint32_t k = 0; k < singleLevels && !r.Failed(); k++) {
			int level = r.I32();
			uint32_t slots = r.U32();
			for (uint32_t s = 0; s < slots && !r.Failed(); s++) {
				int slot = r.I32();
				if (Room* room = roomAt(r.I32())) building->singleRoomBySlot[level][slot] = room;
			}
		}
		uint32_t rowLevels = r.U32();
		for (uint32_t k = 0; k < rowLevels && !r.Failed(); k++) {
			int level = r.I32();
			uint32_t slots = r.U32();
			for (uint32_t s = 0; s < slots && !r.Failed(); s++) {
				int slot = r.I32();
				vector<Room*>& list = building->rowRoomBySlot[level][slot];
				uint32_t n = r.U32();
				for (uint32_t m = 0; m < n && !r.Failed(); m++) {
					if (Room* room = roomAt(r.I32())) list.push_back(room);
				}
			}
		}
		building->stated = r.U8() != 0;

		uint32_t cabinCount = r.U32();
		for (uint32_t k = 0; k < cabinCount && !r.Failed(); k++) {
			int specIndex = r.I32();
			string interactName = r.Str();
			int currentLevel = r.I32();
			float currentZ = r.F32();
			bool moving = r.U8() != 0;
			float fromZ = r.F32();
			int targetLevel = r.I32();
			float elapsed = r.F32();
			vector<string> options = ReadStrings(r);
			if (specIndex < 0 || specIndex >= static_cast<int>(building->cabins.size())) continue;
			// 构造函数会按spec创建电梯的Script(和新开局一样，脚本状态由script_save恢复)。
			const ElevatorCabinSpec& spec = building->cabins[specIndex];
			Cabin* cabin = new Cabin(building, specIndex, interactName, spec.shaftIndex, spec.minFloor, spec.maxFloor,
				currentLevel, currentZ, spec.scriptModName, spec.milestoneNames, &map.scriptFactory);
			cabin->moving = moving;
			cabin->fromZ = fromZ;
			cabin->targetLevel = targetLevel;
			cabin->elapsed = elapsed;
			// options和门一样：存档带脚本状态段时(读档跳过game_start)才恢复，否则game_start会再加一遍。
			if (restoreOptions) cabin->options = options;
			building->cabinEntities.push_back(cabin);
			map.cabinsByInteractName[interactName] = cabin;
		}
		map.buildings[building->name] = building;
	}

	for (size_t i = 0; i < t.zones.size() && i < zoneInternalBuildings.size(); i++) {
		for (int index : zoneInternalBuildings[i]) {
			if (Building* building = t.BuildingAt(index)) t.zones[i]->internalBuildings.push_back(building);
		}
	}

	uint32_t doorCount = r.U32();
	for (uint32_t i = 0; i < doorCount && !r.Failed(); i++) {
		string id = r.Str();
		DOOR_KIND_TYPE kind = static_cast<DOOR_KIND_TYPE>(r.I32());
		string tag = r.Str();
		DoorSpec spec = ReadDoorSpec(r);
		Zone* zone = t.ZoneAt(r.I32());
		Building* building = t.BuildingAt(r.I32());
		Building* roomBuilding = t.BuildingAt(r.I32());
		int roomIndex = r.I32();
		Room* room = (roomBuilding && roomIndex >= 0 && roomIndex < static_cast<int>(roomBuilding->rooms.size()))
			? roomBuilding->rooms[roomIndex] : nullptr;
		string interactName = r.Str();
		float x = r.F32(), y = r.F32(), z = r.F32();
		float width = r.F32(), height = r.F32(), yaw = r.F32();
		int level = r.I32();
		bool vehicleGate = r.U8() != 0;
		bool flippedSide = r.U8() != 0;
		DOOR_ACCESS_TYPE access = static_cast<DOOR_ACCESS_TYPE>(r.I32());
		vector<pair<string, bool>> registrations(r.U32());
		for (auto& [citizen, pass] : registrations) { citizen = r.Str(); pass = r.U8() != 0; }
		vector<string> options = ReadStrings(r);
		if (r.Failed()) break;

		// 构造函数会按spec创建门的Script(和新开局一样，脚本状态由script_save恢复)。
		Door* door = new Door(id, kind, tag, spec, zone, building, room, interactName, flippedSide,
			x, y, z, width, height, yaw, level, vehicleGate, &map.scriptFactory);
		door->access = access;
		for (auto& [citizen, pass] : registrations) door->registrations[citizen] = pass;
		// options(剧情脚本加的交互选项)：存档带脚本状态段时读档会跳过game_start广播，选项按存档
		// 恢复；没有脚本状态段(P4及更早的存档)时game_start会重新加一遍，这里不恢复，避免重复。
		if (restoreOptions) door->options = options;

		t.doors.push_back(door);
		map.allDoors.push_back(door);
		map.doorsById[id] = door;
		if (!interactName.empty()) map.doorsByInteractName[interactName] = door;
	}

	for (Zone* zone : t.zones) {
		uint32_t n = r.U32();
		for (uint32_t i = 0; i < n && !r.Failed(); i++) if (Door* door = t.DoorAt(r.I32())) zone->doorEntities.push_back(door);
	}
	for (Building* building : t.buildings) {
		uint32_t n = r.U32();
		for (uint32_t i = 0; i < n && !r.Failed(); i++) if (Door* door = t.DoorAt(r.I32())) building->doorEntities.push_back(door);
		for (Room* room : building->rooms) {
			uint32_t m = r.U32();
			for (uint32_t i = 0; i < m && !r.Failed(); i++) if (Door* door = t.DoorAt(r.I32())) room->doorEntities.push_back(door);
		}
	}

	if (r.Failed()) {
		error = "structures section is truncated";
		return false;
	}
	// 实体脚本(Zone/Building/Room)：快照字段已读回，补建Script，脚本状态由script_save恢复。
	map.BuildAllEntityScripts();
	return true;
}

// ======== 植被 ========

void MapSaveCodec::WriteVegetation(const Map& map, ByteWriter& w) {
	const Vegetation& v = map.vegetation;
	WriteStrings(v.meshPaths, w);
	w.U32(static_cast<uint32_t>(v.trees.size()));
	for (const TreeInstance& tree : v.trees) {
		w.I32(tree.id); w.F32(tree.x); w.F32(tree.y); w.F32(tree.yaw); w.F32(tree.scale); w.I32(tree.meshIndex);
	}
	w.I32(v.nextId);
}

bool MapSaveCodec::ReadVegetation(Map& map, ByteReader& r, string& error) {
	Vegetation& v = map.vegetation;
	v.meshPaths = ReadStrings(r);
	v.meshIndices.clear();
	for (size_t i = 0; i < v.meshPaths.size(); i++) v.meshIndices[v.meshPaths[i]] = static_cast<int>(i);
	v.trees.resize(r.U32());
	for (TreeInstance& tree : v.trees) {
		tree.id = r.I32(); tree.x = r.F32(); tree.y = r.F32(); tree.yaw = r.F32(); tree.scale = r.F32(); tree.meshIndex = r.I32();
	}
	v.nextId = r.I32();
	if (r.Failed()) {
		error = "vegetation section is truncated";
		return false;
	}
	return true;
}

// ======== 入口 ========

string MapSaveCodec::lastReport;

const string& MapSaveCodec::GetLastReport() {
	return lastReport;
}

namespace {
	string FormatReport(const char* verb, int roads, int lots, int edges, int zones, int buildings, int doors, int trees, int unresolved) {
		char buffer[512];
		snprintf(buffer, sizeof(buffer), "map %s - %d roads, %d lots, %d nav edges, %d zones, %d buildings, %d doors, %d trees, %d unresolved references",
			verb, roads, lots, edges, zones, buildings, doors, trees, unresolved);
		return buffer;
	}
}

void MapSaveCodec::Save(const Map& map, SaveArchive& archive) {
	SaveTables tables;
	BuildSaveTables(map, tables);

	auto versionOf = [](const char* name) {
		for (const SaveSectionVersion& v : GetSaveSectionVersions()) {
			if (string(v.name) == name) return v.current;
		}
		return 1;
		};

	ByteWriter terrain;
	WriteTerrain(map, terrain);
	archive.SetBinary(SAVE_SECTION_MAP_TERRAIN, versionOf(SAVE_SECTION_MAP_TERRAIN), terrain.Take());

	ByteWriter roadnet;
	WriteRoadnet(map, tables, roadnet);
	archive.SetBinary(SAVE_SECTION_MAP_ROADNET, versionOf(SAVE_SECTION_MAP_ROADNET), roadnet.Take());

	ByteWriter structures;
	WriteStructures(map, tables, structures);
	archive.SetBinary(SAVE_SECTION_MAP_STRUCTURES, versionOf(SAVE_SECTION_MAP_STRUCTURES), structures.Take());

	ByteWriter vegetation;
	WriteVegetation(map, vegetation);
	archive.SetBinary(SAVE_SECTION_MAP_VEGETATION, versionOf(SAVE_SECTION_MAP_VEGETATION), vegetation.Take());

	lastReport = FormatReport("saved", static_cast<int>(tables.roads.size()), static_cast<int>(tables.lots.size()),
		static_cast<int>(tables.edges.size()), static_cast<int>(tables.zones.size()), static_cast<int>(tables.buildings.size()),
		static_cast<int>(map.allDoors.size()), static_cast<int>(map.vegetation.trees.size()), tables.unresolved);
	debugf("Log: %s.\n", lastReport.c_str());
}

Map* MapSaveCodec::Load(const SaveArchive& archive, string& error) {
	const SaveSection* terrain = archive.Find(SAVE_SECTION_MAP_TERRAIN);
	const SaveSection* roadnet = archive.Find(SAVE_SECTION_MAP_ROADNET);
	const SaveSection* structures = archive.Find(SAVE_SECTION_MAP_STRUCTURES);
	const SaveSection* vegetation = archive.Find(SAVE_SECTION_MAP_VEGETATION);
	if (!terrain || !roadnet || !structures || !vegetation) {
		error = "the save has no map data (it was written by an older version that only saved the manifest)";
		return nullptr;
	}

	gMaxLoadedNodeId = -1;
	try {
		ByteReader terrainReader(terrain->data);
		int width = terrainReader.I32();
		int height = terrainReader.I32();
		if (terrainReader.Failed() || width <= 0 || height <= 0 || static_cast<int64_t>(width) * height > (1LL << 28)) {
			error = "invalid map size";
			return nullptr;
		}
		Map* map = new Map(width, height);
		ByteReader terrainBody(terrain->data);
		if (!ReadTerrain(*map, terrainBody, error)) return nullptr;

		LoadTables tables;
		ByteReader roadnetReader(roadnet->data);
		if (!ReadRoadnet(*map, tables, roadnetReader, error)) return nullptr;
		ByteReader structuresReader(structures->data);
		if (!ReadStructures(*map, tables, structuresReader, archive.Find(SAVE_SECTION_SCRIPTS) != nullptr, error)) return nullptr;
		ByteReader vegetationReader(vegetation->data);
		if (!ReadVegetation(*map, vegetationReader, error)) return nullptr;

		lastReport = FormatReport("loaded", static_cast<int>(tables.roads.size()), static_cast<int>(tables.lots.size()),
			static_cast<int>(tables.edges.size()), static_cast<int>(tables.zones.size()), static_cast<int>(tables.buildings.size()),
			static_cast<int>(tables.doors.size()), static_cast<int>(map->vegetation.trees.size()), tables.unresolved);
		debugf("Log: %s.\n", lastReport.c_str());
		return map;
	}
	catch (const exception& e) {
		error = string("map load failed: ") + e.what();
		return nullptr;
	}
	catch (...) {
		error = "map load failed with an unknown error";
		return nullptr;
	}
}
