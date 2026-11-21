#include "child_world.h"

#include "process_util.h"
#include "test_framework.h"

#include "common/random.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>


using namespace std;

int RunWorldChild(const TestOptions& options) {
	string error;
	ofstream out(options.childOut, ios::binary);
	if (!out) return 4;
	if (!LoadTestConfig(options.configPath, options.childSeed, filesystem::path(options.childOut).parent_path().string(), error)) {
		out << "error\t" << error << "\n";
		return 3;
	}
	TestWorld world;
	const bool ok = BuildWorld(world, options.EffectiveMapSize(), error);
	for (const WorldStage& stage : world.stages) out << "stage\t" << stage.name << "\t" << stage.seconds << "\t" << stage.unscopedDelta << "\t" << stage.signature << "\n";
	if (!ok) {
		out << "error\t" << error << "\n";
		return 3;
	}
	for (const auto& [key, value] : ComputeFingerprint(world)) out << "fp\t" << key << "\t" << value << "\n";
	out << "unscoped\t" << RandomRegistry::UnscopedDraws() << "\n";
	return 0;
}

int RunSuiteChild(const TestOptions& options, SuiteBody body) {
	string error;
	TestRun& run = TestRun::Get();
	run.Begin(options.childSuite, options.childSeed, options.selfCommand + " --suite " + options.childSuite +
		" --seed " + to_string(options.childSeed) + " --size " + to_string(options.EffectiveMapSize()));
	if (!LoadTestConfig(options.configPath, options.childSeed, filesystem::path(options.childOut).parent_path().string(), error)) {
		run.Check(false, "load config", error);
		run.Export(options.childOut);
		return 3;
	}
	TestWorld world;
	if (!BuildWorld(world, options.EffectiveMapSize(), error)) {
		run.Check(false, "world generation", error);
		run.Export(options.childOut);
		return 3;
	}
	double previousMB = 0;
	for (const WorldStage& stage : world.stages) {
		run.Metric("stage." + stage.name, stage.seconds, "s");
		run.Metric("stage." + stage.name + " memory", stage.privateMB - previousMB, "MB");
		previousMB = stage.privateMB;
		run.Check(stage.unscopedDelta == 0, "no unscoped GetRandom in stage " + stage.name, to_string(stage.unscopedDelta) + " draws");
	}
	RecordMemory("after world generation");
	body(world, options);
	RecordMemory("after suite body");
	run.Metric("memory peak working set", ReadProcessMemory().peakWorkingSetMB, "MB");
	run.Export(options.childOut);
	return 0;
}

void RunSuiteInChildren(const TestOptions& options, const string& suiteName, int mapSize, const vector<string>& extraArgs) {
	TestRun& run = TestRun::Get();
	error_code ec;
	const filesystem::path dir = filesystem::path(options.outDir) / "child";
	filesystem::create_directories(dir, ec);
	for (unsigned long long seed : options.EffectiveSeeds()) {
		const string base = (dir / (suiteName + "_s" + to_string(seed))).string();
		const string outFile = base + ".chk";
		const string logPath = base + ".log";
		vector<string> args = { options.configPath, "--child", "suite", "--child-suite", suiteName, "--child-out", outFile,
			"--seed", to_string(seed), "--size", to_string(mapSize) };
		for (const string& extra : extraArgs) args.push_back(extra);
		const string tag = "[seed " + to_string(seed) + "]";
		const int exitCode = RunSelf(args, logPath, 3600);
		run.Import(outFile, tag);
		const string repro = options.selfCommand + " --suite " + suiteName + " --seed " + to_string(seed) + " --size " + to_string(mapSize);
		run.Check(exitCode == 0, suiteName + " child process finished " + tag,
			(exitCode == -2 ? string("timeout") : "exit code " + to_string(exitCode)) + " (log " + logPath + ")  repro: " + repro);
	}
}

ChildWorldResult RunWorldInChild(const TestOptions& options, unsigned long long seed, int mapSize, const string& tag) {
	ChildWorldResult result;
	error_code ec;
	const filesystem::path dir = filesystem::path(options.outDir) / "child";
	filesystem::create_directories(dir, ec);
	const string base = (dir / (tag + "_s" + to_string(seed))).string();
	const string outFile = base + ".fp";
	result.logPath = base + ".log";

	vector<string> args = { options.configPath, "--child", "world", "--child-out", outFile,
		"--seed", to_string(seed), "--size", to_string(mapSize) };
	result.exitCode = RunSelf(args, result.logPath, 1800);

	ifstream in(outFile);
	string line;
	while (getline(in, line)) {
		vector<string> parts;
		stringstream ss(line);
		string part;
		while (getline(ss, part, '\t')) parts.push_back(part);
		if (parts.empty()) continue;
		if (parts[0] == "fp" && parts.size() >= 3) result.fingerprint[parts[1]] = parts[2];
		else if (parts[0] == "stage" && parts.size() >= 4) {
			WorldStage stage;
			stage.name = parts[1];
			stage.seconds = atof(parts[2].c_str());
			stage.unscopedDelta = strtoull(parts[3].c_str(), nullptr, 10);
			if (parts.size() >= 5) stage.signature = parts[4];
			result.stages.push_back(stage);
		}
		else if (parts[0] == "error" && parts.size() >= 2) result.error = parts[1];
	}
	if (result.exitCode != 0 && result.error.empty()) {
		result.error = result.exitCode == -2 ? "timeout" : "child exit code " + to_string(result.exitCode) + " (see " + result.logPath + ")";
	}
	return result;
}

string DiffFingerprint(const map<string, string>& a, const map<string, string>& b) {
	string out;
	int shown = 0, total = 0;
	auto add = [&](const string& text) {
		total++;
		if (shown < 12) { out += (shown ? "; " : "") + text; shown++; }
	};
	for (const auto& [key, value] : a) {
		auto it = b.find(key);
		if (it == b.end()) add(key + " missing on the other side");
		else if (it->second != value) add(key + ": " + value + " vs " + it->second);
	}
	for (const auto& [key, value] : b) {
		if (!a.count(key)) add(key + " missing on this side");
	}
	if (total > shown) out += "; ... (" + to_string(total) + " differences)";
	return out;
}
