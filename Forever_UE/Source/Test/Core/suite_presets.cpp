#include "suites.h"

#include <cmath>
#include <filesystem>
#include <fstream>

#include "test_framework.h"
#include "world_build.h"

#include "common/json.h"
#include "map/map.h"
#include "map/zone.h"

using namespace std;

// presets套件：路网文件里的园区/建筑预设(Tools/MapEditor/samples/presets.roadnet，由tests/make_presets.js生成)。
//   平地上40x40方框路 + 一块居住地块 + 4个预设(第2、3、4个落在第一个预设切出的子地块里；第4个id不存在)。
//   检查：Core实际放出的zone矩形 == 编辑器模拟器算出的期望(presets_expect.json，验证本地坐标系/方向/单位)；无效id只告警但占位；
//   config里zone_residence_high带--skip，所以zone只有预设的3个(mod自己的Assign被跳过)。
// 重新生成fixture后期望值要同步更新下面的kExpect。

namespace {
	struct Expect { float x, y, w, h; };   // 左上角+尺寸；Quad的位置是中心
	const Expect kExpect[] = { { 101, 101, 10, 10 }, { 101, 129, 10, 10 }, { 129, 101, 10, 10 } };
}

void PresetsSuiteBody(TestWorld& world, const TestOptions& options) {
	(void)options;
	TestRun& run = TestRun::Get();
	const auto& zones = world.map->GetZones();
	run.Check(zones.size() == 3, "only the 3 valid presets became zones (--skip silences the mod's own Assign)", to_string(zones.size()));
	for (const Expect& e : kExpect) {
		bool found = false;
		for (const auto& kv : zones) {
			const Zone* z = kv.second;
			if (fabsf(z->GetPosX() - (e.x + e.w / 2)) < 1e-3f && fabsf(z->GetPosY() - (e.y + e.h / 2)) < 1e-3f && fabsf(z->GetSizeX() - e.w) < 1e-3f && fabsf(z->GetSizeY() - e.h) < 1e-3f) found = true;
		}
		run.Check(found, "a zone sits exactly on the editor-simulated preset rect", to_string(e.x) + "," + to_string(e.y));
	}
	for (const auto& kv : zones) {
		run.Check(kv.second->GetParentLot() != nullptr, "preset zone has a parent lot");
		run.Check(kv.second->GetDirection() >= 0, "preset zone was laid out");
	}
}

void RunPresetsSuite(const TestOptions& options) {
	TestRun& run = TestRun::Get();
	run.Begin("presets", 0, options.selfCommand + " --suite presets");
	filesystem::path samples = filesystem::path(options.sourceRoot).parent_path() / "Tools" / "MapEditor" / "samples";
	const filesystem::path roadnet = samples / "presets.roadnet", terrain = samples / "presets.terrain";
	if (!filesystem::exists(roadnet) || !filesystem::exists(terrain)) {
		run.Check(false, "presets map files exist", roadnet.string() + " / " + terrain.string());
		return;
	}
	JsonValue root;
	{
		ifstream in(options.configPath);
		JsonReader reader;
		if (!in || !reader.Parse(in, root)) {
			run.Check(false, "read base config", options.configPath);
			return;
		}
	}
	auto slashes = [](string text) { for (char& c : text) if (c == '\\') c = '/'; return text; };
	JsonValue roadnets(DATA_ARRAY), terrains(DATA_ARRAY), zonesList(DATA_ARRAY);
	roadnets.append(JsonValue("roadnet_file --path=" + slashes(roadnet.string())));
	terrains.append(JsonValue("terrain_file --path=" + slashes(terrain.string())));
	zonesList.append(JsonValue("zone_residence_high --skip"));
	root["roadnet_mods"] = roadnets;
	root["terrain_mods"] = terrains;
	root["zone_mods"] = zonesList;
	error_code ec;
	const filesystem::path dir = filesystem::path(options.outDir) / "child";
	filesystem::create_directories(dir, ec);
	const filesystem::path derived = dir / "config_presets.json";
	{
		ofstream out(derived, ios::binary);
		StyledWriter writer;
		out << writer.Write(root);
	}
	TestOptions copy = options;
	copy.configPath = derived.string();
	RunSuiteInChildren(copy, "presets", 256);
}
