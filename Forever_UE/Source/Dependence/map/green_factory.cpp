#include "green_factory.h"

#include "common/random.h"


using namespace std;

void GreenFactory::RegisterGreen(const string& id, CreateFunc creator, DestroyFunc deleter,
	PowerFunc powerFree, PowerFunc powerLinked) {
	registries[id] = { creator, deleter, powerFree, powerLinked };
}

GreenMod* GreenFactory::CreateGreen(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	if (!IsEnabled(id)) return nullptr;

	auto argsIt = configuredArgs.find(id);
	GreenMod* instance = it->second.creator(argsIt->second);
	if (instance) {
		liveInstances[instance] = id;
		RandomRegistry::BindInstance(instance, id);
	}
	return instance;
}

void GreenFactory::DestroyGreen(GreenMod* instance) {
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

bool GreenFactory::CheckRegistered(const string& id) const {
	return IsEnabled(id);
}

vector<string> GreenFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		if (configuredArgs.find(id) == configuredArgs.end()) continue;
		ids.push_back(id);
	}
	return ids;
}

void GreenFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}

bool GreenFactory::IsEnabled(const string& id) const {
	return registries.find(id) != registries.end() && configuredArgs.find(id) != configuredArgs.end();
}

float GreenFactory::GetPowerFree(const string& id) const {
	auto it = registries.find(id);
	if (it == registries.end() || !IsEnabled(id) || !it->second.powerFree) return 0.f;
	RandomScope scope(RandomRegistry::GetModStream(id));
	return it->second.powerFree();
}

float GreenFactory::GetPowerLinked(const string& id) const {
	auto it = registries.find(id);
	if (it == registries.end() || !IsEnabled(id) || !it->second.powerLinked) return 0.f;
	RandomScope scope(RandomRegistry::GetModStream(id));
	return it->second.powerLinked();
}

vector<pair<string, GreenMod*>> GreenFactory::GetLiveInstances() const {
	vector<pair<string, GreenMod*>> result;
	result.reserve(liveInstances.size());
	for (auto& [instance, id] : liveInstances) result.emplace_back(id, instance);
	return result;
}
