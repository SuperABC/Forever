#include "party_factory.h"

#include "common/error.h"

#include "common/random.h"

#include <algorithm>


using namespace std;

void PartyFactory::RegisterParty(const string& id, CreateFunc creator, DestroyFunc deleter) {
	registries[id] = { creator, deleter };
}

PartyMod* PartyFactory::CreateParty(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	if (!IsEnabled(id)) return nullptr;

	auto argsIt = configuredArgs.find(id);
	PartyMod* instance = MOD_ID_CALL(it->first, it->second.creator(argsIt->second));
	if (instance) {
		liveInstances[instance] = id;
		RandomRegistry::BindInstance(instance, id);
		instance->SetProperty();
	}
	return instance;
}

void PartyFactory::DestroyParty(PartyMod* instance) {
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

bool PartyFactory::CheckRegistered(const string& id) const {
	return IsEnabled(id);
}

vector<string> PartyFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		if (configuredArgs.find(id) == configuredArgs.end()) continue;
		ids.push_back(id);
	}
	sort(ids.begin(), ids.end());
	return ids;
}

void PartyFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}

int PartyFactory::UnregisterExcept(const unordered_set<string>& keepIds) {
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

bool PartyFactory::IsEnabled(const string& id) const {
	return registries.find(id) != registries.end() && configuredArgs.find(id) != configuredArgs.end();
}

vector<pair<string, PartyMod*>> PartyFactory::GetLiveInstances() const {
	vector<pair<string, PartyMod*>> result;
	result.reserve(liveInstances.size());
	for (auto& [instance, id] : liveInstances) result.emplace_back(id, instance);
	return result;
}
