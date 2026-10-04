#include "traffic/station.h"

#include "map/building.h"
#include "map/map.h"

#include <cmath>

using namespace std;

Station::Station(StationFactory* factory, const string& modId, const string& name, Building* inBuilding) :
	factory(factory), name(name), building(inBuilding) {
	mod = factory->CreateStation(modId);
	if (!mod || !building) return;

	int direction = building->GetDirection();
	mod->Layout(direction, building->GetSizeX(), building->GetSizeY());

	float quadSizeX = building->GetSizeX();
	float quadSizeY = building->GetSizeY();
	float buildingRotation = building->GetRotation(); // 弧度

	for (const StationInterfaceSpec& spec : mod->interfaces) {
		// 占地矩形局部坐标(原点左下角) -> 楼体局部坐标(LocalToWorld要求的输入空间)，公式见
		// station.md"接口坐标换算"一节：先换到"相对占地矩形中心"，再换算成"相对楼体中心"
		// (加楼体尺寸一半、减bodyOffset)，这样才能喂给只认楼体局部坐标的LocalToWorld。
		float quadLocalX = spec.position[0] * quadSizeX + spec.position[1];
		float quadLocalY = spec.position[2] * quadSizeY + spec.position[3];
		float bodyLocalX = quadLocalX - quadSizeX * 0.5f + building->GetBodySizeX() * 0.5f - building->GetBodyOffsetX();
		float bodyLocalY = quadLocalY - quadSizeY * 0.5f + building->GetBodySizeY() * 0.5f - building->GetBodyOffsetY();

		auto [worldX, worldY] = building->LocalToWorld(bodyLocalX, bodyLocalY);
		float worldZ = building->GetFloorBaseZ(0) + spec.z;

		float heading = buildingRotation + spec.headingDegrees * (3.14159265358979323846f / 180.f);
		float dirX = cosf(heading);
		float dirY = sinf(heading);

		StationInterface iface;
		iface.x = worldX; iface.y = worldY; iface.z = worldZ;
		iface.arriveDirX = dirX; iface.arriveDirY = dirY;
		iface.departDirX = dirX; iface.departDirY = dirY;
		interfaces.push_back(iface);
	}
}

Station::Station(const string& name, float x, float y, float z,
	float arriveDirX, float arriveDirY, float departDirX, float departDirY) :
	name(name), edge(true) {
	StationInterface iface;
	iface.x = x; iface.y = y; iface.z = z;
	iface.arriveDirX = arriveDirX; iface.arriveDirY = arriveDirY;
	iface.departDirX = departDirX; iface.departDirY = departDirY;
	interfaces.push_back(iface);
}

Station::Station(StationFactory* factory, const string& modId, const string& name,
	float worldX, float worldY, float worldZ, float headingDegrees, Road* attachedRoad) :
	factory(factory), name(name), attachedRoad(attachedRoad) {
	mod = factory->CreateStation(modId);
	if (!mod) return;

	// 没有building，不走mod->Layout()/mod->interfaces那套相对建筑占地矩形的换算——世界坐标/
	// 朝向都是调用方(Traffic::InitRoadsideStations)已经按道路几何算好的，直接用，"普通站点的
	// 两个方向相同"(见station_mod.h StationInterfaceSpec注释)这次也一样，arrive==depart。
	float heading = headingDegrees * (3.14159265358979323846f / 180.f);
	float dirX = cosf(heading);
	float dirY = sinf(heading);

	StationInterface iface;
	iface.x = worldX; iface.y = worldY; iface.z = worldZ;
	iface.arriveDirX = dirX; iface.arriveDirY = dirY;
	iface.departDirX = dirX; iface.departDirY = dirY;
	interfaces.push_back(iface);
}

Station::~Station() {
	for (auto& [index, link] : roadLinks) {
		delete link.inEdge;
		delete link.outEdge;
		delete link.stationNode;
	}
	if (mod) factory->DestroyStation(mod);
}

bool Station::IsValid() const { return edge || mod != nullptr; }

const string& Station::GetName() const { return name; }

const string& Station::GetStationType() const {
	static const string empty;
	return mod ? mod->stationType : empty;
}

bool Station::IsEdge() const { return edge; }

Building* Station::GetBuilding() const { return building; }

const vector<StationInterface>& Station::GetInterfaces() const { return interfaces; }

const StationRoadLink* Station::EnsureRoadLink(Map* map, int interfaceIndex) {
	auto it = roadLinks.find(interfaceIndex);
	if (it != roadLinks.end()) return &it->second;

	if (!map) return nullptr;
	if (interfaceIndex < 0 || interfaceIndex >= static_cast<int>(interfaces.size())) return nullptr;

	Road* road = building ? building->GetBoundaryRoad(building->GetDirection()) : attachedRoad;
	if (!road) return nullptr;

	const StationInterface& iface = interfaces[interfaceIndex];
	Map::LaneSegment segment;
	if (!map->GetLaneSegmentAt(road, iface.x, iface.y, iface.departDirX, iface.departDirY, segment)) {
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

	auto result = roadLinks.emplace(interfaceIndex, link);
	return &result.first->second;
}

const StationRoadLink* Station::GetRoadLink(int interfaceIndex) const {
	auto it = roadLinks.find(interfaceIndex);
	return it != roadLinks.end() ? &it->second : nullptr;
}
