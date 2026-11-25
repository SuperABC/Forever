#include "zone_factory.h"

#include "../common/mod_args.h"
#include "common/random.h"


using namespace std;

void ZoneFactory::RegisterZone(const string& id, CreateFunc creator, DestroyFunc deleter,
	AssignFunc assign) {
	registries[id] = { creator, deleter, assign };
}

ZoneMod* ZoneFactory::CreateZone(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	if (!IsEnabled(id)) return nullptr;

	auto argsIt = configuredArgs.find(id);
	ZoneMod* instance = it->second.creator(argsIt->second);
	if (instance) {
		liveInstances[instance] = id;
		RandomRegistry::BindInstance(instance, id);
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

	RandomRegistry::UnbindInstance(instance);
	liveInstances.erase(liveIt);
}

bool ZoneFactory::CheckRegistered(const string& id) const {
	return IsEnabled(id);
}

vector<string> ZoneFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		if (configuredArgs.find(id) == configuredArgs.end()) continue;
		ids.push_back(id);
	}
	return ids;
}

void ZoneFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}

int ZoneFactory::UnregisterExcept(const unordered_set<string>& keepIds) {
	int removed = 0;
	for (auto it = registries.begin(); it != registries.end();) {
		if (keepIds.count(it->first)) {
			++it;
			continue;
		}
		it = registries.erase(it);
		removed++;
	}
	return removed;
}

bool ZoneFactory::IsEnabled(const string& id) const {
	return registries.find(id) != registries.end() && configuredArgs.find(id) != configuredArgs.end();
}

void ZoneFactory::Assign(const string& id, const vector<Lot*>& lots,
	PlacementEmitFunc emit, void* context) const {
	auto it = registries.find(id);
	if (it != registries.end() && IsEnabled(id)) {
		// --skip：只跳过Assign(显式占位)，mod仍启用——按权重分布(GetPower/FillRemainder)和路网预设落地都不受影响。
		if (ModArgs(configuredArgs.at(id)).GetBool("skip")) return;
		RandomScope scope(RandomRegistry::GetModStream(id));
		it->second.assign(lots, emit, context);
	}
}

vector<pair<string, ZoneMod*>> ZoneFactory::GetLiveInstances() const {
	vector<pair<string, ZoneMod*>> result;
	result.reserve(liveInstances.size());
	for (auto& [instance, id] : liveInstances) result.emplace_back(id, instance);
	return result;
}
