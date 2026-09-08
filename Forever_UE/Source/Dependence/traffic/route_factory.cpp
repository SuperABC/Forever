#include "route_factory.h"

using namespace std;

void RouteFactory::RegisterRoute(const string& id, CreateFunc creator, DestroyFunc deleter) {
	registries[id] = { creator, deleter };
}

void RouteFactory::CleanTemp() {
}

RouteMod* RouteFactory::CreateRoute(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	RouteMod* instance = it->second.creator();
	if (instance) {
		liveInstances[instance] = id;

		auto argsIt = configuredArgs.find(id);
		instance->ApplyArgs(argsIt != configuredArgs.end() ? argsIt->second : string());
	}
	return instance;
}

void RouteFactory::DestroyRoute(RouteMod* instance) {
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

bool RouteFactory::CheckRegistered(const string& id) const {
	return registries.find(id) != registries.end();
}

vector<string> RouteFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		ids.push_back(id);
	}
	return ids;
}

void RouteFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}
