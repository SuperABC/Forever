#include "suites.h"

#include "test_framework.h"
#include "world_build.h"

#include "common/config.h"

#include <filesystem>
#include <string>

using namespace std;
namespace fs = std::filesystem;

// lint套件：不生成世界，只查"构建产物和源码是否一致"这类历史上最贵的坑——
// mod DLL比Dependence头文件旧 = 虚表/结构体布局可能已不一致(跨DLL读越界、崩溃)，见cross_dll_allocator_crash。

namespace {
	fs::file_time_type NewestHeader(const fs::path& root, string& newestPath) {
		fs::file_time_type newest = fs::file_time_type::min();
		error_code ec;
		for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
			if (!it->is_regular_file(ec)) continue;
			const string ext = it->path().extension().string();
			if (ext != ".h" && ext != ".hpp") continue;
			const fs::file_time_type time = it->last_write_time(ec);
			if (time > newest) { newest = time; newestPath = it->path().string(); }
		}
		return newest;
	}
}

void RunLintSuite(const TestOptions& options) {
	TestRun& run = TestRun::Get();
	run.Begin("lint", 0, options.selfCommand + " --suite lint");
	string error;
	if (!LoadTestConfig(options.configPath, 0, options.outDir, error)) {
		run.Check(false, "load config", error);
		return;
	}

	string newestPath;
	const fs::file_time_type newestHeader = NewestHeader(fs::path(options.sourceRoot) / "Dependence", newestPath);
	run.Note("newest Dependence header: " + newestPath);

	int dlls = 0;
	for (const string& path : Config::GetMods()) {
		error_code ec;
		if (!fs::exists(path, ec)) {
			run.Check(false, "configured mod dll exists: " + path);
			continue;
		}
		dlls++;
		const fs::file_time_type time = fs::last_write_time(path, ec);
		run.Check(time >= newestHeader, "mod dll is newer than every Dependence header: " + fs::path(path).filename().string(),
			"rebuild it (build.bat) - " + newestPath + " changed after the dll was built");
	}
	run.Metric("mod dlls checked", dlls, "");
	run.Check(dlls > 0, "at least one mod dll is configured");
}
