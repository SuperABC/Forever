#include "roadnet_factory.h"

using namespace std;

void RoadnetFactory::RegisterRoadnet(const string& id, CreateFunc creator, DestroyFunc deleter) {
	registries[id] = { creator, deleter };
}

void RoadnetFactory::CleanTemp() {
}

RoadnetMod* RoadnetFactory::CreateRoadnet(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	RoadnetMod* instance = it->second.creator();
	if (instance) {
		liveInstances[instance] = id;

		auto argsIt = configuredArgs.find(id);
		instance->ApplyArgs(argsIt != configuredArgs.end() ? argsIt->second : string());
	}
	return instance;
}

void RoadnetFactory::DestroyRoadnet(RoadnetMod* instance) {
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

bool RoadnetFactory::CheckRegistered(const string& id) const {
	return registries.find(id) != registries.end();
}

vector<string> RoadnetFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		ids.push_back(id);
	}
	return ids;
}

void RoadnetFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}

void RoadnetFactory::SetConfig(const string& id, bool enabled) {
	enabledConfig[id] = enabled;
}

string RoadnetFactory::GetRoadnet() const {
	for (const auto& [id, enabled] : enabledConfig) {
		if (enabled) return id;
	}
	return string();
}
