#include "suites.h"

#include "test_framework.h"

#include "common/config.h"
#include "common/registry.h"
#include "common/save.h"
#include "industry/industry.h"
#include "map/map.h"
#include "player/player.h"
#include "polit/polit.h"
#include "populace/populace.h"
#include "society/society.h"
#include "story/script.h"
#include "story/story.h"
#include "traffic/traffic.h"

#include <windows.h>

#include <filesystem>
#include <map>
#include <string>
#include <vector>


using namespace std;

// 套件unload：一局结束后Core有没有泄漏、mod dll能不能卸干净、卸载后重新加载再生成的世界和第一次是否一样。
// 游戏里对应AForeverFrameworkActor::EndPlay(销毁各域 -> Script::ClearCache -> Registry::UnloadMods)和下一局的
// RefreshMods，见Core/common/registry.md"卸载"。在子进程里跑(世界已经生成好了)，连做3轮"销毁-卸载-重载-生成"。
namespace {
	// 和AForeverFrameworkActor::EndPlay同一个销毁顺序。
	void DestroyWorld(TestWorld& world) {
		delete world.player; world.player = nullptr;
		delete world.traffic; world.traffic = nullptr;
		delete world.industry; world.industry = nullptr;
		delete world.polit; world.polit = nullptr; // 先于society：polit持有被society绑定的政策值
		delete world.society; world.society = nullptr;
		delete world.story; world.story = nullptr;
		delete world.populace; world.populace = nullptr;
		delete world.map; world.map = nullptr;
	}

	string Join(const vector<string>& items) {
		string joined;
		for (const string& item : items) joined += (joined.empty() ? "" : "; ") + item;
		return joined;
	}

	// 销毁世界 -> 查Core有没有泄漏 -> 卸载 -> 查dll确实离开了进程、注册项清干净了。
	void TeardownAndUnload(TestWorld& world, const string& label) {
		TestRun& run = TestRun::Get();
		const vector<string> loaded = Registry::Get().GetLoadedDllPaths();
		run.Check(!loaded.empty(), "mod dlls are loaded while a world exists" + label);

		DestroyWorld(world);

		// ---- Core层面的泄漏：一局结束后不该再有任何Script对象、任何mod实例 ----
		// Script::ClearCache在还有Script活着时拒绝清缓存并返回false。
		run.Check(Script::ClearCache(), "no Script object is alive after the world is destroyed" + label,
			"Script::ClearCache refused: some Script was not destroyed with its owner (leak)");
		vector<string> leaks;
		for (const auto& [id, count] : Registry::Get().GetLiveInstanceCounts()) leaks.push_back(id + " x" + to_string(count));
		run.Check(leaks.empty(), "no mod instance is alive after the world is destroyed" + label,
			"created through a Factory but never destroyed: " + Join(leaks));
		run.Metric("leaked mod instance ids" + label, static_cast<double>(leaks.size()), "");

		// ---- 卸载 ----
		const Registry::UnloadReport report = Registry::Get().UnloadMods();
		run.Check(report.keptDlls.empty(), "every mod dll is unloaded" + label, "kept: " + Join(report.keptDlls));
		run.Check(Registry::Get().GetLoadedDllPaths().empty(), "Registry holds no mod dll after unload" + label);
		vector<string> stillMapped;
		for (const string& path : loaded) {
			if (GetModuleHandleA(path.c_str())) stillMapped.push_back(filesystem::path(path).filename().string());
		}
		run.Check(stillMapped.empty(), "unloaded mod dlls are no longer in the process" + label, Join(stillMapped));
		vector<string> stillRegistered;
		for (const Config::ModEntry& entry : Config::GetModEnables()) {
			if (Registry::Get().CheckModRegistered(entry.modId)) stillRegistered.push_back(entry.modId);
		}
		run.Check(stillRegistered.empty(), "no mod id stays registered after unload" + label, Join(stillRegistered));
		run.Metric("mod dlls unloaded" + label, static_cast<double>(report.unloadedDlls.size()), "");
	}

	// 保护：卸载时还有mod实例活着(=泄漏)，它所在的dll必须留下(实例的虚函数表和销毁函数都在dll里)，别的dll照常卸载。
	// 这里故意留一个取名mod的实例不销毁，模拟泄漏。
	void CheckLeakGuard(const TestOptions& options, const string& workDir) {
		TestRun& run = TestRun::Get();
		string error;
		if (!LoadTestConfig(options.configPath, options.childSeed, workDir, error)) {
			run.Check(false, "reload config (leak guard)", error);
			return;
		}
		const size_t loadedCount = Registry::Get().GetLoadedDllPaths().size();
		const string id = Config::GetUniqueConceptMod("Names");
		NameFactory& factory = Registry::Get().GetNameFactory();
		NameMod* leaked = factory.CreateName(id);
		if (!leaked) {
			run.Check(false, "leak guard: create a name mod instance", "CreateName(" + id + ") returned null");
			return;
		}

		Registry::UnloadReport report = Registry::Get().UnloadMods();
		run.Check(report.liveInstances.size() == 1 && report.liveInstances[0].first == id && report.liveInstances[0].second == 1,
			"leak guard: the leaked instance is reported", "expected " + id + " x1");
		run.Check(report.keptDlls.size() == 1, "leak guard: exactly the dll of the leaked instance is kept", Join(report.keptDlls));
		run.Check(report.unloadedDlls.size() + 1 == loadedCount, "leak guard: every other dll is still unloaded",
			to_string(report.unloadedDlls.size()) + " of " + to_string(loadedCount) + " unloaded");
		const bool keptMapped = !report.keptDlls.empty() && GetModuleHandleA(report.keptDlls[0].c_str()) != nullptr;
		run.Check(keptMapped, "leak guard: the kept dll is still in the process");
		run.Check(factory.CheckRegistered(id), "leak guard: the kept dll keeps its registration (needed to destroy the instance)");

		// 实例还能正常用、正常销毁(dll要是被卸了这里就崩了)；销毁之后再卸一次，全部走干净。
		const char* name = leaked->GetName();
		run.Check(name != nullptr, "leak guard: the leaked instance is still usable");
		factory.DestroyName(leaked);
		report = Registry::Get().UnloadMods();
		run.Check(report.keptDlls.empty() && report.liveInstances.empty() && Registry::Get().GetLoadedDllPaths().empty(),
			"leak guard: after destroying the instance everything unloads", "kept: " + Join(report.keptDlls));
	}
}

void UnloadSuiteBody(TestWorld& world, const TestOptions& options) {
	TestRun& run = TestRun::Get();
	const map<string, string> first = ComputeFingerprint(world);
	const int mapSize = world.mapSize;
	const string workDir = filesystem::path(options.childOut).parent_path().string();

	constexpr int kCycles = 3;
	double afterBuild[kCycles] = {}, afterUnload[kCycles] = {};
	afterBuild[0] = ReadProcessMemory().privateMB;
	TeardownAndUnload(world, " (cycle 1)");
	afterUnload[0] = ReadProcessMemory().privateMB;

	for (int cycle = 2; cycle <= kCycles; cycle++) {
		const string label = " (cycle " + to_string(cycle) + ")";
		// 重新加载：和游戏里开下一局一样，读同一份带种子的config并RefreshMods(dll这时全部是重新LoadLibrary进来的)，
		// 新开局复位命名计数器。
		string error;
		if (!LoadTestConfig(options.configPath, options.childSeed, workDir, error)) {
			run.Check(false, "reload config" + label, error);
			return;
		}
		SaveGame::ResetInstanceCounters();
		TestWorld again;
		if (!BuildWorld(again, mapSize, error)) {
			run.Check(false, "world generation after unload/reload" + label, error);
			return;
		}
		run.Check(true, "world generation after unload/reload" + label);
		// 同一个种子：卸载重载不应该改变生成结果(mod和Core都不能有跨局残留的状态影响生成)。
		const string difference = DiffFingerprint(first, ComputeFingerprint(again));
		run.Check(difference.empty(), "world rebuilt after unload/reload matches the first one" + label, difference);
		afterBuild[cycle - 1] = ReadProcessMemory().privateMB;
		TeardownAndUnload(again, label);
		afterUnload[cycle - 1] = ReadProcessMemory().privateMB;
	}

	// ---- 内存：每轮"生成-销毁-卸载"之后的私有内存不应该一轮比一轮高 ----
	// 堆不一定把内存还给系统，所以不比"销毁后回到生成前"，只比相邻两轮销毁之后的水位：整个域没释放的话每轮涨一个世界的量。
	for (int i = 0; i < kCycles; i++) {
		run.Metric("private memory after build, cycle " + to_string(i + 1), afterBuild[i], "MB");
		run.Metric("private memory after unload, cycle " + to_string(i + 1), afterUnload[i], "MB");
	}
	const double footprint = afterBuild[kCycles - 1] - afterUnload[kCycles - 2]; // 最后一轮：生成这个世界涨了多少
	const double growth = afterUnload[kCycles - 1] - afterUnload[kCycles - 2];   // 最后两轮销毁后的水位差
	run.Metric("world footprint", footprint, "MB");
	run.Metric("memory growth per build/destroy cycle", growth, "MB");
	run.Check(footprint <= 0 || growth < footprint * 0.2, "memory does not keep growing across build/destroy cycles",
		to_string(growth) + " MB per cycle, a world takes " + to_string(footprint) + " MB");

	CheckLeakGuard(options, workDir);
}

void RunUnloadSuite(const TestOptions& options) {
	TestRun::Get().Begin("unload", 0, options.selfCommand + " --suite unload");
	RunSuiteInChildren(options, "unload", options.EffectiveMapSize());
}
