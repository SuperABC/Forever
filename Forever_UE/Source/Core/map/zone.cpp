#include "zone.h"

#include "common/error.h"

#include "map/door.h"
#include "map/map.h"

#include <cmath>


using namespace std;

namespace {
	constexpr float kPi = 3.14159265358979323846f;
	// 这次没有在ZoneGateSpec里加高度字段，先给一个固定值，见door_system_plan.md门资产规格
	// 一节"园区大门约3m*2m"的建议尺寸。单位是地图单位，1地图单位=10米(DOOR_WORLD_SCALE/
	// BUILDING_WORLD_SCALE=1000)——最初直接写成2.5(当成"2.5米"理解)，实际是25米高的园区
	// 大门，实测复现(门离地/离人体尺度老远)，这里改成0.25地图单位=2.5米。
	constexpr float kZoneGateHeight = 0.25f;

	// 和Building::BuildDoors()用的normalYaw表同一套约定(EAST=0/SOUTH=π/2/WEST=π/
	// NORTH=-π/2)，这里独立一份——两处都是很小的自包含表，没有共享的必要，和map.cpp/
	// roadnet.cpp里LaneCenterOffset/SumWidths同样处理方式。
	float FaceNormalYaw(int direction) {
		switch (direction) {
		case FACE_EAST: return 0.f;
		case FACE_SOUTH: return kPi * 0.5f;
		case FACE_WEST: return kPi;
		default: return -kPi * 0.5f; // FACE_NORTH
		}
	}
}

Zone::Zone(ZoneFactory* factory, ZoneMod* mod) :
	Quad(),
	mod(mod),
	factory(factory),
	type(),
	name() {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Zone mod is null.\n");
	}

	type = mod->GetType();
	name = mod->GetName();
}

Zone::~Zone() {
	for (Road* road : internalRoads) {
		delete road;
	}
	for (Door* door : doorEntities) {
		delete door;
	}
	factory->DestroyZone(mod);
}

string Zone::GetType() const {
	return type;
}

string Zone::GetName() const {
	return name;
}

ZoneMod* Zone::GetMod() const {
	return mod;
}

float Zone::GetRotation() const {
	return parentLot ? parentLot->GetRotation() : 0.f;
}

string Zone::GetAddress() const {
	if (!parentLot) return "";
	return parentLot->GetAddress() + " " + GetName();
}

void Zone::Layout(int direction) {
	mod->Layout(direction, *this, GetBoundaryRoads());
}

Lot* Zone::GetParentLot() const {
	return parentLot;
}

void Zone::SetParentLot(Lot* lot) {
	parentLot = lot;
}

void Zone::SetBoundaryRoad(int direction, Road* road) {
	boundaryRoads[direction] = road;
}

Road* Zone::GetBoundaryRoad(int direction) const {
	auto it = boundaryRoads.find(direction);
	return it != boundaryRoads.end() ? it->second : nullptr;
}

const unordered_map<int, Road*>& Zone::GetBoundaryRoads() const {
	return boundaryRoads;
}

const vector<ZoneWallSpec>& Zone::GetWalls() const {
	return mod->walls;
}

const vector<ZoneGateSpec>& Zone::GetGates() const {
	return mod->gates;
}

void Zone::SetInternalRoads(const vector<Road*>& roads) {
	internalRoads = roads;
}

const vector<Road*>& Zone::GetInternalRoads() const {
	return internalRoads;
}

void Zone::AddInternalBuilding(Building* building) {
	internalBuildings.push_back(building);
}

const vector<Building*>& Zone::GetInternalBuildings() const {
	return internalBuildings;
}

Citizen* Zone::GetOwner() const { return owner; }
void Zone::SetOwner(Citizen* value) { owner = value; }
bool Zone::GetStated() const { return stated; }
void Zone::SetStated(bool value) { stated = value; }

void Zone::BuildDoors(Map& map) {
	float halfX = GetSizeX() * 0.5f;
	float halfY = GetSizeY() * 0.5f;
	float rot = GetRotation();
	float cosR = cosf(rot), sinR = sinf(rot);

	auto localToWorld = [&](float lx, float ly) -> pair<float, float> {
		return { GetPosX() + lx * cosR - ly * sinR, GetPosY() + lx * sinR + ly * cosR };
		};

	// 判定一个局部坐标点离哪条边最近——和Map::ConnectZoneAccessPoint同一套"按局部距离找
	// 最近边"的思路，IsVehicleGate用这个给vehicleEntries/vehicleExits的出入口点找归属边。
	auto nearestFace = [&](float x, float y) -> int {
		float distWest = fabsf(x - (-halfX));
		float distEast = fabsf(x - halfX);
		float distNorth = fabsf(y - (-halfY));
		float distSouth = fabsf(y - halfY);
		float minDist = distWest;
		int face = FACE_WEST;
		if (distEast < minDist) { minDist = distEast; face = FACE_EAST; }
		if (distNorth < minDist) { minDist = distNorth; face = FACE_NORTH; }
		if (distSouth < minDist) { minDist = distSouth; face = FACE_SOUTH; }
		return face;
		};

	for (const ZoneGateSpec& gate : mod->gates) {
		if (gate.door.mesh.empty()) continue;

		bool alongY = (gate.direction == FACE_WEST || gate.direction == FACE_EAST);
		float edgeX = (gate.direction == FACE_WEST) ? -halfX : (gate.direction == FACE_EAST ? halfX : 0.f);
		float edgeY = (gate.direction == FACE_NORTH) ? -halfY : (gate.direction == FACE_SOUTH ? halfY : 0.f);

		float inwardX = 0.f, inwardY = 0.f;
		if (gate.direction == FACE_WEST) inwardX = 1.f;
		else if (gate.direction == FACE_EAST) inwardX = -1.f;
		else if (gate.direction == FACE_NORTH) inwardY = 1.f;
		else inwardY = -1.f;
		float depthDirX = gate.depthInward ? inwardX : -inwardX;
		float depthDirY = gate.depthInward ? inwardY : -inwardY;

		float centerAcrossX = edgeX + depthDirX * gate.depth;
		float centerAcrossY = edgeY + depthDirY * gate.depth;

		float lenStart = alongY ? (-halfY + gate.marginStart) : (-halfX + gate.marginStart);
		float lenEnd = alongY ? (halfY - gate.marginEnd) : (halfX - gate.marginEnd);
		if (lenEnd <= lenStart) continue;

		float alongCenter = (lenStart + lenEnd) * 0.5f;
		float lx = alongY ? centerAcrossX : alongCenter;
		float ly = alongY ? alongCenter : centerAcrossY;
		pair<float, float> worldPos = localToWorld(lx, ly);

		bool isVehicleGate = false;
		auto overlaps = [&](const ZoneAccessPoint& pt) {
			if (nearestFace(pt.x, pt.y) != gate.direction) return false;
			float ptAlong = alongY ? pt.y : pt.x;
			float ptStart = ptAlong - pt.width * 0.5f;
			float ptEnd = ptAlong + pt.width * 0.5f;
			return ptEnd >= lenStart && ptStart <= lenEnd;
			};
		for (const ZoneAccessPoint& pt : mod->vehicleEntries) {
			if (overlaps(pt)) { isVehicleGate = true; break; }
		}
		if (!isVehicleGate) {
			for (const ZoneAccessPoint& pt : mod->vehicleExits) {
				if (overlaps(pt)) { isVehicleGate = true; break; }
			}
		}

		Door* door = map.CreateDoor(DOOR_KIND_ZONE, this, nullptr, nullptr, string(), gate.door,
			worldPos.first, worldPos.second, 0.f, lenEnd - lenStart, kZoneGateHeight,
			FaceNormalYaw(gate.direction) + rot, 0, isVehicleGate);
		if (door) doorEntities.push_back(door);
	}
}

const vector<Door*>& Zone::GetDoorEntities() const { return doorEntities; }
