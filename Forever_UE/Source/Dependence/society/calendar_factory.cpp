#include "calendar_factory.h"

using namespace std;

void CalendarFactory::RegisterCalendar(const string& id, CreateFunc creator, DestroyFunc deleter) {
	registries[id] = { creator, deleter };
}

void CalendarFactory::CleanTemp() {
}

CalendarMod* CalendarFactory::CreateCalendar(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	CalendarMod* instance = it->second.creator();
	if (instance) {
		liveInstances[instance] = id;

		auto argsIt = configuredArgs.find(id);
		instance->ApplyArgs(argsIt != configuredArgs.end() ? argsIt->second : string());
	}
	return instance;
}

void CalendarFactory::DestroyCalendar(CalendarMod* instance) {
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

bool CalendarFactory::CheckRegistered(const string& id) const {
	return registries.find(id) != registries.end();
}

vector<string> CalendarFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		ids.push_back(id);
	}
	return ids;
}

void CalendarFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}
