#include "building_factory.h"
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
	BuildingMod* instance = it->second.creator(argsIt->second);
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

bool BuildingFactory::IsEnabled(const string& id) const {
	return registries.find(id) != registries.end() && configuredArgs.find(id) != configuredArgs.end();
}

float BuildingFactory::RandomAcreage(const string& id) const {
	auto it = registries.find(id);
	if (!IsEnabled(id)) return 0.f;
	RandomScope scope(RandomRegistry::GetModStream(id));
	return it->second.randomAcreage();
}

float BuildingFactory::GetAcreageMin(const string& id) const {
	auto it = registries.find(id);
	if (!IsEnabled(id)) return 0.f;
	RandomScope scope(RandomRegistry::GetModStream(id));
	return it->second.acreageMin();
}

float BuildingFactory::GetAcreageMax(const string& id) const {
	auto it = registries.find(id);
	if (!IsEnabled(id)) return 0.f;
	RandomScope scope(RandomRegistry::GetModStream(id));
	return it->second.acreageMax();
}

float BuildingFactory::GetPower(const string& id, AREA_TYPE area) const {
	auto it = registries.find(id);
	if (!IsEnabled(id)) return 0.f;
	RandomScope scope(RandomRegistry::GetModStream(id));
	return it->second.power(area);
}

void BuildingFactory::Assign(const string& id, const vector<Lot*>& lots,
	PlacementEmitFunc emit, void* context) const {
	auto it = registries.find(id);
	if (it != registries.end() && IsEnabled(id)) {
		RandomScope scope(RandomRegistry::GetModStream(id));
		it->second.assign(lots, emit, context);
	}
}

vector<pair<string, BuildingMod*>> BuildingFactory::GetLiveInstances() const {
	vector<pair<string, BuildingMod*>> result;
	result.reserve(liveInstances.size());
	for (auto& [instance, id] : liveInstances) result.emplace_back(id, instance);
	return result;
}
