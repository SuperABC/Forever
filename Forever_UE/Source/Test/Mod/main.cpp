// ModTest：给mod作者用的mod自测程序，只依赖Dependence(不需要Core和UE)。用法和检查项见ModTest.md。
#include "mod_checks.h"
#include "test_framework.h"

#include "common/json.h"
#include "common/mod_concepts.h"
#include "common/random.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>


using namespace std;
namespace fs = std::filesystem;

namespace {
	struct Options {
		vector<string> testPaths;   // 要测的dll或目录
		vector<string> withPaths;   // 只加载、不测的dll或目录(被测mod依赖/引用的别人的mod)
		string configPath;          // 可选：从这里取每个mod的参数；没给testPaths时也从这里取dll_paths
		string outDir;              // 可选：报告目录
		unsigned long long seed = 11;
	};

	// 一个被加载的mod dll。
	struct LoadedDll {
		string path;
		HMODULE handle = nullptr;
		bool tested = false;                       // false=--with带进来的，只提供id
		bool explicitFile = false;                 // 命令行里直接点名的文件(不是从目录里扫出来的)
		vector<pair<string, string>> ids;          // (conceptKey, modId)
		vector<pair<string, vector<string>>> dependencies; // GetModDependencies
		bool exportsBindRandomHost = false;
	};

	string ToForwardSlashes(string path) {
		for (char& c : path) if (c == '\\') c = '/';
		return path;
	}

	string LowerCase(string text) {
		for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
		return text;
	}

	// 目录里的dll(递归)。跳过带"Debug"目录段的：/MDd调试CRT的std::string布局和Release不兼容，读它返回的id列表会崩(同Config::AddDllPath)。
	void CollectDlls(const string& path, vector<pair<string, bool>>& out, vector<string>& errors) {
		error_code ec;
		const fs::path root(path);
		if (fs::is_regular_file(root, ec)) {
			out.emplace_back(ToForwardSlashes(fs::absolute(root).string()), true);
			return;
		}
		if (!fs::is_directory(root, ec)) {
			errors.push_back("path does not exist: " + path);
			return;
		}
		vector<string> found;
		for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
			if (!it->is_regular_file(ec)) continue;
			if (LowerCase(it->path().extension().string()) != ".dll") continue;
			bool underDebug = false;
			for (const auto& part : it->path()) if (LowerCase(part.string()) == "debug") underDebug = true;
			if (underDebug) continue;
			found.push_back(ToForwardSlashes(fs::absolute(it->path()).string()));
		}
		sort(found.begin(), found.end());
		for (const string& file : found) out.emplace_back(file, false);
	}

	// Forever_Mod/Empty(Empty.dll)里的mod是给mod作者抄的占位模板：按提供它的dll识别，不看mod id长什么样。
	bool IsEmptyTemplateDll(const string& dllPath) {
		return LowerCase(fs::path(dllPath).stem().string()) == "empty";
	}

	// mod id的硬性要求：非空、不含空白和控制字符。config.json里每个条目是"id 参数..."，按第一个空格切开，id里有空白就配不出来。
	// 除此之外不限制格式(<concept>_<name>只是弱约定)。
	string IdProblem(const string& id) {
		if (id.empty()) return "id is empty";
		for (unsigned char c : id) {
			if (isspace(c) || c < 0x20 || c == 0x7f) return "id contains whitespace or a control character";
		}
		return string();
	}

	bool ParseOptions(int argc, char** argv, Options& options) {
		for (int i = 1; i < argc; i++) {
			const string arg = argv[i];
			auto next = [&](string& out) {
				if (i + 1 >= argc) { fprintf(stderr, "missing value after %s\n", arg.c_str()); return false; }
				out = argv[++i];
				return true;
			};
			if (arg == "--with") { string value; if (!next(value)) return false; options.withPaths.push_back(value); }
			else if (arg == "--config") { if (!next(options.configPath)) return false; }
			else if (arg == "--out") { if (!next(options.outDir)) return false; }
			else if (arg == "--seed") { string value; if (!next(value)) return false; options.seed = strtoull(value.c_str(), nullptr, 10); }
			else if (arg == "--help" || arg == "-h" || arg == "/?") return false;
			else if (arg.rfind("--", 0) == 0) { fprintf(stderr, "unknown option %s\n", arg.c_str()); return false; }
			else options.testPaths.push_back(arg);
		}
		return !options.testPaths.empty() || !options.configPath.empty();
	}

	void PrintUsage() {
		fprintf(stderr,
			"usage: ModTest <mod.dll|dir>... [--with <mod.dll|dir>]... [--config config.json] [--seed N] [--out dir]\n"
			"  <mod.dll|dir>   mod dlls to test (a directory is scanned recursively, Debug folders are skipped)\n"
			"  --with          dlls that are only loaded: their mod ids resolve references/dependencies of the tested mods\n"
			"                  and take part in the duplicate-id check, but their mods are not tested\n"
			"  --config        take each mod's arguments from the \"<concept>_mods\" arrays of this config.json;\n"
			"                  when no dll/dir is given, test every dll under its \"dll_paths\"\n"
			"  --seed          random seed for the mods' random streams (default 11)\n"
			"  --out           write modtest.json / modtest.txt reports into this directory\n"
			"exit code: 0 = all checks passed, 1 = at least one check failed, 2 = bad arguments\n");
	}

	// config.json：<concept>_mods数组的"id 参数..."条目 -> (jsonKey, id) -> 参数；dll_paths(相对路径相对config所在目录)。
	bool ReadConfig(const string& path, map<pair<string, string>, string>& args, vector<string>& dllPaths, string& error) {
		ifstream in(path);
		if (!in) { error = "cannot open config " + path; return false; }
		JsonReader reader;
		JsonValue root;
		if (!reader.Parse(in, root)) { error = "bad config json: " + reader.GetErrorMessages(); return false; }
		const fs::path configDir = fs::absolute(fs::path(path)).parent_path();
		for (const auto& dllPath : root["dll_paths"]) {
			fs::path resolved(dllPath.AsString());
			if (resolved.is_relative()) resolved = configDir / resolved;
			dllPaths.push_back(ToForwardSlashes(resolved.string()));
		}
		const string suffix = "_mods";
		for (const string& key : root.GetMembers()) {
			if (key.size() <= suffix.size() || key.compare(key.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
			if (!root[key].IsArray()) continue;
			for (const auto& entry : root[key]) {
				const string raw = entry.AsString();
				const size_t space = raw.find(' ');
				args[{ key, raw.substr(0, space) }] = space == string::npos ? string() : raw.substr(space + 1);
			}
		}
		return true;
	}

	int Run(const Options& options, const string& repro) {
		TestRun& run = TestRun::Get();
		run.Begin("modtest", options.seed, repro);

		// ---- 配置(可选) ----
		map<pair<string, string>, string> configuredArgs;
		vector<string> configDllPaths;
		if (!options.configPath.empty()) {
			string error;
			if (!ReadConfig(options.configPath, configuredArgs, configDllPaths, error)) {
				run.Check(false, "read config", error);
				return 1;
			}
		}

		// ---- 收集dll ----
		vector<string> pathErrors;
		vector<pair<string, bool>> testFiles, withFiles; // (路径, 是否命令行直接点名)
		for (const string& path : options.testPaths) CollectDlls(path, testFiles, pathErrors);
		if (options.testPaths.empty()) for (const string& path : configDllPaths) CollectDlls(path, testFiles, pathErrors);
		for (const string& path : options.withPaths) CollectDlls(path, withFiles, pathErrors);
		for (const string& error : pathErrors) run.Check(false, "input path", error);

		// ---- 加载并探测 ----
		const vector<ModConceptDescriptor>& descriptors = GetModConceptDescriptors();
		ModTestHost host;
		for (const ModConceptDescriptor& descriptor : descriptors) {
			run.Check(host.FindOps(descriptor.conceptKey) != nullptr, string("ModTest knows concept ") + descriptor.conceptKey,
				"add it to ModTestHost in mod_checks.cpp");
		}

		vector<LoadedDll> dlls;
		set<string> loadedPaths;
		auto loadAll = [&](const vector<pair<string, bool>>& files, bool tested) {
			for (const auto& [path, explicitFile] : files) {
				if (!loadedPaths.insert(LowerCase(path)).second) continue; // 同一个文件给了两次
				LoadedDll dll;
				dll.path = path;
				dll.tested = tested;
				dll.explicitFile = explicitFile;
				dll.handle = LoadLibraryA(path.c_str());
				if (!dll.handle) {
					// 目录里扫到的dll可能根本不是mod(依赖库等)，加载不了不算错；点名的文件加载不了才报。
					if (explicitFile) run.Check(false, "dll loads: " + path, "LoadLibrary failed, error " + to_string(GetLastError()));
					continue;
				}
				bool isMod = false;
				for (const ModConceptDescriptor& descriptor : descriptors) {
					auto getMods = reinterpret_cast<void* (*)()>(GetProcAddress(dll.handle, descriptor.getModSymbol));
					if (!getMods) continue;
					isMod = true;
					if (void* raw = getMods()) {
						for (const string& id : *static_cast<vector<string>*>(raw)) dll.ids.emplace_back(descriptor.conceptKey, id);
					}
				}
				if (!isMod) {
					if (explicitFile) run.Check(false, "dll is a mod dll: " + path, "it exports none of the GetMod<Concepts> functions");
					FreeLibrary(dll.handle);
					continue;
				}
				if (auto getDependencies = reinterpret_cast<void* (*)()>(GetProcAddress(dll.handle, kModDependenciesSymbol))) {
					if (void* raw = getDependencies()) dll.dependencies = *static_cast<vector<pair<string, vector<string>>>*>(raw);
				}
				// 把宿主的随机host交给它(和游戏里ModLoader做的一样)，mod里的GetRandom才走受种子控制的流。
				if (auto bind = reinterpret_cast<void(*)(RandomHost*)>(GetProcAddress(dll.handle, kBindRandomHostSymbol))) {
					dll.exportsBindRandomHost = true;
					bind(GetRandomHost());
				}
				dlls.push_back(std::move(dll));
			}
		};
		loadAll(testFiles, true);
		loadAll(withFiles, false);

		int testedDlls = 0;
		for (const LoadedDll& dll : dlls) if (dll.tested) testedDlls++;
		run.Metric("mod dlls tested", testedDlls, "");
		run.Metric("mod dlls loaded for reference", static_cast<double>(dlls.size()) - testedDlls, "");
		run.Check(testedDlls > 0, "at least one mod dll to test");

		// ---- id：合法性 + 全局不重名 ----
		// 规则：所有mod的id全局唯一——不管属于哪个concept、由哪个dll提供，两个mod不能用同一个id。
		// 随机流、依赖声明、脚本的mod_dependences都只按id找mod，重名就会串。
		map<string, vector<string>> owners; // id -> 提供它的"dll文件名 / concept"
		for (const LoadedDll& dll : dlls) {
			const string dllName = fs::path(dll.path).filename().string();
			for (const auto& [conceptKey, id] : dll.ids) {
				owners[id].push_back(dllName + " / " + conceptKey);
				if (dll.tested) {
					const string problem = IdProblem(id);
					run.Check(problem.empty(), "mod id is well-formed: '" + id + "' (" + dllName + " / " + conceptKey + ")", problem);
				}
			}
		}
		set<string> knownIds;
		for (const auto& [id, list] : owners) {
			knownIds.insert(id);
			if (list.size() == 1) continue;
			string joined;
			for (const string& owner : list) joined += (joined.empty() ? "" : "; ") + owner;
			run.Check(false, "mod id is globally unique: '" + id + "'", "provided " + to_string(list.size()) + " times: " + joined);
		}
		run.Metric("mod ids loaded", static_cast<double>(knownIds.size()), "");

		// ---- 每个被测dll自己的检查 ----
		for (const LoadedDll& dll : dlls) {
			if (!dll.tested) continue;
			const string dllName = fs::path(dll.path).filename().string();
			run.Check(dll.exportsBindRandomHost, "dll exports BindRandomHost: " + dllName,
				"without it GetRandom inside this dll is not controlled by the seed (see Dependence/common/random.h)");
			run.Check(!dll.ids.empty(), "dll provides at least one mod: " + dllName);
			set<string> ownIds;
			for (const auto& [conceptKey, id] : dll.ids) ownIds.insert(id);
			for (const auto& [modId, dependencyIds] : dll.dependencies) {
				run.Check(ownIds.count(modId) > 0, "GetModDependencies entry belongs to this dll: '" + modId + "' (" + dllName + ")",
					"the dll declares dependencies for a mod id it does not provide");
				for (const string& dependencyId : dependencyIds) {
					run.Check(knownIds.count(dependencyId) > 0, "dependency is available: '" + modId + "' -> '" + dependencyId + "'",
						"no loaded dll provides it; pass the dll that does with --with");
				}
			}
		}

		// ---- 依赖不能成环 ----
		// 配置界面把"被某个已启用的mod依赖"的mod锁成不能取消。成环的几个mod一旦都启用就互相锁死，哪个都关不掉。
		// 图用加载进来的全部dll的声明来建。
		{
			map<string, set<string>> graph;
			for (const LoadedDll& dll : dlls) {
				for (const auto& [modId, dependencyIds] : dll.dependencies) graph[modId].insert(dependencyIds.begin(), dependencyIds.end());
			}
			// 从start出发按依赖方向广搜，回到start就是环；返回"start -> ... -> start"，没有环返回空。
			auto findCycle = [&graph](const string& start) {
				map<string, string> parent;
				vector<string> queue = { start };
				for (size_t head = 0; head < queue.size(); head++) {
					const string current = queue[head];
					auto it = graph.find(current);
					if (it == graph.end()) continue;
					for (const string& next : it->second) {
						if (next == start) {
							vector<string> path = { start };
							for (string node = current; node != start; node = parent[node]) path.insert(path.begin() + 1, node);
							string joined = start;
							for (size_t i = 1; i < path.size(); i++) joined += " -> " + path[i];
							return joined + " -> " + start;
						}
						if (parent.count(next)) continue;
						parent[next] = current;
						queue.push_back(next);
					}
				}
				return string();
			};
			for (const LoadedDll& dll : dlls) {
				if (!dll.tested) continue;
				string cycles;
				for (const auto& [modId, dependencyIds] : dll.dependencies) {
					const string cycle = findCycle(modId);
					if (!cycle.empty()) cycles += (cycles.empty() ? "" : "; ") + cycle;
				}
				run.Check(cycles.empty(), "GetModDependencies has no cycle: " + fs::path(dll.path).filename().string(),
					cycles + " (mods in a cycle lock each other in the config screen and cannot be disabled)");
			}
		}

		// ---- 注册进ModTest自己的Factory ----
		// 每个concept：已发现的id全部视为启用，参数取config里的(没有就是空串)。
		for (const ModConceptDescriptor& descriptor : descriptors) {
			const ModConceptOps* ops = host.FindOps(descriptor.conceptKey);
			if (!ops) continue;
			const string jsonKey = ModConceptJsonKey(descriptor.conceptKey);
			unordered_map<string, string> args;
			for (const LoadedDll& dll : dlls) {
				for (const auto& [conceptKey, id] : dll.ids) {
					if (conceptKey != descriptor.conceptKey) continue;
					auto configured = configuredArgs.find({ jsonKey, id });
					// "--seed N"是给宿主的(游戏里由Registry剥掉)，不传给mod的构造函数。
					bool hasSeed = false;
					uint64_t seed = 0;
					args[id] = configured == configuredArgs.end() ? string() : RandomRegistry::ExtractSeed(configured->second, hasSeed, seed);
				}
			}
			ops->setArgs(args);
			for (const LoadedDll& dll : dlls) {
				ops->registerDll(reinterpret_cast<void*>(GetProcAddress(dll.handle, descriptor.registerModSymbol)),
					reinterpret_cast<void*>(GetProcAddress(dll.handle, descriptor.finishModSymbol)));
			}
		}

		// ---- 逐个mod ----
		map<string, int> perConcept;
		int referencesSeen = 0; // 各mod引用到的别的mod(去重后)的总数，用来确认引用检查确实看到了东西
		for (const LoadedDll& dll : dlls) {
			if (!dll.tested) continue;
			const bool isTemplate = IsEmptyTemplateDll(dll.path);
			for (const auto& [conceptKey, id] : dll.ids) {
				const ModConceptOps* ops = host.FindOps(conceptKey);
				if (!ops) continue;
				if (owners[id].size() > 1 || !IdProblem(id).empty()) continue; // 上面已经报过，重名的mod没法单独测

				run.Check(ops->isRegistered(id), "mod registers in its Factory: " + id,
					"GetMod" + conceptKey + " lists it but RegisterMod" + conceptKey + " did not register it");

				// 每个mod一条固定种子的随机流。
				RandomRegistry::SetGlobalSeed(true, options.seed);
				RandomRegistry::SetModSeed(id, true, Fnv1a(id.data(), id.size(), options.seed));

				ModCheckContext context;
				context.modId = id;
				context.isTemplate = isTemplate;
				context.isKnownId = [&knownIds](const string& other) { return knownIds.count(other) > 0; };
				ModOutcome outcome;
				ops->run(context, outcome);
				run.Check(outcome.created, "mod creates: " + id, "Create returned null although the mod is registered");
				if (!outcome.created) continue;
				perConcept[conceptKey]++;
				run.Check(outcome.passed, "UnitTest passes: " + id, outcome.error);
				{
					string joined;
					for (const string& problem : outcome.problems) joined += (joined.empty() ? "" : "; ") + problem;
					run.Check(outcome.problems.empty(), "concept field validation: " + id, joined);
				}
				{
					// 字段里引用到的别的mod必须存在(由加载进来的某个dll提供)。依赖声明不在这里查，见mod_checks.cpp"引用收集"。
					string joined;
					for (const string& problem : outcome.referenceProblems) joined += (joined.empty() ? "" : "; ") + problem;
					run.Check(outcome.referenceProblems.empty(), "referenced mods are provided by a loaded dll: " + id, joined);
					set<string> referenced;
					for (const auto& [what, referencedId] : outcome.references) if (referencedId != id) referenced.insert(referencedId);
					referencesSeen += static_cast<int>(referenced.size());
				}
				run.Check(outcome.unscoped == 0, "UnitTest has no unscoped GetRandom: " + id,
					to_string(outcome.unscoped) + " draws outside any RandomScope");
				// 占位模板名字固定，不受"GetName全局唯一"约定约束。
				if (!isTemplate) run.Check(outcome.nameUnique, "two instances get different GetName(): " + id);
			}
		}
		for (const auto& [conceptKey, count] : perConcept) run.Metric("mods tested: " + conceptKey, count, "");
		run.Metric("mod references checked", referencesSeen, "");

		const int failures = run.Failures(), passes = run.Passes();
		printf("\n==== ModTest summary ====\npassed %d, failed %d\n", passes, failures);
		if (!options.outDir.empty()) {
			if (run.Write(options.outDir)) printf("reports in %s\n", options.outDir.c_str());
			else printf("could not write reports to %s\n", options.outDir.c_str());
		}
		// 不FreeLibrary：进程马上退出，mod的static析构顺序由系统处理。
		return failures == 0 ? 0 : 1;
	}
}

int main(int argc, char** argv) {
	SetConsoleOutputCP(CP_UTF8);
	Options options;
	if (!ParseOptions(argc, argv, options)) {
		PrintUsage();
		return 2;
	}
	string repro = "ModTest";
	for (int i = 1; i < argc; i++) repro += string(" ") + argv[i];
	try {
		return Run(options, repro);
	}
	catch (const exception& e) {
		fprintf(stderr, "[ModTest] exception: %s\n", e.what());
		return 1;
	}
}
