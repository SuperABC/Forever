#include "name_factory.h"


using namespace std;

void NameFactory::RegisterName(const string& id, CreateFunc creator, DestroyFunc deleter) {
	registries[id] = { creator, deleter };
}

NameMod* NameFactory::CreateName(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	auto argsIt = configuredArgs.find(id);
	NameMod* instance = it->second.creator(argsIt != configuredArgs.end() ? argsIt->second : string());
	if (instance) {
		liveInstances[instance] = id;
	}
	return instance;
}

void NameFactory::DestroyName(NameMod* instance) {
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

bool NameFactory::CheckRegistered(const string& id) const {
	return registries.find(id) != registries.end();
}

vector<string> NameFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		ids.push_back(id);
	}
	return ids;
}

void NameFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}
