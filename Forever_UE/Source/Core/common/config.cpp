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
unordered_map<string, vector<string>> Config::dllDependences = {};
vector<string> Config::storyScriptPaths = {};

void Config::NormalizeUniqueConcepts() {
	static const vector<string> kUniqueConcepts = { "Roadnets", "Names" };

	for (const string& conceptKey : kUniqueConcepts) {
		string jsonKey = ConceptKeyToJsonKey(conceptKey);
		auto it = conceptMods.find(jsonKey);
		if (it == conceptMods.end())
			continue;
		// 只保留数组里排在最前面的那个条目，其余直接从数组删掉——这张数组本身就是启用
		// 状态的唯一真相来源，收口即直接截断数组，不需要额外的平行状态。
		if (it->second.size() > 1) {
			it->second.erase(it->second.begin() + 1, it->second.end());
		}
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
	// filesystem::absolute()/canonical()等"分解再重组"的操作在Windows下会把路径分隔符
	// 规范化成反斜杠，即使原始输入是正斜杠——这个字符串一旦原样塞进JSON字符串值(反斜杠
	// 后面跟的如果不是合法JSON转义字符,比如"\W"/"\P"/"\F")就是非法JSON,这份自定义
	// JsonReader遇到一处非法转义会导致整个文件解析失败(不只是这一个字段读不到,
	// dll_paths等其它字段也会被一起扔掉)——真实复现过一次：AddStoryScript存进
	// storyScriptPaths的路径带着反斜杠，写回config.json后下次启动读不进来，
	// Config::GetMods()因此是空的,触发"回退扫描整个Forever_Mod"，表现成
	// "dll路径只剩一个"。统一在这里收口成正斜杠，不是只修story_scripts这一处。
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
	dllDependences.clear();
	storyScriptPaths.clear();

	ifstream fin(path);
	if (!fin.is_open()) {
		// 找不到/打不开config.json时留空,不抛异常——调用方(ForeverModSubsystem)据此
		// 决定要不要回退到硬编码默认目录,和UForeverKeyBindingSubsystem的容错风格一致。
		// 但这里必须打一条警告(不是彻底静默)：这个分支和下面"json语法错误"分支长得很像
		// (两者最终都导致dll_paths/resource_paths/layout_paths全部回退)，但根因完全不同
		// (前者是文件都没打开——可能是路径错了，也可能是另一个进程这一瞬间以独占方式
		// 持有这个文件比如正在WriteConfig——而不是内容有问题)，没有这条日志，排查"为什么
		// 回退扫描了"时无法区分这两种情况，容易误以为是config.json内容写错了。
		cerr << "[Config] Warning: cannot open config file: " << path << "\n";
		return;
	}

	// 必须用canonical()而不是直接取parent_path()——Windows对一条路径字符串里嵌入的".."是
	// 纯文本/词法化解析(GetFullPathNameW那一套，不会真的打开每一级目录去看是不是reparse
	// point)，不会"看穿"目录连接(junction)。打包产物里Resource/是用mklink /J连接指向开发树
	// 真实位置的(见Forever_UE/pak.bat)，如果这里只取parent_path()，configDir会是一条还停留
	// 在"连接点那一侧"的路径，后面dll_paths/resource_paths里"../../../Forever_Mod/..."这种
	// 相对写法对着这条路径词法化展开，文本上只会在连接点所在的归档目录树内部兜圈子，永远
	// 走不到连接指向的真实开发树里的Forever_Mod——用canonical()强制走一次真正打开目录、
	// 跟随reparse point的OS级解析，把configDir变成连接目标那一侧的真实绝对路径，后续所有
	// 相对路径的".."才会从正确的起点往上退。这一步只需要做一次，PIE/未打包的开发环境下
	// Resource本来就是真目录，canonical()是无副作用的空操作。
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
		// dll_paths里的相对路径,相对config.json自己所在目录(configDir)解析——纯文件系统
		// 概念,不依赖UE的FPaths::ProjectDir,保持Config engine-agnostic。
		filesystem::path resolved(dllPath.AsString());
		if (resolved.is_relative()) {
			resolved = filesystem::path(configDir) / resolved;
		}
		AddDllPath(ToForwardSlashes(resolved.string()));
	}

	for (const auto& layoutPath : root["layout_paths"]) {
		// 和dll_paths同一个相对路径解析规则，相对configDir。
		filesystem::path resolved(layoutPath.AsString());
		if (resolved.is_relative()) {
			resolved = filesystem::path(configDir) / resolved;
		}
		AddLayoutPath(ToForwardSlashes(resolved.string()));
	}

	for (const auto& resourcePath : root["resource_paths"]) {
		// 和dll_paths/layout_paths同一个相对路径解析规则，相对configDir。
		filesystem::path resolved(resourcePath.AsString());
		if (resolved.is_relative()) {
			resolved = filesystem::path(configDir) / resolved;
		}
		AddResourcePath(ToForwardSlashes(resolved.string()));
	}

	mainStoryScriptModName = root["main_story"].AsString();

	for (const auto& scriptPath : root["story_scripts"]) {
		// 和dll_paths/layout_paths/resource_paths同一个相对路径解析规则，相对configDir。
		filesystem::path resolved(scriptPath.AsString());
		if (resolved.is_relative()) {
			resolved = filesystem::path(configDir) / resolved;
		}
		AddStoryScript(ToForwardSlashes(resolved.string()));
	}

	// 任何以"_mods"结尾的顶层key都当作一个concept的mod列表解析(如"building_mods"),不
	// 硬编码20个concept的名字——config.json本身决定内容,和旧工程的写法(每个concept一个
	// "<concept>_mods"数组)保持一致。每个数组元素是形如"id"或"id 参数..."的字符串,按第一个
	// 空格切成(id, 参数字符串)。
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

	// 文件里Roadnets/Names可能同时列着好几个条目(老版本手写的config.json本来就不限制
	// 数量)，读完立刻收口成最多一个，不等到UI查询或者下次保存才发现。
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

	// skip_permission_denied：文件夹选得范围越大，越容易递归到没有访问权限的子目录
	// (比如系统目录)——默认选项下，光是构造/自增这个iterator碰到一个这样的子目录就会
	// 抛filesystem::filesystem_error，没有这个选项的话一个子目录没权限就会导致整次
	// 扫描提前中断，这个选项让迭代器自己跳过那个子目录继续，不需要我们在这里额外
	// try/catch。
	error_code dirIterError;
	for (const auto& entry : filesystem::recursive_directory_iterator(
			dir, filesystem::directory_options::skip_permission_denied, dirIterError)) {
		if (!CheckFileFormat(entry.path(), ".dll"))
			continue;

		// 跳过路径里带"Debug"目录段的dll——Debug配置用/MDd调试CRT编译,std::string内存
		// 布局和这个模块(Development/Release)不兼容,跨DLL读它返回的vector<string>内容
		// 会读到按调试CRT布局解释的垂悬/错位数据,直接崩(真实复现过一次:回退扫描整个
		// Forever_Mod时扫到x64/Debug/下的旧dll,在下面读取GetMod<Concept>s()返回的
		// vector<string>时EXCEPTION_ACCESS_VIOLATION，见config.md)。手工配置的dll_paths
		// 一直靠人工只填Release目录规避这个坑,但回退扫描(ForeverModSubsystem::Initialize()
		// 的默认目录分支)会无差别递归整个目录树,必须在这里挡掉,不能指望每条路径都是人工
		// 精确配置的。
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
		vector<string> dependences;

		HMODULE modHandle = LoadLibraryA(full.data());
		if (modHandle) {
			// 这次不再只探测符号是否存在——符号存在就真的调用一次，读取返回的
			// vector<string>*内容(mod内static vector，调用方只读不释放，和
			// mod_empty.cpp::GetMod<Concept>s()的既有形状一致，在本工程统一动态CRT
			// (MultiThreadedDLL/MultiThreadedDebugDLL)下安全)，拿到这个dll实际提供
			// 哪些(concept,modId)，FreeLibrary之前完成，不长期持有句柄。
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

			// 新增的DLL级依赖声明探测——整个DLL一份，不按concept重复，见loader.h
			// kModDllDependenciesSymbol注释。
			auto getDepsFunc = reinterpret_cast<GetModFunc>(GetProcAddress(modHandle, kModDllDependenciesSymbol));
			if (getDepsFunc) {
				if (void* raw = getDepsFunc()) {
					dependences = *static_cast<vector<string>*>(raw);
				}
			}

			FreeLibrary(modHandle);
		}

		if (valid) {
			dllPaths[path].push_back(full);
			dllConceptIds[full] = ids;
			dllDependences[full] = dependences;
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
				}
			}
			dllConceptIds.erase(dll);
			dllDependences.erase(dll);
		}
	}
	dllPaths.erase(path);
}

void Config::AddLayoutPath(const string& path) {
	filesystem::path dir(path);
	if (!filesystem::exists(dir) || !filesystem::is_directory(dir)) {
		cerr << "[Config] Warning: layout path does not exist: " << path << "\n";
		return;
	}

	layoutPaths.erase(path);

	vector<string> found;
	for (const auto& entry : filesystem::recursive_directory_iterator(dir)) {
		if (!CheckFileFormat(entry.path(), ".layout"))
			continue;
		found.push_back(ToForwardSlashes(filesystem::absolute(entry.path()).string()));
	}
	layoutPaths[path] = found;
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
			// 独立迷你UE工程里的Plugin——WITH_EDITOR下直接挂载这个Plugin的Content目录，
			// 不需要cook/pak，见config.md"打包功能迁移"一节。
			plugins.push_back(full);
		} else if (CheckFileFormat(entry.path(), ".pak")) {
			// 同一个Plugin cook+UnrealPak出来的产物，非编辑器(打包)下MountPaksEx挂载。
			paks.push_back(full);
		} else if (CheckFileFormat(entry.path(), ".layout")) {
			// 游戏启动配置界面的Resource screen要求一个文件夹同时覆盖.layout和.script，
			// 这里顺带收集进现有的layoutPaths(和AddLayoutPath共用同一个map，不新增容器)，
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

string Config::GetMainStoryScriptModName() {
	return mainStoryScriptModName;
}

string Config::GetMainStoryScriptPath() {
	return GetScriptPath("test");
}

vector<pair<string, string>> Config::GetConceptMods(const string& jsonKey) {
	auto it = conceptMods.find(jsonKey);
	if (it == conceptMods.end()) {
		return {};
	}
	return it->second;
}

// ---- 游戏启动配置界面(Story/Mod/Resource三screen)新增 ----

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

void Config::SetModEnabled(const string& conceptKey, const string& modId, bool enabled) {
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

	// Roadnet/Name这两个concept只应该同时生效一个(不是Terrain那种按GetPriority()多mod
	// 叠加的语义，见roadnet_factory.md)——选中一个之后，把这个concept下所有已发现的其它
	// mod id都从数组里删掉，不是"UI上看起来单选"，是真的从"<concept>_mods"数组移除。
	if (enabled && (conceptKey == "Roadnets" || conceptKey == "Names")) {
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

vector<string> Config::GetDllDependences(const string& dllPath) {
	auto it = dllDependences.find(dllPath);
	return it != dllDependences.end() ? it->second : vector<string>();
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

	// 直接写绝对路径，不再转成相对configDir的路径——config.json本来就不是跨设备使用的
	// 文件(每台开发机上mod/资源的实际存放位置也经常不一样)，相对路径解析ConfigDir/
	// canonical那一套反而更容易因为目录连接(junction)、当前这次是不是打包产物等情况产生
	// 让人困惑的歧义(配置界面这几个Add按钮从原生文件/文件夹选择器拿到的本来就已经是绝对
	// 路径)。dllPaths/layoutPaths/resourcePaths的key、storyScriptPaths的内容在内存里已经
	// 是绝对路径(AddDllPath/AddResourcePath/AddLayoutPath调用前，ReadConfig或原生选择器
	// 都已经解析/返回成绝对路径)，这里原样写出即可。ReadConfig那边保留对相对路径的兼容
	// 解析(没坏处，只是以后不会再有代码主动生成相对路径这种写法了)。
	JsonValue root(DATA_OBJECT);

	JsonValue dllArr(DATA_ARRAY);
	for (const auto& [rootPath, _] : dllPaths) {
		(void)_;
		dllArr.append(JsonValue(rootPath));
	}
	root["dll_paths"] = dllArr;

	JsonValue layoutArr(DATA_ARRAY);
	for (const auto& [rootPath, _] : layoutPaths) {
		(void)_;
		layoutArr.append(JsonValue(rootPath));
	}
	root["layout_paths"] = layoutArr;

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
