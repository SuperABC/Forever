#include "vehicle_factory.h"


using namespace std;

void VehicleFactory::RegisterVehicle(const string& id, CreateFunc creator, DestroyFunc deleter, CategoryFunc category) {
	registries[id] = { creator, deleter, category ? string(category()) : string() };
}

string VehicleFactory::GetCategory(const string& id) const {
	if (!IsEnabled(id)) return string();
	return registries.find(id)->second.category;
}

vector<string> VehicleFactory::GetIdsByCategory(const string& category) const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		if (configuredArgs.find(id) == configuredArgs.end()) continue;
		if (entry.category == category) ids.push_back(id);
	}
	return ids;
}

VehicleMod* VehicleFactory::CreateVehicle(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	if (!IsEnabled(id)) return nullptr;

	auto argsIt = configuredArgs.find(id);
	VehicleMod* instance = it->second.creator(argsIt->second);
	if (instance) {
		liveInstances[instance] = id;
	}
	return instance;
}

void VehicleFactory::DestroyVehicle(VehicleMod* instance) {
	if (!instance)
		return;

	auto liveIt = liveInstances.find(instance);
	if (liveIt == liveInstances.end())
		return;

	auto regIt = registries.find(liveIt->second);
	if (regIt != registries.end())
		regIt->second.deleter(instance);

	liveInstances.erase(liveIt);
}

bool VehicleFactory::CheckRegistered(const string& id) const {
	return IsEnabled(id);
}

vector<string> VehicleFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		if (configuredArgs.find(id) == configuredArgs.end()) continue;
		ids.push_back(id);
	}
	return ids;
}

void VehicleFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}

bool VehicleFactory::IsEnabled(const string& id) const {
	return registries.find(id) != registries.end() && configuredArgs.find(id) != configuredArgs.end();
}

vector<pair<string, VehicleMod*>> VehicleFactory::GetLiveInstances() const {
	vector<pair<string, VehicleMod*>> result;
	result.reserve(liveInstances.size());
	for (auto& [instance, id] : liveInstances) result.emplace_back(id, instance);
	return result;
}
