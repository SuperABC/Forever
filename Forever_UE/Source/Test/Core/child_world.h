#pragma once

#include "test_options.h"
#include "world_build.h"

#include <map>
#include <string>
#include <vector>


// 子进程一次运行的结果(世界指纹+各阶段耗时/unscoped)。
struct ChildWorldResult {
	int exitCode = 0;
	std::string error;
	std::map<std::string, std::string> fingerprint;
	std::vector<WorldStage> stages;
	std::string logPath;
};

// 子进程入口：--child world。读config(带种子)、生成世界、把指纹写到options.childOut，返回进程退出码。
int RunWorldChild(const TestOptions& options);

// 父进程：启动一个子进程跑seed/size指定的世界，解析结果。
ChildWorldResult RunWorldInChild(const TestOptions& options, unsigned long long seed, int mapSize, const std::string& tag);

// 套件主体：拿到已生成好的世界，把结果记进TestRun。
typedef void (*SuiteBody)(TestWorld& world, const TestOptions& options);

// 子进程入口：--child suite --child-suite <name>。生成世界后调用body，把TestRun导出到options.childOut。
int RunSuiteChild(const TestOptions& options, SuiteBody body);

// 父进程：对每个种子启动子进程跑suiteName，把子进程的check/metric导入当前TestRun(名字带"[seed N]"标签)。
// extraArgs追加给子进程(如--months)。
void RunSuiteInChildren(const TestOptions& options, const std::string& suiteName, int mapSize, const std::vector<std::string>& extraArgs = {});

// 两份指纹逐项比较，返回第一处差异的描述(完全一致返回空串)。
std::string DiffFingerprint(const std::map<std::string, std::string>& a, const std::map<std::string, std::string>& b);
