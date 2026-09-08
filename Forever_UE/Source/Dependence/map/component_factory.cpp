#include "component_factory.h"

using namespace std;

void ComponentFactory::RegisterComponent(const string& id, CreateFunc creator, DestroyFunc deleter) {
	registries[id] = { creator, deleter };
}

void ComponentFactory::CleanTemp() {
}

ComponentMod* ComponentFactory::CreateComponent(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	ComponentMod* instance = it->second.creator();
	if (instance) {
		liveInstances[instance] = id;

		auto argsIt = configuredArgs.find(id);
		instance->ApplyArgs(argsIt != configuredArgs.end() ? argsIt->second : string());
	}
	return instance;
}

void ComponentFactory::DestroyComponent(ComponentMod* instance) {
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

bool ComponentFactory::CheckRegistered(const string& id) const {
	return registries.find(id) != registries.end();
}

vector<string> ComponentFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		ids.push_back(id);
	}
	return ids;
}

void ComponentFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}
