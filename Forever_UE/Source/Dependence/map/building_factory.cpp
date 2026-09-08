#include "building_factory.h"

using namespace std;

void BuildingFactory::RegisterBuilding(const string& id, CreateFunc creator, DestroyFunc deleter) {
	registries[id] = { creator, deleter };
}

void BuildingFactory::CleanTemp() {
}

BuildingMod* BuildingFactory::CreateBuilding(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	BuildingMod* instance = it->second.creator();
	if (instance) {
		liveInstances[instance] = id;

		auto argsIt = configuredArgs.find(id);
		instance->ApplyArgs(argsIt != configuredArgs.end() ? argsIt->second : string());
	}
	return instance;
}

void BuildingFactory::DestroyBuilding(BuildingMod* instance) {
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

bool BuildingFactory::CheckRegistered(const string& id) const {
	return registries.find(id) != registries.end();
}

vector<string> BuildingFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		ids.push_back(id);
	}
	return ids;
}

void BuildingFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}
