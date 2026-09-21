#include "building_factory.h"


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
	return IsEnabled(id) ? it->second.randomAcreage() : 0.f;
}

float BuildingFactory::GetAcreageMin(const string& id) const {
	auto it = registries.find(id);
	return IsEnabled(id) ? it->second.acreageMin() : 0.f;
}

float BuildingFactory::GetAcreageMax(const string& id) const {
	auto it = registries.find(id);
	return IsEnabled(id) ? it->second.acreageMax() : 0.f;
}

float BuildingFactory::GetPower(const string& id, AREA_TYPE area) const {
	auto it = registries.find(id);
	return IsEnabled(id) ? it->second.power(area) : 0.f;
}

void BuildingFactory::Assign(const string& id, const vector<Lot*>& lots,
	PlacementEmitFunc emit, void* context) const {
	auto it = registries.find(id);
	if (it != registries.end() && IsEnabled(id)) {
		it->second.assign(lots, emit, context);
	}
}
