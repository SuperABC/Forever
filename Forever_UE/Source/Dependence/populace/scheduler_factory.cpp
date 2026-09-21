#include "scheduler_factory.h"


using namespace std;

void SchedulerFactory::RegisterScheduler(const string& id, CreateFunc creator, DestroyFunc deleter, PowerFunc power) {
	registries[id] = { creator, deleter, power };
}

SchedulerMod* SchedulerFactory::CreateScheduler(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	auto argsIt = configuredArgs.find(id);
	SchedulerMod* instance = it->second.creator(argsIt != configuredArgs.end() ? argsIt->second : string());
	if (instance) {
		liveInstances[instance] = id;
	}
	return instance;
}

void SchedulerFactory::DestroyScheduler(SchedulerMod* instance) {
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

bool SchedulerFactory::CheckRegistered(const string& id) const {
	return registries.find(id) != registries.end();
}

vector<string> SchedulerFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		ids.push_back(id);
	}
	return ids;
}

void SchedulerFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}

float SchedulerFactory::GetPower(const string& id) const {
	auto it = registries.find(id);
	return it != registries.end() ? it->second.power() : 0.f;
}
