#include "building.h"

#include "common/error.h"

using namespace std;

Building::Building(BuildingFactory* factory, const string& buildingId) :
	Quad(),
	mod(factory->CreateBuilding(buildingId)),
	factory(factory),
	type(),
	name() {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Building " + buildingId + " mod is null.\n");
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

float Building::GetRotation() const {
	return parentLot ? parentLot->GetRotation() : 0.f;
}

Lot* Building::GetParentLot() const {
	return parentLot;
}

void Building::SetParentLot(Lot* lot) {
	parentLot = lot;
}
