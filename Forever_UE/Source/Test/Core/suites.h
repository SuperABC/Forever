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

// 子进程里的套件主体(世界已生成)。
void WorldSuiteBody(TestWorld& world, const TestOptions& options);
void EconSuiteBody(TestWorld& world, const TestOptions& options);
void SaveSuiteBody(TestWorld& world, const TestOptions& options);
void PerfSuiteBody(TestWorld& world, const TestOptions& options);
void RouteSuiteBody(TestWorld& world, const TestOptions& options);

// 在子进程里(世界已生成)执行的套件主体；按套件名查找，没有返回nullptr。
SuiteBody FindSuiteBody(const std::string& name);
