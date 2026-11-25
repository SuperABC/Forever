#include "suites.h"

#include "test_framework.h"
#include "world_build.h"

#include "common/config.h"
#include "common/random.h"
#include "common/registry.h"
#include "common/save.h"
#include "map/building_mod.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>


using namespace std;

// mods套件：只查"mod接进Core之后"的事——Core的Registry认不认得每个已发现的mod、id是否全局唯一、
// 命名计数器复位、Config对mod参数的读写。
// 对mod本身的测试(UnitTest、字段合法性、名字唯一、漏包随机、依赖声明)在ModTest里(Source/Test/Mod，只依赖Dependence，
// 给mod作者用)，run_tests.bat会在CoreTest之前先对config里的全部mod跑一遍ModTest。

void RunModsSuite(const TestOptions& options) {
	TestRun& run = TestRun::Get();
	run.Begin("mods", 0, options.selfCommand + " --suite mods");
	string error;
	if (!LoadTestConfig(options.configPath, 0, options.outDir, error)) {
		run.Check(false, "load config", error);
		return;
	}

	// Core里23个concept的清单只有Registry::ForEachConcept那一份(注册、灌参数、查询、卸载、存档都从它遍历)，
	// 它必须和Dependence的导出符号表一一对上。Registry构造时也会自检并抛异常，这里再留一条看得见的检查。
	{
		const vector<string> keys = Registry::Get().GetConceptKeys();
		const vector<ModConceptDescriptor>& descriptors = GetModConceptDescriptors();
		string problem;
		for (const ModConceptDescriptor& descriptor : descriptors) {
			if (find(keys.begin(), keys.end(), descriptor.conceptKey) == keys.end()) problem += string(" missing ") + descriptor.conceptKey;
		}
		for (const string& key : keys) {
			if (!FindModConceptDescriptor(key)) problem += " unknown " + key;
		}
		run.Check(problem.empty() && keys.size() == descriptors.size(), "Registry lists exactly the concepts in mod_concepts",
			to_string(keys.size()) + " vs " + to_string(descriptors.size()) + ":" + problem);
		run.Metric("concepts", static_cast<double>(keys.size()), "");
	}

	const vector<Config::ModEntry> entries = Config::GetModEnables();
	run.Metric("mods discovered", static_cast<double>(entries.size()), "");

	// 所有mod的id全局唯一(不分concept、不分dll)：随机流、依赖表、CheckModRegistered都只按id找mod。
	// ModTest对它加载的那批dll也查这一条；这里按config的dll_paths再查一次，保证游戏实际会加载的集合没有重名。
	set<string> seenIds;
	map<string, int> perConcept;
	for (const Config::ModEntry& entry : entries) {
		run.Check(seenIds.insert(entry.modId).second, "mod id is globally unique: " + entry.modId,
			"another mod discovered under dll_paths uses the same id (this one: " + entry.conceptKey + " in " + entry.dllPath + ")");

		// 启用它(单选concept会顶掉别的)并重载参数表，Core的Registry应当认得。
		Config::SetModEnabled(entry.conceptKey, entry.modId, true);
		Registry::Get().ReloadModArgs();
		run.Check(Registry::Get().CheckModRegistered(entry.modId), "mod is registered in its Factory: " + entry.modId);
		perConcept[entry.conceptKey]++;
	}
	for (const auto& [conceptKey, count] : perConcept) run.Metric("mods discovered: " + conceptKey, count, "");

	// 新开局复位命名计数器(SaveGame::ResetInstanceCounters)：同一进程连开两局，名字不能接着上一局往下编。
	// 取一个带计数器的building mod：复位后建的第一个实例，名字每次都应该一样。
	{
		BuildingFactory& factory = Registry::Get().GetBuildingFactory();
		vector<string> ids = factory.GetRegisteredIds();
		sort(ids.begin(), ids.end());
		string usedId, firstName, secondName;
		for (const string& id : ids) {
			SaveGame::ResetInstanceCounters();
			BuildingMod* first = factory.CreateBuilding(id);
			if (!first) continue;
			const bool hasCounter = first->GetInstanceCounter() >= 0;
			firstName = first->GetName();
			factory.DestroyBuilding(first);
			if (!hasCounter) continue; // 没有计数器的mod(占位模板)测不出来
			if (BuildingMod* extra = factory.CreateBuilding(id)) factory.DestroyBuilding(extra); // 让计数器再往前走一格
			SaveGame::ResetInstanceCounters();
			if (BuildingMod* second = factory.CreateBuilding(id)) {
				secondName = second->GetName();
				factory.DestroyBuilding(second);
			}
			usedId = id;
			break;
		}
		run.Check(!usedId.empty() && firstName == secondName, "ResetInstanceCounters restarts mod naming: " + usedId,
			"first '" + firstName + "' vs after reset '" + secondName + "'");
	}

	// Config::Get/SetModArgs：已启用读写conceptMods；禁用后参数暂存内存(不落盘)，再启用带回。
	for (const Config::ModEntry& entry : entries) {
		if (Config::IsUniqueConcept(entry.conceptKey)) continue;
		const string& ck = entry.conceptKey;
		const string& id = entry.modId;
		Config::SetModEnabled(ck, id, true);
		Config::SetModArgs(ck, id, "--a=1 --b");
		run.Check(Config::GetModArgs(ck, id) == "--a=1 --b", "mod args: set on enabled mod");
		Config::SetModEnabled(ck, id, false);
		run.Check(Config::GetModArgs(ck, id) == "--a=1 --b", "mod args: kept in memory after disable");
		Config::SetModArgs(ck, id, "--c");
		std::filesystem::create_directories(options.outDir); // 手动运行时--out目录可能还不存在，写不出文件会让下面的检查误判
		const string tmp = options.outDir + "/_modargs_test.json";
		Config::WriteConfig(tmp);
		{
			ifstream in(tmp);
			string text((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
			run.Check(text.find("--c") == string::npos, "mod args: stashed args are not written to disk");
		}
		Config::SetModEnabled(ck, id, true);
		run.Check(Config::GetModArgs(ck, id) == "--c", "mod args: restored on re-enable");
		Config::WriteConfig(tmp);
		{
			ifstream in(tmp);
			string text((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
			run.Check(text.find(id + " --c") != string::npos, "mod args: enabled args are written to disk", text.empty() ? "config file was not written (does the --out directory exist?)" : "");
		}
		remove(tmp.c_str());
		Config::SetModArgs(ck, id, "");
		break;
	}

	// 路径规范化：同一个目录的不同写法(带".."、结尾带斜杠、反斜杠、盘符小写)只算一个根，不会多出一条。
	// 配置里已有的根本身也必须是规范写法(没有".."、没有反斜杠、结尾不带斜杠)。
	{
		auto altSpelling = [](const string& root) {
			const string leaf = std::filesystem::path(root).filename().string();
			string alt = root + "/../" + leaf + "/";
			for (char& c : alt) if (c == '/') c = '\\';
			if (alt.size() >= 2 && alt[1] == ':') alt[0] = static_cast<char>(tolower(static_cast<unsigned char>(alt[0])));
			return alt;
		};
		auto isNormal = [](const string& path) {
			return path.find("..") == string::npos && path.find('\\') == string::npos && !path.empty() && path.back() != '/';
		};
		const vector<string> resourceRoots = Config::GetResourceRootPaths();
		const vector<string> dllRoots = Config::GetDllPaths();
		bool allNormal = true;
		for (const string& root : resourceRoots) allNormal = allNormal && isNormal(root);
		for (const string& root : dllRoots) allNormal = allNormal && isNormal(root);
		run.Check(allNormal, "config paths are stored in normalized form");

		if (!resourceRoots.empty()) {
			Config::AddResourcePath(altSpelling(resourceRoots[0]));
			run.Check(Config::GetResourceRootPaths().size() == resourceRoots.size(),
				"adding another spelling of a resource path does not create a second root", altSpelling(resourceRoots[0]));
			Config::RemoveResourcePath(altSpelling(resourceRoots[0]));
			run.Check(Config::GetResourceRootPaths().size() + 1 == resourceRoots.size(), "removing by another spelling removes the root");
			Config::AddResourcePath(resourceRoots[0]); // 还原
			run.Check(Config::GetResourceRootPaths().size() == resourceRoots.size(), "resource root restored");
		}
		if (!dllRoots.empty()) {
			Config::AddDllPath(altSpelling(dllRoots[0]));
			run.Check(Config::GetDllPaths().size() == dllRoots.size(),
				"adding another spelling of a dll path does not create a second root", altSpelling(dllRoots[0]));
		}
	}
}
