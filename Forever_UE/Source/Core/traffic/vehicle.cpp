#include "traffic/vehicle.h"

using namespace std;

Vehicle::Vehicle(VehicleFactory* factory, const string& id, const string& name) :
	factory(factory), name(name) {
	mod = factory->CreateVehicle(id);
}

Vehicle::~Vehicle() {
	if (mod) factory->DestroyVehicle(mod);
}

bool Vehicle::IsValid() const { return mod != nullptr; }

const string& Vehicle::GetName() const { return name; }

string Vehicle::GetType() const {
	return mod ? mod->GetType() : string();
}

const string& Vehicle::GetBlueprintPath() const {
	static const string empty;
	return mod ? mod->blueprintPath : empty;
}

void Vehicle::SetTransform(float inX, float inY, float inZ, float inYaw) {
	x = inX; y = inY; z = inZ; yaw = inYaw;
}

void Vehicle::GetTransform(float& outX, float& outY, float& outZ, float& outYaw) const {
	outX = x; outY = y; outZ = z; outYaw = yaw;
}
