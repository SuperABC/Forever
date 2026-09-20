#include "zone.h"

#include "common/error.h"


using namespace std;

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
