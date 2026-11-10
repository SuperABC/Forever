#include <set>
#include "config.h"

#include "json.h"

#include "loader.h"

#include <fstream>
#include <iostream>
#include <unordered_set>
#include <cctype>
#include <algorithm>
#include <windows.h>


using namespace std;

string Config::configDir = "";
unordered_map<string, vector<string>> Config::dllPaths = {};
unordered_map<string, vector<string>> Config::layoutPaths = {};
unordered_map<string, vector<string>> Config::resourcePaths = {};
unordered_map<string, vector<string>> Config::pluginPaths = {};
unordered_map<string, vector<string>> Config::pakPaths = {};
string Config::mainStoryScriptModName = "";
unordered_map<string, vector<pair<string, string>>> Config::conceptMods = {};
unordered_map<string, vector<pair<string, string>>> Config::dllConceptIds = {};
unordered_map<string, vector<string>> Config::modDependences = {};
vector<string> Config::storyScriptPaths = {};

void Config::NormalizeUniqueConcepts() {
	static const vector<string> kUniqueConcepts = { "Roadnets", "Names" };

	for (const string& conceptKey : kUniqueConcepts) {
		string jsonKey = ConceptKeyToJsonKey(conceptKey);
		auto it = conceptMods.find(jsonKey);
		if (it == conceptMods.end())
			continue;
		// 数组即启用状态的唯一真相。留下第一个"已发现"的(UI看得见的)，其它全删；一个都没发现就留第一个，
		// 不然藏起来的陈旧id排在前面会把真正选中的那个挤掉。
		if (it->second.size() > 1) {
			size_t keep = 0;
			for (size_t i = 0; i < it->second.size(); i++) {
				if (IsModDiscovered(conceptKey, it->second[i].first)) { keep = i; break; }
			}
			pair<string, string> kept = it->second[keep];
			it->second.assign(1, kept);
		}
	}
}

bool Config::IsModDiscovered(const string& conceptKey, const string& modId) {
	for (const auto& [dllPath, ids] : dllConceptIds) {
		(void)dllPath;
		for (const auto& [ck, id] : ids) {
			if (ck == conceptKey && id == modId) return true;
		}
	}
	return false;
}

void Config::PruneUndiscoveredMods() {
	// 已发现的(jsonKey, id)集合：当前dll_paths下的dll实际提供的mod，也就是UI里看得见的。
	set<pair<string, string>> discovered;
	for (const auto& [dllPath, ids] : dllConceptIds) {
		(void)dllPath;
		for (const auto& [ck, id] : ids) discovered.insert({ ConceptKeyToJsonKey(ck), id });
	}
	for (auto& [jsonKey, entries] : conceptMods) {
		entries.erase(remove_if(entries.begin(), entries.end(),
			[&](const pair<string, string>& e) { return discovered.find({ jsonKey, e.first }) == discovered.end(); }),
			entries.end());
	}
	// 主线剧情mod也必须是已启用且看得见的script mod，否则视为没选。
	if (!mainStoryScriptModName.empty() && !IsModEnabled("Scripts", mainStoryScriptModName)) {
		mainStoryScriptModName.clear();
	}
}

string Config::ConceptKeyToJsonKey(const string& conceptKey) {
	string singular = conceptKey.empty() ? conceptKey : conceptKey.substr(0, conceptKey.size() - 1);
	for (char& c : singular) {
		c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
	}
	return singular + "_mods";
}

namespace {
	// absolute()/canonical()在Windows下会产出反斜杠，原样写进JSON会因非法转义让整个文件解析失败，
	// 所以所有写回config.json的路径统一在这里收口成正斜杠。
	string ToForwardSlashes(const string& path) {
		string result = path;
		for (char& c : result) {
			if (c == '\\') {
				c = '/';
			}
		}
		return result;
	}

	using GetModFunc = void* (*)();
}

bool Config::CheckFileFormat(const filesystem::path& filePath, const string& format) {
	if (!filesystem::is_regular_file(filePath))
		return false;
	string ext = filePath.extension().string();
	for (char& c : ext) {
		c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
	}
	return ext == format;
}

void Config::ReadConfig(const string& path) {
	dllPaths.clear();
	layoutPaths.clear();
	resourcePaths.clear();
	pluginPaths.clear();
	pakPaths.clear();
	mainStoryScriptModName.clear();
	conceptMods.clear();
	dllConceptIds.clear();
	modDependences.clear();
	storyScriptPaths.clear();

	ifstream fin(path);
	if (!fin.is_open()) {
		// 留空不抛异常，调用方据此回退默认目录；但要打警告，区分"文件打不开"(路径错/被独占)和"JSON语法错误"。
		cerr << "[Config] Warning: cannot open config file: " << path << "\n";
		return;
	}

	// 必须canonical()而不是parent_path()：Resource/在打包产物里是mklink /J目录连接，词法化的".."
	// 不会跟随reparse point，会在归档目录树内部兜圈子，走不到真实开发树。PIE下是无副作用的空操作。
	error_code canonicalError;
	filesystem::path canonicalPath = filesystem::canonical(filesystem::path(path), canonicalError);
	configDir = (canonicalError ? filesystem::path(path) : canonicalPath).parent_path().string();

	JsonReader reader;
	JsonValue root;
	bool parsed = reader.Parse(fin, root);
	fin.close();

	if (!parsed) {
		cerr << "[Config] Warning: json syntax error in " << path << ": "
			<< reader.GetErrorMessages() << "\n";
		return;
	}

	for (const auto& dllPath : root["dll_paths"]) {
		// 相对路径相对configDir解析
		filesystem::path resolved(dllPath.AsString());
		if (resolved.is_relative()) {
			resolved = filesystem::path(configDir) / resolved;
		}
		AddDllPath(ToForwardSlashes(resolved.string()));
	}

	for (const auto& resourcePath : root["resource_paths"]) {
		// 和dll_paths同一个相对路径解析规则，相对configDir。
		filesystem::path resolved(resourcePath.AsString());
		if (resolved.is_relative()) {
			resolved = filesystem::path(configDir) / resolved;
		}
		AddResourcePath(ToForwardSlashes(resolved.string()));
	}

	mainStoryScriptModName = root["main_story"].AsString();

	for (const auto& scriptPath : root["story_scripts"]) {
		// 和dll_paths/resource_paths同一个相对路径解析规则，相对configDir。
		filesystem::path resolved(scriptPath.AsString());
		if (resolved.is_relative()) {
			resolved = filesystem::path(configDir) / resolved;
		}
		AddStoryScript(ToForwardSlashes(resolved.string()));
	}

	// 所有以"_mods"结尾的顶层key都当作一个concept的mod列表；元素"id"或"id 参数..."按第一个空格切开
	const string suffix = "_mods";
	for (const string& key : root.GetMembers()) {
		if (key.size() <= suffix.size())
			continue;
		if (key.compare(key.size() - suffix.size(), suffix.size(), suffix) != 0)
			continue;

		vector<pair<string, string>> entries;
		for (const auto& entry : root[key]) {
			string raw = entry.AsString();
			size_t spacePos = raw.find(' ');
			if (spacePos == string::npos) {
				entries.push_back({ raw, string() });
			} else {
				entries.push_back({ raw.substr(0, spacePos), raw.substr(spacePos + 1) });
			}
		}
		conceptMods[key] = entries;
	}

	// 文件里Roadnets/Names可能同时列着好几个条目(手写config.json不限制数量)，读完立刻
	// 收口成最多一个，不等到UI查询或者下次保存才发现。
	NormalizeUniqueConcepts();
}

string Config::GetConfigDir() {
	return configDir;
}

vector<string> Config::GetDllPaths() {
	vector<string> paths;
	for (const auto& [dllDir, _] : dllPaths) {
		paths.push_back(dllDir);
	}
	return paths;
}

vector<string> Config::GetMods() {
	vector<string> mods;
	unordered_set<string> seen;
	for (const auto& [_, dlls] : dllPaths) {
		for (const auto& mod : dlls) {
			if (seen.insert(mod).second) {
				mods.push_back(mod);
			}
		}
	}
	return mods;
}

void Config::AddDllPath(const string& path) {
	filesystem::path dir(path);
	if (!filesystem::exists(dir) || !filesystem::is_directory(dir)) {
		cerr << "[Config] Warning: mod path does not exist: " << path << "\n";
		return;
	}

	RemoveDllPath(path);

	// skip_permission_denied：递归到无权限子目录时跳过，而不是抛filesystem_error中断整次扫描
	error_code dirIterError;
	for (const auto& entry : filesystem::recursive_directory_iterator(
			dir, filesystem::directory_options::skip_permission_denied, dirIterError)) {
		if (!CheckFileFormat(entry.path(), ".dll"))
			continue;

		// 跳过带"Debug"目录段的dll：/MDd调试CRT的std::string布局不兼容，跨DLL读返回的vector<string>会崩；
		// 回退扫描会无差别递归整个目录树，必须在这里挡掉(见config.md)
		bool underDebugDir = false;
		for (const auto& part : entry.path()) {
			string partLower = part.string();
			for (char& c : partLower) {
				c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
			}
			if (partLower == "debug") {
				underDebugDir = true;
				break;
			}
		}
		if (underDebugDir)
			continue;

		string full = ToForwardSlashes(filesystem::absolute(entry.path()).string());
		bool valid = false;
		vector<pair<string, string>> ids;
		vector<pair<string, vector<string>>> perModDependences;

		HMODULE modHandle = LoadLibraryA(full.data());
		if (modHandle) {
			// 真正调用一次读出(concept,modId)，FreeLibrary之前完成；返回的static vector调用方只读不释放
			for (const auto& descriptor : GetModConceptDescriptors()) {
				auto getModFunc = reinterpret_cast<GetModFunc>(GetProcAddress(modHandle, descriptor.getModSymbol));
				if (!getModFunc)
					continue;
				valid = true;
				if (void* raw = getModFunc()) {
					auto* modIds = static_cast<vector<string>*>(raw);
					for (const string& id : *modIds) {
						ids.push_back({ descriptor.conceptKey, id });
					}
				}
			}

			// per-mod依赖声明探测，见loader.h kModDependenciesSymbol
			auto getDepsFunc = reinterpret_cast<GetModFunc>(GetProcAddress(modHandle, kModDependenciesSymbol));
			if (getDepsFunc) {
				if (void* raw = getDepsFunc()) {
					perModDependences = *static_cast<vector<pair<string, vector<string>>>*>(raw);
				}
			}

			FreeLibrary(modHandle);
		}

		if (valid) {
			dllPaths[path].push_back(full);
			dllConceptIds[full] = ids;
			// 按modId拆开存进扁平map——modId本身全局唯一，不需要再按dllPath分组。
			for (const auto& [modId, depIds] : perModDependences) {
				modDependences[modId] = depIds;
			}
		}
	}
}

void Config::RemoveDllPath(const string& path) {
	auto it = dllPaths.find(path);
	if (it != dllPaths.end()) {
		for (const string& dll : it->second) {
			auto idsIt = dllConceptIds.find(dll);
			if (idsIt != dllConceptIds.end()) {
				// 这个dll提供的id被移除了，连带把它们从对应的"<concept>_mods"数组里也
				// 删掉——不然会留下"启用着一个已经不存在的dll提供的id"这种孤儿状态。
				for (const auto& [conceptKey, modId] : idsIt->second) {
					string jsonKey = ConceptKeyToJsonKey(conceptKey);
					auto modsIt = conceptMods.find(jsonKey);
					if (modsIt != conceptMods.end()) {
						auto& entries = modsIt->second;
						entries.erase(
							remove_if(entries.begin(), entries.end(),
								[&modId](const pair<string, string>& e) { return e.first == modId; }),
							entries.end());
					}
					// 依赖声明按modId存，随之清掉
					modDependences.erase(modId);
				}
			}
			dllConceptIds.erase(dll);
		}
	}
	dllPaths.erase(path);
}

vector<string> Config::GetLayouts() {
	vector<string> paths;
	for (const auto& [_, layouts] : layoutPaths) {
		for (const auto& layout : layouts) {
			paths.push_back(layout);
		}
	}
	return paths;
}

void Config::AddResourcePath(const string& path) {
	filesystem::path dir(path);
	if (!filesystem::exists(dir) || !filesystem::is_directory(dir)) {
		cerr << "[Config] Warning: resource path does not exist: " << path << "\n";
		return;
	}

	resourcePaths.erase(path);
	pluginPaths.erase(path);
	pakPaths.erase(path);

	vector<string> scripts;
	vector<string> plugins;
	vector<string> paks;
	vector<string> layouts;
	for (const auto& entry : filesystem::recursive_directory_iterator(dir)) {
		string full = ToForwardSlashes(filesystem::absolute(entry.path()).string());
		if (CheckFileFormat(entry.path(), ".script")) {
			scripts.push_back(full);
		} else if (CheckFileFormat(entry.path(), ".uplugin")) {
			// 独立迷你UE工程里的Plugin：WITH_EDITOR下直接挂载Content目录，不需要cook/pak
			plugins.push_back(full);
		} else if (CheckFileFormat(entry.path(), ".pak")) {
			// 同一个Plugin cook+UnrealPak出来的产物，非编辑器(打包)下MountPaksEx挂载。
			paks.push_back(full);
		} else if (CheckFileFormat(entry.path(), ".layout")) {
			// 游戏启动配置界面的Resource screen要求一个文件夹同时覆盖.layout和.script，
			// 这里顺带收集进现有的layoutPaths(和resourcePaths同一次扫描，不新增容器)，
			// 不强制玩家为同一个文件夹分别在两个地方各添加一次。
			layouts.push_back(full);
		}
	}
	resourcePaths[path] = scripts;
	pluginPaths[path] = plugins;
	pakPaths[path] = paks;
	layoutPaths[path] = layouts;
}

bool Config::HasResourcePaths() {
	return !resourcePaths.empty();
}

vector<string> Config::GetPlugins() {
	vector<string> paths;
	for (const auto& [_, plugins] : pluginPaths) {
		for (const auto& plugin : plugins) {
			paths.push_back(plugin);
		}
	}
	return paths;
}

vector<string> Config::GetPakFiles() {
	vector<string> paths;
	for (const auto& [_, paks] : pakPaths) {
		for (const auto& pak : paks) {
			paths.push_back(pak);
		}
	}
	return paths;
}

string Config::GetScriptPath(const string& name) {
	for (const auto& [_, scripts] : resourcePaths) {
		for (const auto& script : scripts) {
			if (filesystem::path(script).stem().string() == name) {
				return script;
			}
		}
	}
	return string();
}

string Config::GetResourceFilePath(const string& fileName) {
	for (const auto& [rootPath, _] : resourcePaths) {
		(void)_;
		error_code dirIterError;
		for (const auto& entry : filesystem::recursive_directory_iterator(
				rootPath, filesystem::directory_options::skip_permission_denied, dirIterError)) {
			if (entry.path().filename().string() == fileName && entry.is_regular_file()) {
				return ToForwardSlashes(filesystem::absolute(entry.path()).string());
			}
		}
	}
	return string();
}

string Config::GetMainStoryScriptModName() {
	return mainStoryScriptModName;
}

string Config::GetMainStoryScriptPath() {
	return GetScriptPath("test");
}

vector<string> Config::GetMainStoryScriptPaths() {
	if (!storyScriptPaths.empty()) return storyScriptPaths;
	string fallback = GetMainStoryScriptPath();
	return fallback.empty() ? vector<string>() : vector<string>{ fallback };
}

vector<string> Config::GetConceptJsonKeys() {
	vector<string> keys;
	keys.reserve(conceptMods.size());
	for (auto& [key, mods] : conceptMods) keys.push_back(key);
	return keys;
}

void Config::SetConceptMods(const string& jsonKey, const vector<pair<string, string>>& mods) {
	conceptMods[jsonKey] = mods;
}

void Config::SetStoryScripts(const vector<string>& paths) {
	storyScriptPaths = paths;
}

void Config::SetMainStoryScriptModName(const string& name) {
	mainStoryScriptModName = name;
}

void Config::RescanFiles() {
	// 先拷出根目录列表——AddResourcePath会erase再重新插入同一个key。.layout也由AddResourcePath
	// 在同一次扫描里收进layoutPaths，所以只需要按resource根重扫。
	vector<string> resourceRoots;
	for (const auto& [root, _] : resourcePaths) resourceRoots.push_back(root);
	for (const string& root : resourceRoots) AddResourcePath(root);
}

vector<string> Config::FindMissingScripts(const vector<string>& scripts) {
	vector<string> missing;
	for (const string& name : scripts) {
		if (GetScriptPath(name).empty()) missing.push_back(name);
	}
	return missing;
}

vector<string> Config::FindMissingLayouts(const vector<string>& layouts) {
	unordered_set<string> available;
	for (const string& path : GetLayouts()) available.insert(filesystem::path(path).stem().string());
	vector<string> missing;
	for (const string& name : layouts) {
		if (!available.count(name)) missing.push_back(name);
	}
	return missing;
}

vector<pair<string, string>> Config::GetConceptMods(const string& jsonKey) {
	auto it = conceptMods.find(jsonKey);
	if (it == conceptMods.end()) {
		return {};
	}
	return it->second;
}

// ---- 游戏启动配置界面(Story/Mod/Resource三screen)使用的接口 ----

vector<Config::ModEntry> Config::GetModEnables() {
	NormalizeUniqueConcepts();

	vector<ModEntry> rows;
	for (const auto& [rootPath, dlls] : dllPaths) {
		(void)rootPath;
		for (const auto& dll : dlls) {
			auto it = dllConceptIds.find(dll);
			if (it == dllConceptIds.end())
				continue;
			for (const auto& [conceptKey, modId] : it->second) {
				rows.push_back({ dll, conceptKey, modId, IsModEnabled(conceptKey, modId) });
			}
		}
	}
	return rows;
}

bool Config::IsUniqueConcept(const string& conceptKey) {
	return conceptKey == "Roadnets" || conceptKey == "Names";
}

void Config::SetModEnabled(const string& conceptKey, const string& modId, bool enabled) {
	// Roadnets/Names必须且只能启用一个：选中的不能取消，只能通过选另一个来切换。
	if (!enabled && IsUniqueConcept(conceptKey) && IsModEnabled(conceptKey, modId)) return;
	// 取消启用的script mod正是主线剧情时，主线变成"没选"。
	if (!enabled && conceptKey == "Scripts" && mainStoryScriptModName == modId) mainStoryScriptModName.clear();

	string jsonKey = ConceptKeyToJsonKey(conceptKey);
	auto& entries = conceptMods[jsonKey];
	auto it = find_if(entries.begin(), entries.end(),
		[&modId](const pair<string, string>& e) { return e.first == modId; });

	if (enabled) {
		if (it == entries.end()) {
			// 新启用的id还没有参数字符串,给空串,和AddDllPath发现新mod时的默认行为一致。
			entries.push_back({ modId, string() });
		}
		// 已经在数组里就不用动,保留原有的参数字符串。
	} else if (it != entries.end()) {
		entries.erase(it);
	}

	// Roadnets/Names单选：选中后把同concept下其它已发现id全部从数组移除
	if (enabled && IsUniqueConcept(conceptKey)) {
		for (const auto& [dllPath, ids] : dllConceptIds) {
			(void)dllPath;
			for (const auto& [ck, id] : ids) {
				if (ck == conceptKey && id != modId) {
					auto& list = conceptMods[jsonKey];
					list.erase(
						remove_if(list.begin(), list.end(),
							[&id](const pair<string, string>& e) { return e.first == id; }),
						list.end());
				}
			}
		}
	}
}

bool Config::IsModEnabled(const string& conceptKey, const string& modId) {
	string jsonKey = ConceptKeyToJsonKey(conceptKey);
	auto conceptIt = conceptMods.find(jsonKey);
	if (conceptIt == conceptMods.end())
		return false;
	for (const auto& [id, args] : conceptIt->second) {
		(void)args;
		if (id == modId) {
			return true;
		}
	}
	return false;
}

bool Config::IsModIdEnabled(const string& id) {
	for (const auto& [dllPath, ids] : dllConceptIds) {
		(void)dllPath;
		for (const auto& [conceptKey, modId] : ids) {
			if (modId == id && IsModEnabled(conceptKey, modId)) {
				return true;
			}
		}
	}
	return false;
}

bool Config::IsDllActive(const string& dllPath) {
	auto it = dllConceptIds.find(dllPath);
	if (it == dllConceptIds.end())
		return false;
	for (const auto& [conceptKey, modId] : it->second) {
		if (IsModEnabled(conceptKey, modId)) {
			return true;
		}
	}
	return false;
}

vector<string> Config::GetActiveDllPaths() {
	vector<string> result;
	for (const auto& [rootPath, dlls] : dllPaths) {
		(void)rootPath;
		for (const auto& dll : dlls) {
			if (IsDllActive(dll)) {
				result.push_back(dll);
			}
		}
	}
	return result;
}

vector<pair<string, string>> Config::GetModIdsForDll(const string& dllPath) {
	auto it = dllConceptIds.find(dllPath);
	return it != dllConceptIds.end() ? it->second : vector<pair<string, string>>();
}

vector<string> Config::GetModDependences(const string& modId) {
	auto it = modDependences.find(modId);
	return it != modDependences.end() ? it->second : vector<string>();
}

vector<string> Config::GetResourceRootPaths() {
	vector<string> paths;
	for (const auto& [rootPath, _] : resourcePaths) {
		(void)_;
		paths.push_back(rootPath);
	}
	return paths;
}

void Config::RemoveResourcePath(const string& path) {
	resourcePaths.erase(path);
	pluginPaths.erase(path);
	pakPaths.erase(path);
	layoutPaths.erase(path);
}

void Config::AddStoryScript(const string& path) {
	if (!CheckFileFormat(filesystem::path(path), ".script")) {
		cerr << "[Config] Warning: not a .script file: " << path << "\n";
		return;
	}
	string full = ToForwardSlashes(filesystem::absolute(path).string());
	if (find(storyScriptPaths.begin(), storyScriptPaths.end(), full) == storyScriptPaths.end()) {
		storyScriptPaths.push_back(full);
	}
}

void Config::RemoveStoryScript(const string& path) {
	string full = ToForwardSlashes(filesystem::absolute(path).string());
	storyScriptPaths.erase(remove(storyScriptPaths.begin(), storyScriptPaths.end(), full), storyScriptPaths.end());
}

vector<string> Config::GetStoryScripts() {
	return storyScriptPaths;
}

void Config::WriteConfig(const string& path) {
	// 保存前再收口一次Roadnets/Names的唯一性——哪怕调用方从来没碰过GetModEnables()
	// (比如直接走ValidateAndStartGame，中间没有UI刷新过列表)，写进文件的也必须是已经
	// 规范化过的状态。
	NormalizeUniqueConcepts();
	// 当前加载的dll里没有的mod是UI里看不见的，不能悄悄存进文件。
	PruneUndiscoveredMods();

	// 写绝对路径(config.json不跨设备使用)，ReadConfig仍兼容解析相对路径
	JsonValue root(DATA_OBJECT);

	JsonValue dllArr(DATA_ARRAY);
	for (const auto& [rootPath, _] : dllPaths) {
		(void)_;
		dllArr.append(JsonValue(rootPath));
	}
	root["dll_paths"] = dllArr;

	JsonValue resourceArr(DATA_ARRAY);
	for (const auto& [rootPath, _] : resourcePaths) {
		(void)_;
		resourceArr.append(JsonValue(rootPath));
	}
	root["resource_paths"] = resourceArr;

	root["main_story"] = JsonValue(mainStoryScriptModName);

	JsonValue storyArr(DATA_ARRAY);
	for (const string& script : storyScriptPaths) {
		storyArr.append(JsonValue(script));
	}
	root["story_scripts"] = storyArr;

	// conceptMods本身就是"谁启用谁没启用"的唯一真相来源(SetModEnabled直接维护它)，原样
	// 写回即可，不需要再按别的状态重新筛选/重建。
	for (const auto& [jsonKey, entries] : conceptMods) {
		JsonValue arr(DATA_ARRAY);
		for (const auto& [id, args] : entries) {
			arr.append(JsonValue(args.empty() ? id : (id + " " + args)));
		}
		root[jsonKey] = arr;
	}

	StyledWriter writer;
	string text = writer.Write(root);

	ofstream fout(path);
	if (!fout.is_open()) {
		cerr << "[Config] Warning: cannot write config to " << path << "\n";
		return;
	}
	fout << text;
}
