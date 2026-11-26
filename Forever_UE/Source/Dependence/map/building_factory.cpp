#include "building_factory.h"

#include "common/error.h"

#include "../common/mod_args.h"
#include "common/random.h"


using namespace std;

void BuildingFactory::RegisterBuilding(const string& id, CreateFunc creator, DestroyFunc deleter,
	RandomAcreageFunc randomAcreage, AcreageBoundFunc acreageMin, AcreageBoundFunc acreageMax,
	PowerFunc power, AssignFunc assign) {
	registries[id] = { creator, deleter, randomAcreage, acreageMin, acreageMax, power, assign };
}

BuildingMod* BuildingFactory::CreateBuilding(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	if (!IsEnabled(id)) return nullptr;

	auto argsIt = configuredArgs.find(id);
	BuildingMod* instance = MOD_ID_CALL(it->first, it->second.creator(argsIt->second));
	if (instance) {
		liveInstances[instance] = id;
		RandomRegistry::BindInstance(instance, id);
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

	RandomRegistry::UnbindInstance(instance);
	liveInstances.erase(liveIt);
}

bool BuildingFactory::CheckRegistered(const string& id) const {
	return IsEnabled(id);
}

vector<string> BuildingFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		if (configuredArgs.find(id) == configuredArgs.end()) continue;
		ids.push_back(id);
	}
	return ids;
}

void BuildingFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}

int BuildingFactory::UnregisterExcept(const unordered_set<string>& keepIds) {
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

bool BuildingFactory::IsEnabled(const string& id) const {
	return registries.find(id) != registries.end() && configuredArgs.find(id) != configuredArgs.end();
}

float BuildingFactory::RandomAcreage(const string& id) const {
	auto it = registries.find(id);
	if (!IsEnabled(id)) return 0.f;
	RandomScope scope(RandomRegistry::GetModStream(id));
	return MOD_ID_CALL(it->first, it->second.randomAcreage());
}

float BuildingFactory::GetAcreageMin(const string& id) const {
	auto it = registries.find(id);
	if (!IsEnabled(id)) return 0.f;
	RandomScope scope(RandomRegistry::GetModStream(id));
	return MOD_ID_CALL(it->first, it->second.acreageMin());
}

float BuildingFactory::GetAcreageMax(const string& id) const {
	auto it = registries.find(id);
	if (!IsEnabled(id)) return 0.f;
	RandomScope scope(RandomRegistry::GetModStream(id));
	return MOD_ID_CALL(it->first, it->second.acreageMax());
}

float BuildingFactory::GetPower(const string& id, AREA_TYPE area) const {
	auto it = registries.find(id);
	if (!IsEnabled(id) || area == AREA_PLACEHOLDER) return 0.f;   // 占位分区永远没有权重
	RandomScope scope(RandomRegistry::GetModStream(id));
	return MOD_ID_CALL(it->first, it->second.power(area));
}

void BuildingFactory::Assign(const string& id, const vector<Lot*>& lots,
	PlacementEmitFunc emit, void* context) const {
	auto it = registries.find(id);
	if (it != registries.end() && IsEnabled(id)) {
		// --skip：只跳过Assign(显式占位)，mod仍启用——按权重分布(GetPower/FillRemainder)和路网预设落地都不受影响。
		if (ModArgs(configuredArgs.at(id)).GetBool("skip")) return;
		RandomScope scope(RandomRegistry::GetModStream(id));
		MOD_ID_CALL(it->first, it->second.assign(lots, emit, context));
	}
}

vector<pair<string, BuildingMod*>> BuildingFactory::GetLiveInstances() const {
	vector<pair<string, BuildingMod*>> result;
	result.reserve(liveInstances.size());
	for (auto& [instance, id] : liveInstances) result.emplace_back(id, instance);
	return result;
}
