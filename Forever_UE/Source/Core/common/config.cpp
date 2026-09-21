#include "config.h"

#include "json.h"

#include "loader.h"

#include <fstream>
#include <iostream>
#include <unordered_set>
#include <cctype>
#include <windows.h>


using namespace std;

string Config::configDir = "";
unordered_map<string, vector<string>> Config::dllPaths = {};
unordered_map<string, vector<string>> Config::layoutPaths = {};
unordered_map<string, vector<string>> Config::resourcePaths = {};
string Config::mainStoryScriptModName = "";
unordered_map<string, vector<pair<string, string>>> Config::conceptMods = {};

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
	mainStoryScriptModName.clear();
	conceptMods.clear();

	ifstream fin(path);
	if (!fin.is_open()) {
		// 找不到config.json时静默留空,不报错——调用方(ForeverModSubsystem)据此
		// 决定要不要回退到硬编码默认目录,和UForeverKeyBindingSubsystem的容错风格一致。
		return;
	}
	configDir = filesystem::path(path).parent_path().string();

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
		AddDllPath(resolved.string());
	}

	for (const auto& layoutPath : root["layout_paths"]) {
		// 和dll_paths同一个相对路径解析规则，相对configDir。
		filesystem::path resolved(layoutPath.AsString());
		if (resolved.is_relative()) {
			resolved = filesystem::path(configDir) / resolved;
		}
		AddLayoutPath(resolved.string());
	}

	for (const auto& resourcePath : root["resource_paths"]) {
		// 和dll_paths/layout_paths同一个相对路径解析规则，相对configDir。
		filesystem::path resolved(resourcePath.AsString());
		if (resolved.is_relative()) {
			resolved = filesystem::path(configDir) / resolved;
		}
		AddResourcePath(resolved.string());
	}

	mainStoryScriptModName = root["main_story"].AsString();

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

	for (const auto& entry : filesystem::recursive_directory_iterator(dir)) {
		if (!CheckFileFormat(entry.path(), ".dll"))
			continue;

		string full = filesystem::absolute(entry.path()).string();
		bool valid = false;

		HMODULE modHandle = LoadLibraryA(full.data());
		if (modHandle) {
			for (const auto& descriptor : GetModConceptDescriptors()) {
				if (GetProcAddress(modHandle, descriptor.getModSymbol)) {
					valid = true;
					break;
				}
			}
			FreeLibrary(modHandle);
		}

		if (valid) {
			dllPaths[path].push_back(full);
		}
	}
}

void Config::RemoveDllPath(const string& path) {
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
		found.push_back(filesystem::absolute(entry.path()).string());
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

	vector<string> found;
	for (const auto& entry : filesystem::recursive_directory_iterator(dir)) {
		if (!CheckFileFormat(entry.path(), ".script"))
			continue;
		found.push_back(filesystem::absolute(entry.path()).string());
	}
	resourcePaths[path] = found;
}

bool Config::HasResourcePaths() {
	return !resourcePaths.empty();
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
