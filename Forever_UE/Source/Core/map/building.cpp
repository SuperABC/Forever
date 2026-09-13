#include "building.h"

#include "common/error.h"

#include <algorithm>

using namespace std;

namespace {
	constexpr float kDefaultFloorHeight = 0.4f; // 老工程Hotel同款兜底值，长度对不上时用它补齐
}

Building::Building(BuildingFactory* factory, BuildingMod* mod) :
	Quad(),
	mod(mod),
	factory(factory),
	type(),
	name() {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Building mod is null.\n");
	}

	type = mod->GetType();
	name = mod->GetName();
}

Building::~Building() {
	factory->DestroyBuilding(mod);
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

void Building::Layout(int direction) {
	mod->Layout(direction, *this, GetBoundaryRoads());

	const BuildingFootprintSpec& fp = mod->footprint;
	bodyOffsetX = (fp.centerRatioX - 0.5f) * GetSizeX();
	bodyOffsetY = (fp.centerRatioY - 0.5f) * GetSizeY();
	bodySizeX = fp.sizeRatioX * GetSizeX();
	bodySizeY = fp.sizeRatioY * GetSizeY();

	basements = std::max(0, mod->basements);
	layers = std::max(1, mod->layers);
	int expected = basements + layers;
	floorHeights = mod->floorHeights;
	if (static_cast<int>(floorHeights.size()) != expected) {
		floorHeights.assign(expected, kDefaultFloorHeight);
	}
	lodMaterialPath = mod->lodMaterial;
}

float Building::GetBodyOffsetX() const {
	return bodyOffsetX;
}

float Building::GetBodyOffsetY() const {
	return bodyOffsetY;
}

float Building::GetBodySizeX() const {
	return bodySizeX;
}

float Building::GetBodySizeY() const {
	return bodySizeY;
}

int Building::GetBasementCount() const {
	return basements;
}

int Building::GetLayerCount() const {
	return layers;
}

const vector<float>& Building::GetFloorHeights() const {
	return floorHeights;
}

const string& Building::GetLodMaterialPath() const {
	return lodMaterialPath;
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
