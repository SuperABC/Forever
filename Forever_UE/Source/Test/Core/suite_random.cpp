#include "suites.h"

#include "child_world.h"
#include "test_framework.h"

#include "common/config.h"
#include "common/random.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>


using namespace std;

namespace {
	// 允许直接用C++随机库的文件(路径子串)及原因。其余文件出现即视为绕过了GetRandom。
	struct SourceAllow {
		const char* pathPart;
		const char* reason;
	};
	const SourceAllow kRandomSourceAllow[] = {
		{ "Dependence/common/random.", "种子流实现本身" },
		{ "Dependence/common/utility.cpp", "GetRandom无作用域时的random_device回退" },
		{ "Basic/player/puzzle_common.h", "PuzzleRng：缺省种子取自GetRandom，显式seed时不受控是设计" },
		{ "Wxdj/Cpp/Wxdj/battle/", "战斗内部随机，random.md声明不受约束" },
	};

	string Slashes(string path) {
		replace(path.begin(), path.end(), '\\', '/');
		return path;
	}

	string StripComment(const string& line) {
		const size_t pos = line.find("//");
		string code = pos == string::npos ? line : line.substr(0, pos);
		return code;
	}

	// 1. 同种子两个子进程指纹完全一致，不同种子必须不同，各阶段没有漏包的随机。
	void CheckDeterminism(const TestOptions& options) {
		TestRun& run = TestRun::Get();
		const vector<unsigned long long> seeds = options.EffectiveSeeds();
		const int size = options.quick ? min(options.EffectiveMapSize(), 256) : options.EffectiveMapSize();
		const unsigned long long seedA = seeds[0];
		const unsigned long long seedB = seeds.size() > 1 ? seeds[1] : seeds[0] + 1;

		const ChildWorldResult first = RunWorldInChild(options, seedA, size, "random_a");
		const ChildWorldResult second = RunWorldInChild(options, seedA, size, "random_b");
		const ChildWorldResult other = RunWorldInChild(options, seedB, size, "random_c");
		const string repro = options.selfCommand + " --suite random --seed " + to_string(seedA) + " --size " + to_string(size);

		run.Check(first.exitCode == 0, "world generation child (seed A) succeeds", first.error);
		run.Check(second.exitCode == 0, "world generation child (seed A, again) succeeds", second.error);
		run.Check(other.exitCode == 0, "world generation child (seed B) succeeds", other.error);
		if (first.exitCode != 0 || second.exitCode != 0 || other.exitCode != 0) return;

		// 先逐阶段比签名，定位第一个分歧的阶段。
		string firstDivergent;
		for (size_t i = 0; i < first.stages.size() && i < second.stages.size(); i++) {
			if (first.stages[i].signature != second.stages[i].signature) { firstDivergent = first.stages[i].name; break; }
		}
		run.Check(firstDivergent.empty(), "same seed: every generation stage ends in an identical state", "first divergent stage: " + firstDivergent);
		const string diff = DiffFingerprint(first.fingerprint, second.fingerprint);
		run.Check(diff.empty(), "same seed gives an identical world across processes (all domains)", diff + "  repro: " + repro);
		const string otherDiff = DiffFingerprint(first.fingerprint, other.fingerprint);
		run.Check(!otherDiff.empty(), "different seeds give different worlds (the seed is effective)", "seed " + to_string(seedA) + " vs " + to_string(seedB));

		for (const WorldStage& stage : first.stages) {
			run.Check(stage.unscopedDelta == 0, "no unscoped GetRandom in stage " + stage.name,
				to_string(stage.unscopedDelta) + " draws outside any RandomScope  repro: " + repro);
			run.Metric("stage." + stage.name, stage.seconds, "s");
		}
	}

	// 2. 每个被加载的mod DLL都导出BindRandomHost(否则它的GetRandom退回random_device，且unscoped计数宿主看不到)。
	void CheckBindRandomHost() {
		TestRun& run = TestRun::Get();
		int dllCount = 0;
		for (const string& path : Config::GetMods()) {
			HMODULE module = GetModuleHandleA(path.c_str());
			if (!module) module = LoadLibraryA(path.c_str());
			if (!module) continue;
			dllCount++;
			run.Check(GetProcAddress(module, kBindRandomHostSymbol) != nullptr, "dll exports BindRandomHost: " + path,
				"add FOREVER_MOD_BIND_RANDOM to the dll entry file, otherwise its GetRandom ignores the seed");
		}
		run.Metric("mod dlls inspected", dllCount, "");
	}

	// 3. 源码里不允许出现绕过GetRandom的随机源。
	void CheckRandomSources(const TestOptions& options) {
		TestRun& run = TestRun::Get();
		const regex pattern(R"(random_device|\bmt19937(_64)?\b|minstd_rand|default_random_engine|\bsrand\s*\(|\brand\s*\(\s*\)|random_shuffle|\bstd::shuffle\b|[^A-Za-z_:]shuffle\s*\()");
		vector<filesystem::path> roots = {
			filesystem::path(options.sourceRoot) / "Dependence", filesystem::path(options.sourceRoot) / "Core",
			filesystem::path(options.sourceRoot) / "Basic", filesystem::path(options.modRoot) };
		int filesScanned = 0, hits = 0;
		set<string> reported;
		for (const filesystem::path& root : roots) {
			error_code ec;
			if (!filesystem::exists(root, ec)) continue;
			for (filesystem::recursive_directory_iterator it(root, filesystem::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
				if (ec) break;
				if (!it->is_regular_file()) continue;
				const string path = Slashes(it->path().string());
				if (path.find("/x64/") != string::npos || path.find("/3rdParty/") != string::npos || path.find("/Intermediate/") != string::npos) continue;
				const string ext = it->path().extension().string();
				if (ext != ".cpp" && ext != ".h" && ext != ".hpp") continue;
				bool allowed = false;
				for (const SourceAllow& allow : kRandomSourceAllow) if (path.find(allow.pathPart) != string::npos) allowed = true;
				if (allowed) continue;
				filesScanned++;
				ifstream in(it->path());
				string line;
				int number = 0;
				while (getline(in, line)) {
					number++;
					const string code = StripComment(line);
					if (regex_search(code, pattern)) {
						hits++;
						const string where = path + ":" + to_string(number);
						run.Check(false, "random source bypasses GetRandom: " + where, line);
					}
				}
			}
		}
		run.Metric("source files scanned for random lint", filesScanned, "");
		run.Check(hits == 0, "no random source outside the seeded wrapper", to_string(hits) + " hits");
	}

	// 4. Snapshot/Restore：恢复后各流继续产生与"不存档直接继续"相同的序列。
	void CheckSnapshotRestore() {
		TestRun& run = TestRun::Get();
		RandomRegistry::Reset();
		RandomRegistry::SetGlobalSeed(true, 777);
		RandomStream* map = RandomRegistry::GetDomainStream("core.map");
		RandomStream* mod = RandomRegistry::GetModStream("terrain_empty");
		for (int i = 0; i < 37; i++) { map->NextU64(); mod->NextU64(); }
		const vector<RandomRegistry::Entry> saved = RandomRegistry::Snapshot();

		vector<unsigned long long> direct;
		for (int i = 0; i < 64; i++) { direct.push_back(map->NextU64()); direct.push_back(mod->NextU64()); }

		RandomRegistry::Reset();
		RandomRegistry::SetGlobalSeed(true, 999); // 故意换种子：恢复必须覆盖它
		RandomRegistry::Restore(saved);
		map = RandomRegistry::GetDomainStream("core.map");
		mod = RandomRegistry::GetModStream("terrain_empty");
		vector<unsigned long long> resumed;
		for (int i = 0; i < 64; i++) { resumed.push_back(map->NextU64()); resumed.push_back(mod->NextU64()); }
		run.Check(direct == resumed, "Restore resumes every stream exactly where the snapshot was taken");

		RandomRegistry::Reset();
		RandomRegistry::SetGlobalSeed(true, 777);
		const unsigned long long firstA = RandomRegistry::GetDomainStream("core.map")->NextU64();
		RandomRegistry::Reset();
		RandomRegistry::SetGlobalSeed(true, 777);
		const unsigned long long firstB = RandomRegistry::GetDomainStream("core.map")->NextU64();
		run.Check(firstA == firstB, "Reset + same global seed reproduces the same stream");
		RandomRegistry::Reset();
		RandomRegistry::SetGlobalSeed(true, 778);
		const unsigned long long firstC = RandomRegistry::GetDomainStream("core.map")->NextU64();
		run.Check(firstA != firstC, "different global seeds give different streams");
		RandomRegistry::Reset();
	}
}

void RunRandomSuite(const TestOptions& options) {
	TestRun::Get().Begin("random", 0, options.selfCommand + " --suite random");
	string error;
	if (!LoadTestConfig(options.configPath, 0, options.outDir, error)) {
		TestRun::Get().Check(false, "load config", error);
		return;
	}
	CheckBindRandomHost();
	CheckRandomSources(options);
	CheckSnapshotRestore();
	CheckDeterminism(options);
}

SuiteBody FindSuiteBody(const std::string& name) {
	if (name == "world") return WorldSuiteBody;
	if (name == "econ") return EconSuiteBody;
	if (name == "save") return SaveSuiteBody;
	if (name == "perform") return PerfSuiteBody;
	return nullptr;
}
