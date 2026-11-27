#include "suites.h"

#include "child_world.h"
#include "process_util.h"
#include "test_framework.h"
#include "test_options.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>


using namespace std;

namespace {
	vector<string> Split(const string& text, char sep) {
		vector<string> parts;
		stringstream ss(text);
		string part;
		while (getline(ss, part, sep)) if (!part.empty()) parts.push_back(part);
		return parts;
	}

	// exe在 <Forever_UE>/x64/<Config>/CoreTest.exe：源码根 = <Forever_UE>/Source，mod根 = <Forever_UE>/../Forever_Mod。
	void ResolveRoots(TestOptions& options) {
		filesystem::path exeDir = filesystem::path(SelfPath()).parent_path();
		filesystem::path projectRoot = exeDir.parent_path().parent_path();
		options.sourceRoot = (projectRoot / "Source").string();
		options.modRoot = (projectRoot.parent_path() / "Forever_Mod").string();
	}

	bool ParseArgs(int argc, char** argv, TestOptions& options) {
		for (int i = 1; i < argc; i++) {
			const string arg = argv[i];
			auto next = [&]() -> string { return i + 1 < argc ? argv[++i] : ""; };
			if (arg == "--suite") options.suites = Split(next(), ',');
			else if (arg == "--seed") { options.childSeed = strtoull(next().c_str(), nullptr, 10); options.seeds = { options.childSeed }; }
			else if (arg == "--seeds") {
				const int count = atoi(next().c_str());
				options.seeds.clear();
				for (int k = 0; k < count; k++) options.seeds.push_back(11 + k);
			}
			else if (arg == "--size") options.mapSize = atoi(next().c_str());
			else if (arg == "--months") options.months = atoi(next().c_str());
			else if (arg == "--out") options.outDir = next();
			else if (arg == "--quick") options.quick = true;
			else if (arg == "--full") options.quick = false;
			else if (arg == "--child") options.childKind = next();
			else if (arg == "--child-out") options.childOut = next();
			else if (arg == "--child-suite") options.childSuite = next();
			else if (arg.rfind("--", 0) == 0) { fprintf(stderr, "unknown option %s\n", arg.c_str()); return false; }
			else options.configPath = arg;
		}
		if (options.configPath.empty()) {
			fprintf(stderr, "usage: CoreTest <config.json> [--suite random,mods,lint,world,econ,save,perform,route,green,presets,plateau,polit,transit,terrain,unload,fault|all] [--seed S|--seeds N] [--size N] [--months N] [--quick|--full] [--out dir]\n");
			return false;
		}
		return true;
	}
}

static int RealMain(int argc, char** argv) {
	TestOptions options;
	if (!ParseArgs(argc, argv, options)) return 2;
	ResolveRoots(options);
	options.selfCommand = "CoreTest.exe " + options.configPath;

	if (options.childKind == "world") return RunWorldChild(options);
	if (options.childKind == "suite") {
		SuiteBody body = FindSuiteBody(options.childSuite);
		if (!body) { fprintf(stderr, "unknown child suite %s\n", options.childSuite.c_str()); return 2; }
		return RunSuiteChild(options, body);
	}
	if (!options.childKind.empty()) { fprintf(stderr, "unknown child kind %s\n", options.childKind.c_str()); return 2; }

	struct Suite {
		const char* name;
		function<void(const TestOptions&)> run;
	};
	const vector<Suite> suites = {
		{ "random", RunRandomSuite },
		{ "mods", RunModsSuite },
		{ "lint", RunLintSuite },
		{ "world", RunWorldSuite },
		{ "econ", RunEconSuite },
		{ "demography", RunDemographySuite },
		{ "save", RunSaveSuite },
		{ "perform", RunPerfSuite },
		{ "route", RunRouteSuite },
		{ "green", RunGreenSuite },
		{ "presets", RunPresetsSuite },
		{ "plateau", RunPlateauSuite },
		{ "polit", RunPolitSuite },
		{ "transit", RunTransitSuite },
		{ "terrain", RunTerrainSuite },
		{ "unload", RunUnloadSuite },
		{ "fault", RunFaultSuite },
	};

	int totalFailures = 0, totalPasses = 0;
	vector<string> summary;
	for (const Suite& suite : suites) {
		if (!options.WantSuite(suite.name)) continue;
		try {
			suite.run(options);
		}
		catch (const exception& e) {
			TestRun::Get().Check(false, string("suite ") + suite.name + " threw", e.what());
		}
		TestRun::Get().Write(options.outDir);
		const int failed = TestRun::Get().Failures(), passed = TestRun::Get().Passes();
		totalFailures += failed;
		totalPasses += passed;
		char line[160];
		snprintf(line, sizeof(line), "%-8s passed %4d  failed %4d", suite.name, passed, failed);
		summary.push_back(line);
	}
	printf("\n==== CoreTest summary ====\n");
	for (const string& line : summary) printf("%s\n", line.c_str());
	printf("total: passed %d, failed %d  (reports in %s)\n", totalPasses, totalFailures, options.outDir.c_str());
	return totalFailures > 0 ? 1 : 0;
}

int main(int argc, char** argv) {
	// Core和mod的debugf默认写标准错误，文本是UTF-8(工程开了/utf-8)，控制台不设成UTF-8中文会乱码。
	SetConsoleOutputCP(CP_UTF8);
	InstallCrashHandler();
	try {
		return RealMain(argc, argv);
	}
	catch (const exception& e) {
		fprintf(stderr, "[CoreTest] exception: %s\n", e.what());
		return 2;
	}
}
