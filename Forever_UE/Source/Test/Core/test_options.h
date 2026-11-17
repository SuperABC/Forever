#pragma once

#include <string>
#include <vector>


struct TestOptions {
	std::string configPath;
	std::string outDir = "Saved/TestReports/core";
	std::vector<std::string> suites;          // 空=all
	std::vector<unsigned long long> seeds;    // 空=按quick/full取默认
	int mapSize = 0;                          // 0=按quick/full取默认
	int months = 0;
	bool quick = true;

	// 子进程模式：--child <kind> --child-out <file>
	std::string childKind;
	std::string childOut;
	std::string childSuite;  // --child suite 时要在子进程里跑的套件名(world/econ/save/perf)
	unsigned long long childSeed = 0;

	std::string sourceRoot;  // Forever_UE/Source
	std::string modRoot;     // Forever_Mod
	std::string selfCommand; // 复现命令前缀(CoreTest.exe config)

	int EffectiveMapSize() const { return mapSize > 0 ? mapSize : (quick ? 256 : 1024); }
	int EffectiveMonths() const { return months > 0 ? months : (quick ? 3 : 24); }
	std::vector<unsigned long long> EffectiveSeeds() const {
		if (!seeds.empty()) return seeds;
		return quick ? std::vector<unsigned long long>{ 11, 12 } : std::vector<unsigned long long>{ 11, 12, 13, 14 };
	}
	bool WantSuite(const std::string& name) const {
		if (suites.empty()) return true;
		for (const std::string& s : suites) if (s == name || s == "all") return true;
		return false;
	}
};
