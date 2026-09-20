#include "zone_factory.h"


using namespace std;

void ZoneFactory::RegisterZone(const string& id, CreateFunc creator, DestroyFunc deleter,
	AssignFunc assign) {
	registries[id] = { creator, deleter, assign };
}

ZoneMod* ZoneFactory::CreateZone(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	auto argsIt = configuredArgs.find(id);
	ZoneMod* instance = it->second.creator(argsIt != configuredArgs.end() ? argsIt->second : string());
	if (instance) {
		liveInstances[instance] = id;
	}
	return instance;
}

void ZoneFactory::DestroyZone(ZoneMod* instance) {
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

bool ZoneFactory::CheckRegistered(const string& id) const {
	return registries.find(id) != registries.end();
}

vector<string> ZoneFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		ids.push_back(id);
	}
	return ids;
}

void ZoneFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}

void ZoneFactory::Assign(const string& id, const vector<Lot*>& lots,
	PlacementEmitFunc emit, void* context) const {
	auto it = registries.find(id);
	if (it != registries.end()) {
		it->second.assign(lots, emit, context);
	}
}
