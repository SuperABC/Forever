#include "building_factory.h"

using namespace std;

void BuildingFactory::RegisterBuilding(const string& id, CreateFunc creator, DestroyFunc deleter,
	RandomAcreageFunc randomAcreage, AcreageBoundFunc acreageMin, AcreageBoundFunc acreageMax,
	PowerFunc power, AssignFunc assign) {
	registries[id] = { creator, deleter, randomAcreage, acreageMin, acreageMax, power, assign };
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

float BuildingFactory::RandomAcreage(const string& id) const {
	auto it = registries.find(id);
	return it != registries.end() ? it->second.randomAcreage() : 0.f;
}

float BuildingFactory::GetAcreageMin(const string& id) const {
	auto it = registries.find(id);
	return it != registries.end() ? it->second.acreageMin() : 0.f;
}

float BuildingFactory::GetAcreageMax(const string& id) const {
	auto it = registries.find(id);
	return it != registries.end() ? it->second.acreageMax() : 0.f;
}

float BuildingFactory::GetPower(const string& id, AREA_TYPE area) const {
	auto it = registries.find(id);
	return it != registries.end() ? it->second.power(area) : 0.f;
}

void BuildingFactory::Assign(const string& id, const vector<Lot*>& lots,
	PlacementEmitFunc emit, void* context) const {
	auto it = registries.find(id);
	if (it != registries.end()) {
		it->second.assign(lots, emit, context);
	}
}
