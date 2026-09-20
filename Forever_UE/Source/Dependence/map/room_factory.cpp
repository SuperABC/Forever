#include "room_factory.h"


using namespace std;

void RoomFactory::RegisterRoom(const string& id, CreateFunc creator, DestroyFunc deleter) {
	registries[id] = { creator, deleter };
}

RoomMod* RoomFactory::CreateRoom(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	auto argsIt = configuredArgs.find(id);
	RoomMod* instance = it->second.creator(argsIt != configuredArgs.end() ? argsIt->second : string());
	if (instance) {
		liveInstances[instance] = id;
	}
	return instance;
}

void RoomFactory::DestroyRoom(RoomMod* instance) {
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

bool RoomFactory::CheckRegistered(const string& id) const {
	return registries.find(id) != registries.end();
}

vector<string> RoomFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		ids.push_back(id);
	}
	return ids;
}

void RoomFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}
