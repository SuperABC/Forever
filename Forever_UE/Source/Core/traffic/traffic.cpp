#include "traffic.h"

#include "common/registry.h"

using namespace std;

Traffic::Traffic() :
	vehicleFactory(Registry::Get().GetVehicleFactory()) {
}

Traffic::~Traffic() {
	for (auto& [name, vehicle] : vehicles) {
		delete vehicle;
	}
}

Vehicle* Traffic::CreateVehicle(const string& modId, const string& name) {
	auto it = vehicles.find(name);
	if (it != vehicles.end()) {
		delete it->second;
		vehicles.erase(it);
	}

	Vehicle* vehicle = new Vehicle(&vehicleFactory, modId, name);
	if (!vehicle->IsValid()) {
		delete vehicle;
		return nullptr;
	}

	vehicles.insert_or_assign(name, vehicle);
	return vehicle;
}

void Traffic::DestroyVehicle(const string& name) {
	auto it = vehicles.find(name);
	if (it == vehicles.end()) return;
	delete it->second;
	vehicles.erase(it);
}

Vehicle* Traffic::FindVehicleByName(const string& name) const {
	auto it = vehicles.find(name);
	return it != vehicles.end() ? it->second : nullptr;
}

const unordered_map<string, Vehicle*>& Traffic::GetVehicles() const { return vehicles; }

void Traffic::Tick(const Time& currentTime, bool crossedDay, PostHandle* post) {
	// 占位，等Traffic域真的有需要每帧处理的逻辑时再补，见traffic.h声明处注释。
}

void Traffic::ApplyChange(const Change* change, const ScriptContext& context) {
	// 占位，等Traffic域真的有需要处理的Change子类时再补，见traffic.h声明处注释。
}
