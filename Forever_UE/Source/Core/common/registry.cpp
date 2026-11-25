#include "registry.h"

#include "common/config.h"
#include "common/error.h"
#include "common/random.h"
#include "common/utility.h"

#include <algorithm>
#include <map>


using namespace std;

namespace {
	unordered_map<string, string> ToArgsMap(const vector<pair<string, string>>& entries) {
		unordered_map<string, string> args;
		for (const auto& [id, value] : entries) {
			args[id] = value;
		}
		return args;
	}

	// ToArgsMap + 剥离每个条目里的"--seed N"并登记到RandomRegistry(mod构造函数收到的参数里没有--seed)。
	unordered_map<string, string> ExtractSeeds(const vector<pair<string, string>>& entries) {
		unordered_map<string, string> args;
		for (const auto& [id, value] : entries) {
			bool hasSeed = false;
			uint64_t seed = 0;
			args[id] = RandomRegistry::ExtractSeed(value, hasSeed, seed);
			RandomRegistry::SetModSeed(id, hasSeed, seed);
		}
		return args;
	}
}

Registry& Registry::Get() {
	static Registry instance;
	return instance;
}

void Registry::RegisterMods(const vector<string>& mods) {
	// 导出函数名从Dependence的mod_concepts表里按conceptKey查，不在这里再写一遍。
	ForEachConcept([&](const char* conceptKey, auto& factory, auto, auto) {
		const ModConceptDescriptor* descriptor = FindModConceptDescriptor(conceptKey);
		if (!descriptor) return; // CheckConceptList保证找得到
		modLoader.RegisterConcept(mods, descriptor->registerModSymbol, descriptor->finishModSymbol, &factory);
	});

	for (const string& path : mods) {
		if (!modLoader.IsLoaded(path)) continue;
		loadedDllPaths.insert(path);
		vector<string>& ids = dllModIds[path];
		ids.clear();
		for (const auto& [conceptKey, id] : Config::GetModIdsForDll(path)) ids.push_back(id);
	}
}

void Registry::RefreshMods() {
	vector<string> newMods;
	for (const string& path : Config::GetMods()) {
		if (!loadedDllPaths.count(path)) newMods.push_back(path);
	}
	if (!newMods.empty()) RegisterMods(newMods);
	ReloadModArgs();
}

vector<pair<string, int>> Registry::GetLiveInstanceCounts() {
	map<string, int> counts;
	ForEachConcept([&counts](const char*, auto& factory, auto, auto) {
		for (const auto& [id, instance] : factory.GetLiveInstances()) counts[id]++;
	});
	return vector<pair<string, int>>(counts.begin(), counts.end());
}

vector<string> Registry::GetLoadedDllPaths() const {
	vector<string> paths(loadedDllPaths.begin(), loadedDllPaths.end());
	sort(paths.begin(), paths.end());
	return paths;
}

Registry::UnloadReport Registry::UnloadMods() {
	UnloadReport report;
	report.liveInstances = GetLiveInstanceCounts();
	unordered_set<string> liveIds;
	for (const auto& [id, count] : report.liveInstances) liveIds.insert(id);

	// 还有存活实例的dll留下：实例的虚函数表在dll里，销毁它还要用注册时的deleter。留下的dll连同它全部的注册项一起留
	// (同一个dll里别的mod的注册项也指向这份代码，留着无害)。mod之间不互相调用代码，别的dll可以照常卸载。
	unordered_set<string> keepIds;
	for (const string& path : GetLoadedDllPaths()) {
		const vector<string>& ids = dllModIds[path];
		bool live = false;
		for (const string& id : ids) live = live || liveIds.count(id) > 0;
		if (live) {
			report.keptDlls.push_back(path);
			keepIds.insert(ids.begin(), ids.end());
		}
		else report.unloadedDlls.push_back(path);
	}
	// 存活实例的id不属于任何已知dll(不该发生)：保守起见它的注册项也留着。
	keepIds.insert(liveIds.begin(), liveIds.end());

	// 先注销(Factory里的函数指针指向dll代码)，再卸载。
	ForEachConcept([&keepIds](const char*, auto& factory, auto, auto) { factory.UnregisterExcept(keepIds); });
	for (const string& path : report.unloadedDlls) {
		modLoader.Unload(path);
		loadedDllPaths.erase(path);
		dllModIds.erase(path);
	}

	if (!report.liveInstances.empty()) {
		string leaked;
		for (const auto& [id, count] : report.liveInstances) leaked += (leaked.empty() ? "" : ", ") + id + " x" + to_string(count);
		debugf(DebugLevel::Warning, "Registry::UnloadMods: %d mod instance(s) still alive after the game ended (leak): %s. "
			"%d dll(s) kept loaded:\n",
			static_cast<int>(liveIds.size()), leaked.c_str(), static_cast<int>(report.keptDlls.size()));
		for (const string& path : report.keptDlls) debugf(DebugLevel::Warning, "  kept %s\n", path.c_str());
	}
	debugf(DebugLevel::Log, "Registry::UnloadMods: unloaded %d mod dll(s), kept %d.\n",
		static_cast<int>(report.unloadedDlls.size()), static_cast<int>(report.keptDlls.size()));
	return report;
}

Registry::Registry() {
	CheckConceptList();
	vector<string> mods = Config::GetMods();
	RegisterMods(mods);

	// 构造时先按当前已经读进内存的config内容(Config::ReadConfig)灌一遍参数,保证Get()刚拿到
	// 实例时每个Factory就有参数可用,不强制调用方第一次也要记得手动调ReloadModArgs()。
	ReloadModArgs();
}

void Registry::ReloadModArgs() {
	// 种子登记：新开局/读档都会走到这里，先清空旧流再按config重新登记；读档随后用存档状态覆盖流(见save)。
	RandomRegistry::Reset();
	unsigned long long globalSeed = 0;
	RandomRegistry::SetGlobalSeed(Config::GetGlobalSeed(globalSeed), globalSeed);
	ForEachConcept([](const char* conceptKey, auto& factory, auto, auto) {
		factory.SetModArgs(ExtractSeeds(Config::GetConceptMods(ModConceptJsonKey(conceptKey))));
	});
}

bool Registry::CheckModRegistered(const string& id) {
	bool registered = false;
	ForEachConcept([&](const char*, auto& factory, auto, auto) { registered = registered || factory.CheckRegistered(id); });
	return registered;
}

vector<string> Registry::GetConceptKeys() {
	vector<string> keys;
	ForEachConcept([&keys](const char* conceptKey, auto&, auto, auto) { keys.push_back(conceptKey); });
	return keys;
}

void Registry::CheckConceptList() {
	const vector<string> keys = GetConceptKeys();
	string problem;
	for (size_t i = 0; i < keys.size(); i++) {
		if (!FindModConceptDescriptor(keys[i])) problem += " unknown concept '" + keys[i] + "';";
		for (size_t j = 0; j < i; j++) if (keys[j] == keys[i]) problem += " '" + keys[i] + "' listed twice;";
	}
	for (const ModConceptDescriptor& descriptor : GetModConceptDescriptors()) {
		if (find(keys.begin(), keys.end(), descriptor.conceptKey) == keys.end()) problem += string(" missing concept '") + descriptor.conceptKey + "';";
	}
	if (!problem.empty()) {
		THROW_EXCEPTION(RuntimeException, "Registry::ForEachConcept does not match Dependence/common/mod_concepts:" + problem + "\n");
	}
}
