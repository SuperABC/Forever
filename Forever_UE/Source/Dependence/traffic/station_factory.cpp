#include "station_factory.h"

#include "common/error.h"

#include "common/random.h"


using namespace std;

void StationFactory::RegisterStation(const string& id, CreateFunc creator, DestroyFunc deleter,
	RoadAssignFunc assign) {
	registries[id] = { creator, deleter, assign };
}

StationMod* StationFactory::CreateStation(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	if (!IsEnabled(id)) return nullptr;

	auto argsIt = configuredArgs.find(id);
	StationMod* instance = MOD_ID_CALL(it->first, it->second.creator(argsIt->second));
	if (instance) {
		liveInstances[instance] = id;
		RandomRegistry::BindInstance(instance, id);
	}
	return instance;
}

void StationFactory::DestroyStation(StationMod* instance) {
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

bool StationFactory::CheckRegistered(const string& id) const {
	return IsEnabled(id);
}

vector<string> StationFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		if (configuredArgs.find(id) == configuredArgs.end()) continue;
		ids.push_back(id);
	}
	return ids;
}

void StationFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}

int StationFactory::UnregisterExcept(const unordered_set<string>& keepIds) {
	int removed = 0;
	for (auto it = registries.begin(); it != registries.end();) {
		if (keepIds.count(it->first)) {
			++it;
			continue;
		}
		it = registries.erase(it);
		removed++;
	}
	for (auto it = manualAssigns.begin(); it != manualAssigns.end();) {
		if (keepIds.count(it->first)) ++it;
		else it = manualAssigns.erase(it);
	}
	return removed;
}

bool StationFactory::IsEnabled(const string& id) const {
	return registries.find(id) != registries.end() && configuredArgs.find(id) != configuredArgs.end();
}

void StationFactory::AssignRoads(const string& id, const vector<Road*>& roads,
	RoadStationEmitFunc emit, void* context) const {
	auto it = registries.find(id);
	if (it != registries.end() && IsEnabled(id)) {
		MOD_ID_CALL(it->first, it->second.assign(roads, emit, context));
	}
}

void StationFactory::RegisterManualAssign(const string& id, ManualAssignFunc assign) {
	manualAssigns[id] = assign;
}

bool StationFactory::HasManualAssign(const string& id) const {
	auto it = manualAssigns.find(id);
	return it != manualAssigns.end() && it->second && IsEnabled(id);
}

void StationFactory::AssignManualRoads(const string& id, const vector<Road*>& roads, const TransitData& data,
	RoadStationEmitFunc emit, void* context) const {
	if (!HasManualAssign(id)) return;
	manualAssigns.at(id)(roads, data, emit, context);
}

vector<pair<string, StationMod*>> StationFactory::GetLiveInstances() const {
	vector<pair<string, StationMod*>> result;
	result.reserve(liveInstances.size());
	for (auto& [instance, id] : liveInstances) result.emplace_back(id, instance);
	return result;
}
