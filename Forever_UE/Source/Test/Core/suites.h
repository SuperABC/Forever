#pragma once

#include "child_world.h"
#include "test_options.h"


// 每个套件函数里自己TestRun::Get().Begin(...)，main负责Write和汇总失败数。
void RunRandomSuite(const TestOptions& options);
void RunModsSuite(const TestOptions& options);
void RunWorldSuite(const TestOptions& options);
void RunEconSuite(const TestOptions& options);
void RunSaveSuite(const TestOptions& options);
void RunLintSuite(const TestOptions& options);
void RunPerfSuite(const TestOptions& options);
void RunRouteSuite(const TestOptions& options);
void RunGreenSuite(const TestOptions& options);
void RunPresetsSuite(const TestOptions& options);
void RunPlateauSuite(const TestOptions& options);
void RunPolitSuite(const TestOptions& options);
void RunTransitSuite(const TestOptions& options);
void RunTerrainSuite(const TestOptions& options);
void RunUnloadSuite(const TestOptions& options);
void RunFaultSuite(const TestOptions& options);
void RunDemographySuite(const TestOptions& options);

// 子进程里的套件主体(世界已生成)。
void WorldSuiteBody(TestWorld& world, const TestOptions& options);
void EconSuiteBody(TestWorld& world, const TestOptions& options);
void SaveSuiteBody(TestWorld& world, const TestOptions& options);
void PerfSuiteBody(TestWorld& world, const TestOptions& options);
void RouteSuiteBody(TestWorld& world, const TestOptions& options);
void GreenSuiteBody(TestWorld& world, const TestOptions& options);
void PresetsSuiteBody(TestWorld& world, const TestOptions& options);
void PlateauSuiteBody(TestWorld& world, const TestOptions& options);
void PolitSuiteBody(TestWorld& world, const TestOptions& options);
void TransitSuiteBody(TestWorld& world, const TestOptions& options);
void TransitTwiceSuiteBody(TestWorld& world, const TestOptions& options);
void TransitBusSuiteBody(TestWorld& world, const TestOptions& options);
void UnloadSuiteBody(TestWorld& world, const TestOptions& options);
void FaultSuiteBody(TestWorld& world, const TestOptions& options);
void DemographySuiteBody(TestWorld& world, const TestOptions& options);

// 在子进程里(世界已生成)执行的套件主体；按套件名查找，没有返回nullptr。
SuiteBody FindSuiteBody(const std::string& name);
