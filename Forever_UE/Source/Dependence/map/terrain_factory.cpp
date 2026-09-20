#include "terrain_factory.h"


using namespace std;

void TerrainFactory::RegisterTerrain(const string& id, CreateFunc creator, DestroyFunc deleter) {
	registries[id] = { creator, deleter };
}

TerrainMod* TerrainFactory::CreateTerrain(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	auto argsIt = configuredArgs.find(id);
	TerrainMod* instance = it->second.creator(argsIt != configuredArgs.end() ? argsIt->second : string());
	if (instance) {
		liveInstances[instance] = id;
	}
	return instance;
}

void TerrainFactory::DestroyTerrain(TerrainMod* instance) {
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

bool TerrainFactory::CheckRegistered(const string& id) const {
	return registries.find(id) != registries.end();
}

vector<string> TerrainFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		ids.push_back(id);
	}
	return ids;
}

void TerrainFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}
