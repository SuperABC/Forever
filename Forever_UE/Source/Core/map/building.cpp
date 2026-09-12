#include "building.h"

#include "common/error.h"

using namespace std;

Building::Building(BuildingMod* mod) :
	Quad(),
	mod(mod),
	type(),
	name() {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Building mod is null.\n");
	}

	type = mod->GetType();
	name = mod->GetName();
}

Building::~Building() {
	// 不在这里DestroyBuilding(mod)——这个mod实例按类型共享，可能同时被好几个Building指着，
	// 生命周期由Map::InitBuildings()自己的scanners表统一持有/销毁，见building.h注释。
}

string Building::GetType() const {
	return type;
}

string Building::GetName() const {
	return name;
}

BuildingMod* Building::GetMod() const {
	return mod;
}

float Building::GetRotation() const {
	return (parentLot ? parentLot->GetRotation() : 0.f) + relativeRotation;
}

Lot* Building::GetParentLot() const {
	return parentLot;
}

void Building::SetParentLot(Lot* lot, float relativeRot) {
	parentLot = lot;
	relativeRotation = relativeRot;
}

Zone* Building::GetParentZone() const {
	return parentZone;
}

void Building::SetParentZone(Zone* zone) {
	parentZone = zone;
}

void Building::SetBoundaryRoad(int direction, Road* road) {
	boundaryRoads[direction] = road;
}

Road* Building::GetBoundaryRoad(int direction) const {
	auto it = boundaryRoads.find(direction);
	return it != boundaryRoads.end() ? it->second : nullptr;
}

const unordered_map<int, Road*>& Building::GetBoundaryRoads() const {
	return boundaryRoads;
}
