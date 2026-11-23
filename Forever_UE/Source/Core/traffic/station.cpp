#include "traffic/station.h"

#include "map/building.h"
#include "map/map.h"

#include <cmath>

using namespace std;

Station::Station(StationFactory* factory, const string& modId, const string& name, Building* inBuilding) :
	factory(factory), name(name), building(inBuilding) {
	mod = factory->CreateStation(modId);
	if (!mod || !building) return;

	mod->SetProperty();
	int direction = building->GetDirection();
	mod->Layout(direction, building->GetSizeX(), building->GetSizeY());

	float quadSizeX = building->GetSizeX();
	float quadSizeY = building->GetSizeY();
	float buildingRotation = building->GetRotation(); // 弧度
	float baseZ = building->GetFloorWorldZ(0);

	// 占地矩形局部坐标(原点左下角) -> 世界xy。公式见station.md"接口坐标换算"一节：先换到"相对占地矩形中心"，
	// 再换算成"相对楼体中心"(加楼体尺寸一半、减bodyOffset)，这样才能喂给只认楼体局部坐标的LocalToWorld。
	auto toWorld = [&](const PointParams& point) {
		float quadLocalX = point[0] * quadSizeX + point[1];
		float quadLocalY = point[2] * quadSizeY + point[3];
		float bodyLocalX = quadLocalX - quadSizeX * 0.5f + building->GetBodySizeX() * 0.5f - building->GetBodyOffsetX();
		float bodyLocalY = quadLocalY - quadSizeY * 0.5f + building->GetBodySizeY() * 0.5f - building->GetBodyOffsetY();
		return building->LocalToWorld(bodyLocalX, bodyLocalY);
	};
	auto convertPorts = [&](const vector<StationPortSpec>& specs, vector<StationPort>& out) {
		for (const StationPortSpec& spec : specs) {
			auto [worldX, worldY] = toWorld(spec.position);
			float heading = buildingRotation + spec.headingDegrees * (3.14159265358979323846f / 180.f);
			StationPort port;
			port.x = worldX; port.y = worldY; port.z = baseZ + spec.z;
			port.dirX = cosf(heading); port.dirY = sinf(heading);
			out.push_back(port);
		}
	};
	convertPorts(mod->inlets, inlets);
	convertPorts(mod->outlets, outlets);
	convertPorts(mod->stops, stops);

	for (const StationConnectionSpec& spec : mod->connections) {
		StationConnection connection;
		connection.fromKind = spec.fromKind; connection.fromIndex = spec.fromIndex;
		connection.toKind = spec.toKind; connection.toIndex = spec.toIndex;
		for (const StationCurveSpec& curve : spec.curves) {
			TrackSegment segment;
			auto [x1, y1] = toWorld(curve.p1);
			auto [cx1, cy1] = toWorld(curve.c1);
			auto [cx2, cy2] = toWorld(curve.c2);
			auto [x2, y2] = toWorld(curve.p2);
			segment.x1 = x1; segment.y1 = y1; segment.z1 = baseZ + curve.z1;
			segment.c1x = cx1; segment.c1y = cy1; segment.c1z = baseZ + curve.c1z;
			segment.c2x = cx2; segment.c2y = cy2; segment.c2z = baseZ + curve.c2z;
			segment.x2 = x2; segment.y2 = y2; segment.z2 = baseZ + curve.z2;
			segment.mesh = curve.mesh;
			segment.meshUnit = curve.meshUnit;
			connection.segments.push_back(std::move(segment));
		}
		connections.push_back(std::move(connection));
	}
}

Station::Station(StationFactory* factory, const string& modId, const string& name,
	float worldX, float worldY, float worldZ, float headingDegrees, Road* attachedRoad) :
	factory(factory), name(name), attachedRoad(attachedRoad) {
	mod = factory->CreateStation(modId);
	if (!mod) return;

	mod->SetProperty();

	// 没有building，不走mod->Layout()那套相对建筑占地矩形的换算——世界坐标/朝向都是调用方
	// (Traffic::InitRoadsideStations)已经按道路几何算好的，直接用；贴路站点只有一个停靠位，没有进出站口和站内连线。
	float heading = headingDegrees * (3.14159265358979323846f / 180.f);
	StationPort port;
	port.x = worldX; port.y = worldY; port.z = worldZ;
	port.dirX = cosf(heading); port.dirY = sinf(heading);
	stops.push_back(port);
}

Station::~Station() {
	for (auto& [index, link] : roadLinks) {
		if (linkedMap && link.stationNode) linkedMap->UnregisterAuxNavNode(link.stationNode->GetId());
		delete link.inEdge;
		delete link.outEdge;
		delete link.stationNode;
	}
	if (mod) factory->DestroyStation(mod);
}

bool Station::IsValid() const { return mod != nullptr; }

const string& Station::GetName() const { return name; }

const string& Station::GetStationType() const {
	static const string empty;
	return mod ? mod->stationType : empty;
}

Building* Station::GetBuilding() const { return building; }

const vector<StationPort>& Station::GetInlets() const { return inlets; }
const vector<StationPort>& Station::GetOutlets() const { return outlets; }
const vector<StationPort>& Station::GetStops() const { return stops; }
const vector<StationConnection>& Station::GetConnections() const { return connections; }

const StationRoadLink* Station::EnsureRoadLink(Map* map, int stopIndex) {
	auto it = roadLinks.find(stopIndex);
	if (it != roadLinks.end()) return &it->second;

	if (!map) return nullptr;
	if (stopIndex < 0 || stopIndex >= static_cast<int>(stops.size())) return nullptr;

	Road* road = building ? building->GetBoundaryRoad(building->GetDirection()) : attachedRoad;
	if (!road) return nullptr;

	const StationPort& port = stops[stopIndex];
	Map::LaneSegment segment;
	if (!map->GetLaneSegmentAt(road, port.x, port.y, port.dirX, port.dirY, segment)) {
		return nullptr;
	}

	StationRoadLink link;
	link.laneFrom = segment.fromAnchor;
	link.laneTo = segment.toAnchor;
	link.sourceEdge = segment.edge;
	link.t = segment.t;
	link.stationNode = new Node("station", segment.worldX, segment.worldY, segment.worldZ);
	link.inEdge = new Connection(*segment.fromAnchor, *link.stationNode);
	link.outEdge = new Connection(*link.stationNode, *segment.toAnchor);

	// 把站点登记成Map的辅助导航节点，之后Map::PlanVehicleRouteToNode/PlanVehicleLeg可以直接以stationNode的id为端点。
	Map::AuxNavNode aux;
	aux.node = link.stationNode;
	aux.laneFrom = link.laneFrom;
	aux.inEdge = link.inEdge;
	aux.laneTo = link.laneTo;
	aux.outEdge = link.outEdge;
	aux.sourceEdge = link.sourceEdge;
	aux.t = link.t;
	map->RegisterAuxNavNode(aux);
	linkedMap = map;

	auto result = roadLinks.emplace(stopIndex, link);
	return &result.first->second;
}

const StationRoadLink* Station::GetRoadLink(int stopIndex) const {
	auto it = roadLinks.find(stopIndex);
	return it != roadLinks.end() ? &it->second : nullptr;
}
