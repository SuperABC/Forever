#include "config.h"

#include "common/error.h"

#include "json.h"

#include "loader.h"

#include <set>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <cctype>
#include <cerrno>
#include <algorithm>
#include <windows.h>


using namespace std;

string Config::configDir = "";
unordered_map<string, vector<string>> Config::dllPaths = {};
unordered_map<string, vector<string>> Config::layoutPaths = {};
unordered_map<string, vector<string>> Config::resourcePaths = {};
vector<string> Config::resourceRootOrder = {};
unordered_map<string, unordered_map<string, string>> Config::resourceFilesByRoot = {};
unordered_map<string, vector<string>> Config::pluginPaths = {};
unordered_map<string, vector<string>> Config::pakPaths = {};
string Config::mainStoryScriptModName = "";
string Config::lastReadError;
bool Config::hasGlobalSeed = false;
unsigned long long Config::globalSeed = 0;
bool Config::hasMapSize = false;
int Config::mapWidth = 0;
int Config::mapHeight = 0;
unordered_map<string, vector<pair<string, string>>> Config::conceptMods = {};
unordered_map<string, string> Config::stashedModArgs = {};
unordered_map<string, vector<pair<string, string>>> Config::dllConceptIds = {};
unordered_map<string, vector<string>> Config::modDependences = {};
vector<string> Config::storyScriptPaths = {};

namespace {
	// 单选concept(config里只能启用一个mod)的名单，只在这里列一次。
	const vector<string>& UniqueConcepts() {
		static const vector<string> kUniqueConcepts = { "Roadnets", "Names" };
		return kUniqueConcepts;
	}
}

void Config::NormalizeUniqueConcepts() {
	for (const string& conceptKey : UniqueConcepts()) {
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
	return ModConceptJsonKey(conceptKey);
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

	// 路径的统一写法：绝对路径、去掉"."/".."和多余的分隔符、正斜杠、结尾不带斜杠、盘符大写。
	// dllPaths/resourcePaths等map都拿路径字符串当键，同一个目录只有一种写法才不会被当成两个根
	// (config.json里出现过".../Test/Resource/../../../../../Projects/Forever/Forever_UE/Resource/Layouts"这种写法)。
	// 用词法规范化(lexically_normal)而不是canonical：不解析目录连接(junction)。Windows自己解析".."也是词法的，
	// 所以规范化前后指的是同一个目录；canonical会把经过junction的路径换成它的真实目标(打包产物里Resource/就是junction)。
	string NormalizePath(const string& path) {
		if (path.empty()) return path;
		error_code absoluteError;
		filesystem::path absolute = filesystem::absolute(filesystem::path(path), absoluteError);
		if (absoluteError) absolute = filesystem::path(path);
		string result = ToForwardSlashes(absolute.lexically_normal().string());
		while (result.size() > 3 && result.back() == '/') result.pop_back(); // "C:/"这种根目录保留斜杠
		if (result.size() >= 2 && result[1] == ':') result[0] = static_cast<char>(toupper(static_cast<unsigned char>(result[0])));
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

void Config::ClearState() {
	dllPaths.clear();
	layoutPaths.clear();
	resourcePaths.clear();
	resourceRootOrder.clear();
	resourceFilesByRoot.clear();
	pluginPaths.clear();
	pakPaths.clear();
	mainStoryScriptModName.clear();
	hasGlobalSeed = false;
	globalSeed = 0;
	hasMapSize = false;
	mapWidth = mapHeight = 0;
	conceptMods.clear();
	stashedModArgs.clear();
	dllConceptIds.clear();
	modDependences.clear();
	storyScriptPaths.clear();
}

void Config::ReadConfig(const string& path) {
	// 读到一半出错(例如dll_paths里混进了不是字符串的元素)不能留下半份配置：整份清空，和"文件打不开"一样留空状态，
	// 调用方本来就按"读完是空的"判定配置无效。
	lastReadError.clear();
	ErrorReport report;
	// 文件scope要放在GuardCall里面：异常得穿过它才会被记进报告，放在外面的话异常在到它之前就被接住了。
	if (GuardCall([&] { ErrorScope fileScope(kErrorScopeFile, path); ReadConfigFile(path); }, report)) return;
	ClearState();
	lastReadError = report.ToLine();
	debugf(DebugLevel::Error, "[Config] failed to read %s, the config is left empty: %s\n", path.c_str(), lastReadError.c_str());
}

const string& Config::GetLastReadError() { return lastReadError; }

void Config::ReadConfigFile(const string& path) {
	ClearState();

	ifstream fin(path);
	if (!fin.is_open()) {
		// 留空不抛异常，调用方据此回退默认目录；但要打警告，区分"文件打不开"(路径错/被独占)和"JSON语法错误"。
		debugf(DebugLevel::Warning, "[Config] cannot open config file: %s\n", path.c_str());
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
		debugf(DebugLevel::Error, "[Config] json syntax error in %s: %s\n", path.c_str(), reader.GetErrorMessages().c_str());
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

	hasGlobalSeed = false;
	globalSeed = 0;
	// seed必须是0到2^64-1之间的整数。负数和超范围的数不接受(打警告，当作没配置)：strtoull遇到负号会悄悄绕成一个
	// 很大的数，double超出范围再转无符号整数则是未定义行为，两种都会让同一份配置在不同地方得到不同的世界。
	if (root["seed"].IsString()) {
		const string text = root["seed"].AsString();
		char* end = nullptr;
		errno = 0;
		unsigned long long value = strtoull(text.c_str(), &end, 10);
		const bool negative = text.find('-') != string::npos;
		if (!text.empty() && end != text.c_str() && !negative && errno != ERANGE) { hasGlobalSeed = true; globalSeed = value; }
		else if (!text.empty()) debugf(DebugLevel::Warning, "[Config] \"seed\" must be an integer between 0 and 18446744073709551615, \"%s\" is ignored.\n", text.c_str());
	}
	else if (root["seed"].IsNumeric()) {
		const double value = root["seed"].AsDouble();
		// 18446744073709551616.0 = 2^64，是第一个放不进unsigned long long的值。
		if (value >= 0.0 && value < 18446744073709551616.0) { hasGlobalSeed = true; globalSeed = static_cast<unsigned long long>(value); }
		else debugf(DebugLevel::Warning, "[Config] \"seed\" must be an integer between 0 and 18446744073709551615, %g is ignored.\n", value);
	}

	// map_size：数字=正方形，[宽,高]=长方形；非正整数/格式不对打警告并当没配置。
	{
		const JsonValue& sizeValue = root["map_size"];
		int w = 0, h = 0;
		bool present = false, valid = false;
		if (sizeValue.IsNumeric()) {
			present = true;
			w = h = sizeValue.AsInt();
			valid = true;
		}
		else if (sizeValue.IsArray()) {
			present = true;
			if (sizeValue.size() == 2 && sizeValue[0u].IsNumeric() && sizeValue[1u].IsNumeric()) {
				w = sizeValue[0u].AsInt();
				h = sizeValue[1u].AsInt();
				valid = true;
			}
		}
		if (present && (!valid || w <= 0 || h <= 0)) {
			debugf(DebugLevel::Warning, "[Config] \"map_size\" must be a positive integer or [width, height], ignored.\n");
		}
		else if (present) {
			hasMapSize = true;
			mapWidth = w;
			mapHeight = h;
		}
	}

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

void Config::AddDllPath(const string& rawPath) {
	const string path = NormalizePath(rawPath); // 同一个目录只认一种写法
	filesystem::path dir(path);
	if (!filesystem::exists(dir) || !filesystem::is_directory(dir)) {
		debugf(DebugLevel::Warning, "[Config] mod path does not exist: %s\n", path.c_str());
		return;
	}

	RemoveDllPath(path);

	// skip_permission_denied：递归到无权限子目录时跳过，而不是抛filesystem_error中断整次扫描
	error_code dirIterError;
	for (const auto& entry : filesystem::recursive_directory_iterator(
			dir, filesystem::directory_options::skip_permission_denied, dirIterError)) {
		if (!CheckFileFormat(entry.path(), ".dll"))
			continue;

		// 先只读文件看导出表/导入表(ProbeModDll，不加载、不执行)：没有导出任何GetMod<Concepts>的不是mod，直接跳过——
		// 这里会无差别递归整个目录树，无关dll的DllMain不能因为"探测一下"就被执行。
		// 调试版(导入了调试CRT)也跳过：/MDd的std::string布局和Release不兼容，跨DLL读返回的vector<string>会崩(见config.md)。
		const ModDllProbe probe = ProbeModDll(entry.path().string());
		if (!probe.isMod || probe.isDebug)
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

void Config::RemoveDllPath(const string& rawPath) {
	const string path = NormalizePath(rawPath); // 同一个目录只认一种写法
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
	// 按资源根目录的顺序：几个目录里有同名的.layout(模板名=文件名)时只留排在前面的那个，后面的打警告丢掉。
	unordered_map<string, string> seen; // 模板名 -> 采用的路径
	for (const string& root : resourceRootOrder) {
		auto it = layoutPaths.find(root);
		if (it == layoutPaths.end()) continue;
		for (const auto& layout : it->second) {
			const string templateName = filesystem::path(layout).stem().string();
			auto [existing, inserted] = seen.emplace(templateName, layout);
			if (!inserted) {
				debugf(DebugLevel::Warning, "[Config] layout \"%s\" exists more than once: using %s, ignoring %s\n", templateName.c_str(), existing->second.c_str(), layout.c_str());
				continue;
			}
			paths.push_back(layout);
		}
	}
	return paths;
}

void Config::AddResourcePath(const string& rawPath) {
	const string path = NormalizePath(rawPath); // 同一个目录只认一种写法
	filesystem::path dir(path);
	if (!filesystem::exists(dir) || !filesystem::is_directory(dir)) {
		debugf(DebugLevel::Warning, "[Config] resource path does not exist: %s\n", path.c_str());
		return;
	}

	resourcePaths.erase(path);
	pluginPaths.erase(path);
	pakPaths.erase(path);

	vector<string> scripts;
	vector<string> plugins;
	vector<string> paks;
	vector<string> layouts;
	unordered_map<string, string> files; // 文件名 -> 完整路径，给GetResourceFilePath用
	// skip_permission_denied+error_code：递归到无权限子目录时跳过，而不是抛filesystem_error中断整次扫描(和AddDllPath一样)。
	error_code dirIterError;
	for (const auto& entry : filesystem::recursive_directory_iterator(
			dir, filesystem::directory_options::skip_permission_denied, dirIterError)) {
		error_code absoluteError;
		const filesystem::path absolutePath = filesystem::absolute(entry.path(), absoluteError);
		if (absoluteError) continue;
		string full = ToForwardSlashes(absolutePath.string());
		error_code typeError;
		if (entry.is_regular_file(typeError) && !typeError) files.emplace(entry.path().filename().string(), full); // 同一个目录里重名取先扫到的
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
	// 脚本按名字(文件名去掉扩展名)找：别的目录里已经有同名脚本时提醒一下，实际用排在前面的目录里的那个。
	const bool known = find(resourceRootOrder.begin(), resourceRootOrder.end(), path) != resourceRootOrder.end();
	if (!known) resourceRootOrder.push_back(path); // 重扫已有的目录不改变它的位置
	for (const string& script : scripts) {
		const string name = filesystem::path(script).stem().string();
		for (const string& otherRoot : resourceRootOrder) {
			if (otherRoot == path) continue;
			auto other = resourcePaths.find(otherRoot);
			if (other == resourcePaths.end()) continue;
			for (const string& otherScript : other->second) {
				if (filesystem::path(otherScript).stem().string() != name) continue;
				debugf(DebugLevel::Warning, "[Config] script \"%s\" exists in more than one resource path (%s and %s); the path listed first wins.\n", name.c_str(), otherScript.c_str(), script.c_str());
			}
		}
	}
	resourcePaths[path] = scripts;
	resourceFilesByRoot[path] = std::move(files);
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
	// 按资源根目录的顺序找：几个目录里有同名脚本时，排在前面的目录优先(AddResourcePath扫描时已经打过警告)。
	for (const string& root : resourceRootOrder) {
		auto it = resourcePaths.find(root);
		if (it == resourcePaths.end()) continue;
		for (const auto& script : it->second) {
			if (filesystem::path(script).stem().string() == name) {
				return script;
			}
		}
	}
	return string();
}

string Config::GetResourceFilePath(const string& fileName) {
	// 查AddResourcePath扫描时建好的索引，不碰磁盘。所以游戏进行中新放进资源目录的文件要等下一次RescanFiles
	// (开始新游戏、读档)才找得到——和.script/.layout一样。目录顺序同GetScriptPath。
	for (const string& root : resourceRootOrder) {
		auto files = resourceFilesByRoot.find(root);
		if (files == resourceFilesByRoot.end()) continue;
		auto file = files->second.find(fileName);
		if (file != files->second.end()) return file->second;
	}
	return string();
}

bool Config::GetGlobalSeed(unsigned long long& seed) {
	seed = globalSeed;
	return hasGlobalSeed;
}

bool Config::GetMapSize(int& width, int& height) {
	width = mapWidth;
	height = mapHeight;
	return hasMapSize;
}

string Config::GetMainStoryScriptModName() {
	return mainStoryScriptModName;
}

vector<string> Config::GetMainStoryScriptPaths() {
	return storyScriptPaths;
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
	const vector<string> resourceRoots = resourceRootOrder;
	for (const string& root : resourceRoots) AddResourcePath(root);
}

vector<string> Config::FindMissingScripts(const vector<string>& scripts) {
	vector<string> missing;
	for (const string& name : scripts) {
		if (!GetScriptPath(name).empty()) continue;
		// 存档manifest里的脚本名也包括story_scripts配置的脚本，它们的文件可以不在任何resource_paths里。
		bool isStoryScript = false;
		for (const string& storyPath : storyScriptPaths) {
			if (filesystem::path(storyPath).stem().string() == name && filesystem::exists(storyPath)) { isStoryScript = true; break; }
		}
		if (!isStoryScript) missing.push_back(name);
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
	const vector<string>& unique = UniqueConcepts();
	return find(unique.begin(), unique.end(), conceptKey) != unique.end();
}

string Config::GetUniqueConceptMod(const string& conceptKey) {
	const vector<pair<string, string>> mods = GetConceptMods(ConceptKeyToJsonKey(conceptKey));
	if (mods.empty()) return string();
	if (mods.size() > 1) {
		debugf(DebugLevel::Warning, "[Config] %d %s mods are enabled but only one can be used; using \"%s\".\n",
			static_cast<int>(mods.size()), conceptKey.c_str(), mods.front().first.c_str());
	}
	return mods.front().first;
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
			// 新启用的id：带上之前禁用时暂存的参数(只在内存)，没有就给空串，和AddDllPath发现新mod时的默认行为一致。
			string args;
			auto stash = stashedModArgs.find(StashKey(jsonKey, modId));
			if (stash != stashedModArgs.end()) { args = stash->second; stashedModArgs.erase(stash); }
			entries.push_back({ modId, args });
		}
		// 已经在数组里就不用动,保留原有的参数字符串。
	} else if (it != entries.end()) {
		if (!it->second.empty()) stashedModArgs[StashKey(jsonKey, modId)] = it->second;
		entries.erase(it);
	}

	// Roadnets/Names单选：选中后把同concept下其它已发现id全部从数组移除
	if (enabled && IsUniqueConcept(conceptKey)) {
		for (const auto& [dllPath, ids] : dllConceptIds) {
			(void)dllPath;
			for (const auto& [ck, id] : ids) {
				if (ck == conceptKey && id != modId) {
					auto& list = conceptMods[jsonKey];
					for (const auto& e : list) {
						if (e.first == id && !e.second.empty()) stashedModArgs[StashKey(jsonKey, id)] = e.second;
					}
					list.erase(
						remove_if(list.begin(), list.end(),
							[&id](const pair<string, string>& e) { return e.first == id; }),
						list.end());
				}
			}
		}
	}
}

string Config::StashKey(const string& jsonKey, const string& modId) {
	return jsonKey + '\n' + modId;
}

string Config::GetModArgs(const string& conceptKey, const string& modId) {
	string jsonKey = ConceptKeyToJsonKey(conceptKey);
	auto conceptIt = conceptMods.find(jsonKey);
	if (conceptIt != conceptMods.end()) {
		for (const auto& [id, args] : conceptIt->second) {
			if (id == modId) return args;
		}
	}
	auto stash = stashedModArgs.find(StashKey(jsonKey, modId));
	return stash == stashedModArgs.end() ? string() : stash->second;
}

void Config::SetModArgs(const string& conceptKey, const string& modId, const string& args) {
	string jsonKey = ConceptKeyToJsonKey(conceptKey);
	auto conceptIt = conceptMods.find(jsonKey);
	if (conceptIt != conceptMods.end()) {
		for (auto& entry : conceptIt->second) {
			if (entry.first == modId) { entry.second = args; return; }
		}
	}
	// 没启用：暂存在内存，下次启用时带上；空串就是清掉暂存。
	if (args.empty()) stashedModArgs.erase(StashKey(jsonKey, modId));
	else stashedModArgs[StashKey(jsonKey, modId)] = args;
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

vector<pair<string, string>> Config::GetModIdsForDll(const string& dllPath) {
	auto it = dllConceptIds.find(dllPath);
	return it != dllConceptIds.end() ? it->second : vector<pair<string, string>>();
}

vector<string> Config::GetModDependences(const string& modId) {
	auto it = modDependences.find(modId);
	return it != modDependences.end() ? it->second : vector<string>();
}

vector<string> Config::GetResourceRootPaths() {
	return resourceRootOrder; // 配置里的先后顺序，也是同名文件的优先顺序
}

void Config::RemoveResourcePath(const string& rawPath) {
	const string path = NormalizePath(rawPath); // 同一个目录只认一种写法
	resourcePaths.erase(path);
	resourceFilesByRoot.erase(path);
	resourceRootOrder.erase(remove(resourceRootOrder.begin(), resourceRootOrder.end(), path), resourceRootOrder.end());
	pluginPaths.erase(path);
	pakPaths.erase(path);
	layoutPaths.erase(path);
}

void Config::AddStoryScript(const string& path) {
	if (!CheckFileFormat(filesystem::path(path), ".script")) {
		debugf(DebugLevel::Warning, "[Config] not a .script file: %s\n", path.c_str());
		return;
	}
	string full = NormalizePath(path);
	if (find(storyScriptPaths.begin(), storyScriptPaths.end(), full) == storyScriptPaths.end()) {
		storyScriptPaths.push_back(full);
	}
}

void Config::RemoveStoryScript(const string& path) {
	string full = NormalizePath(path);
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
	for (const string& rootPath : resourceRootOrder) {
		resourceArr.append(JsonValue(rootPath));
	}
	root["resource_paths"] = resourceArr;

	root["main_story"] = JsonValue(mainStoryScriptModName);
	if (hasGlobalSeed) root["seed"] = JsonValue(to_string(globalSeed));
	if (hasMapSize) {
		JsonValue sizeArr(DATA_ARRAY);
		sizeArr.append(JsonValue(mapWidth));
		sizeArr.append(JsonValue(mapHeight));
		root["map_size"] = sizeArr;
	}

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
		debugf(DebugLevel::Warning, "[Config] cannot write config to %s\n", path.c_str());
		return;
	}
	fout << text;
}
