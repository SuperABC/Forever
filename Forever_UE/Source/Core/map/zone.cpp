#include "zone.h"

#include "common/error.h"

using namespace std;

Zone::Zone(ZoneFactory* factory, const string& zoneId) :
	Quad(),
	mod(factory->CreateZone(zoneId)),
	factory(factory),
	type(),
	name() {
	if (!mod) {
		THROW_EXCEPTION(NullPointerException, "Zone " + zoneId + " mod is null.\n");
	}

	type = mod->GetType();
	name = mod->GetName();
}

Zone::~Zone() {
	factory->DestroyZone(mod);
}

string Zone::GetType() const {
	return type;
}

string Zone::GetName() const {
	return name;
}

float Zone::GetRotation() const {
	return rotation;
}

void Zone::SetRotation(float r) {
	rotation = r;
}

Lot* Zone::GetParentLot() const {
	return parentLot;
}

void Zone::SetParentLot(Lot* lot) {
	parentLot = lot;
}
