#include "script_factory.h"


using namespace std;

void ScriptFactory::RegisterScript(const string& id, CreateFunc creator, DestroyFunc deleter) {
	// 重复注册同一id拒绝覆盖：已存活实例要用原来的deleter释放，被覆盖后会换成另一个mod的deleter
	if (registries.find(id) != registries.end()) {
		debugf("ScriptFactory::RegisterScript: id '%s' already registered, ignored.\n", id.c_str());
		return;
	}
	registries[id] = { creator, deleter };
}

ScriptMod* ScriptFactory::CreateScript(const string& id) {
	auto it = registries.find(id);
	if (it == registries.end())
		return nullptr;

	if (!IsEnabled(id)) return nullptr;

	auto argsIt = configuredArgs.find(id);
	ScriptMod* instance = it->second.creator(argsIt->second);
	if (instance) {
		liveInstances[instance] = id;
	}
	return instance;
}

void ScriptFactory::DestroyScript(ScriptMod* instance) {
	if (!instance)
		return;

	auto liveIt = liveInstances.find(instance);
	if (liveIt == liveInstances.end())
		return;

	auto regIt = registries.find(liveIt->second);
	if (regIt == registries.end()) {
		// 找不到deleter时Factory不能直接delete(实例由Mod DLL new出)，保留liveInstances记录不丢指针
		debugf("ScriptFactory::DestroyScript: no deleter for id '%s', instance kept alive.\n", liveIt->second.c_str());
		return;
	}
	regIt->second.deleter(instance);

	liveInstances.erase(liveIt);
}

bool ScriptFactory::CheckRegistered(const string& id) const {
	return IsEnabled(id);
}

vector<string> ScriptFactory::GetRegisteredIds() const {
	vector<string> ids;
	for (const auto& [id, entry] : registries) {
		if (configuredArgs.find(id) == configuredArgs.end()) continue;
		ids.push_back(id);
	}
	return ids;
}

void ScriptFactory::SetModArgs(const unordered_map<string, string>& argsById) {
	configuredArgs = argsById;
}

bool ScriptFactory::IsEnabled(const string& id) const {
	return registries.find(id) != registries.end() && configuredArgs.find(id) != configuredArgs.end();
}

vector<pair<string, ScriptMod*>> ScriptFactory::GetLiveInstances() const {
	vector<pair<string, ScriptMod*>> result;
	result.reserve(liveInstances.size());
	for (auto& [instance, id] : liveInstances) result.emplace_back(id, instance);
	return result;
}
