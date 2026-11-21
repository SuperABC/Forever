#pragma once

#include <string>
#include <vector>


struct TestCheck {
	std::string name;
	bool passed = true;
	std::string detail;
};

struct TestMetric {
	std::string name;
	double value = 0;
	std::string unit;
};

// 当前进程的测试记录器(单例)：套件通过Check/Metric/Note写入，结束时Write落盘。
class TestRun {
public:
	static TestRun& Get();

	// 开始一个套件：suite名、种子(0=无)和复现命令会写进报告。
	void Begin(const std::string& suite, unsigned long long seed, const std::string& repro);
	void Check(bool passed, const std::string& name, const std::string& detail = "");
	void Metric(const std::string& name, double value, const std::string& unit = "");
	void Note(const std::string& line);

	int Failures() const;
	int Passes() const;
	const std::vector<TestCheck>& GetChecks() const { return checks; }
	const std::vector<TestMetric>& GetMetrics() const { return metrics; }

	// 写 <dir>/<suite>.json 和 <suite>.txt，返回是否成功。
	bool Write(const std::string& dir) const;

	// 清空(同一进程里连续跑多个套件时每个套件之前调用)。
	void Reset();

	// 子进程把自己记下的check/metric/note导出成文本，父进程导入并给名字加上标签(如"[seed 11]")。
	bool Export(const std::string& path) const;
	void Import(const std::string& path, const std::string& tag);

private:
	std::string suite;
	unsigned long long seed = 0;
	std::string repro;
	std::vector<TestCheck> checks;
	std::vector<TestMetric> metrics;
	std::vector<std::string> notes;
};

// mod UnitTest的print回调：裸函数指针(跨DLL安全)，把一行输出记进当前套件的Note并打到控制台。
void TestPrintLine(const char* line);

// 进程内存(MB)：privateMB=提交的私有内存(堆+栈，最能反映"这个世界占了多少")，workingSetMB=物理驻留，peakWorkingSetMB=历史峰值。
struct ProcessMemory { double privateMB = 0, workingSetMB = 0, peakWorkingSetMB = 0; };
ProcessMemory ReadProcessMemory();
// 记两条Metric："memory <label> private"(MB)和"memory <label> delta"(和上一次RecordMemory相比的增量，第一次为0)，并随后更新基线。
// 所有套件在"阶段/规模变化/大对象建好"之后调用，内存增长和泄漏就能在报告里按标签对比。
void RecordMemory(const std::string& label);

std::string JsonEscape(const std::string& text);
// FNV-1a 64位，指纹/哈希统一用它。
unsigned long long Fnv1a(const void* data, size_t size, unsigned long long seed = 1469598103934665603ULL);
std::string HexString(unsigned long long value);

#define CHECK(cond, name) TestRun::Get().Check((cond), (name), #cond)
