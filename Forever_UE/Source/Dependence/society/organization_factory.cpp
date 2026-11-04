#include "organization_factory.h"


using namespace std;

void OrganizationFactory::RegisterOrganization(const string& id, CreateFunc creator, DestroyFunc deleter, PowerFunc power,
	RequirementsFunc requirements) {
	registries[id] = { creator, deleter, power, requirements };
}

OrganizationMod* OrganizationFactory::CreateOrganization(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	if (!IsEnabled(id)) return nullptr;

	auto argsIt = configuredArgs.find(id);
	OrganizationMod* instance = it->second.creator(argsIt->second);
	if (instance) {
		liveInstances[instance] = id;
	}
	return instance;
}

void OrganizationFactory::DestroyOrganization(OrganizationMod* instance) {
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

bool OrganizationFactory::CheckRegistered(const string& id) const {
	return IsEnabled(id);
}

vector<string> OrganizationFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		if (configuredArgs.find(id) == configuredArgs.end()) continue;
		ids.push_back(id);
	}
	return ids;
}

void OrganizationFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}

bool OrganizationFactory::IsEnabled(const string& id) const {
	return registries.find(id) != registries.end() && configuredArgs.find(id) != configuredArgs.end();
}

float OrganizationFactory::GetPower(const string& id) const {
	auto it = registries.find(id);
	return IsEnabled(id) ? it->second.power() : 0.f;
}

const unordered_map<string, pair<int, int>>& OrganizationFactory::GetRequirements(const string& id) const {
	static const unordered_map<string, pair<int, int>> empty;
	auto it = registries.find(id);
	if (!IsEnabled(id) || !it->second.requirements) return empty;
	return it->second.requirements();
}

vector<pair<string, OrganizationMod*>> OrganizationFactory::GetLiveInstances() const {
	vector<pair<string, OrganizationMod*>> result;
	result.reserve(liveInstances.size());
	for (auto& [instance, id] : liveInstances) result.emplace_back(id, instance);
	return result;
}
