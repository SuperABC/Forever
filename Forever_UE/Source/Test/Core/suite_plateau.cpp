#include "suites.h"

#include <cmath>
#include <filesystem>
#include <fstream>

#include "test_framework.h"
#include "world_build.h"

#include "common/json.h"
#include "map/building.h"
#include "map/door.h"
#include "map/general_geometry.h"
#include "map/map.h"
#include "map/room.h"
#include "map/zone.h"

using namespace std;

// plateau套件：抹平出来的非0高度平台上的地块(Tools/MapEditor/samples/plateau.roadnet，由tests/make_plateau.js生成)。
//   平台高度h(!=0)，平台上方框路(节点z=h)围着一块地块(corners带z)，放了建筑/园区预设；另有一条z=0的路贴着地块的另一条边。
//   检查(对所有"所属地块z!=0"的建筑/园区)：
//     - 地块自己的z、平台格子被标成construction但不是"z=0的平地"(IsGroundConstruction，路网mod用的回调)；
//     - Building/Zone::GetGroundZ()==所属地块z；房间导航节点z=地坪+楼层相对高度；建筑门/园区大门/园区内部路的z不低于地坪；
//     - 地块的每条边界路都是水平的、高度等于地块z(不等高的路不算边界路)。
// 夹具缺失=失败。重新生成夹具后不需要改常量(检查都按"地块z!=0"自适应)。

namespace {
	constexpr float kTolerance = 1e-3f;

	bool IsNear(float a, float b) { return fabsf(a - b) <= kTolerance; }
}

void PlateauSuiteBody(TestWorld& world, const TestOptions& options) {
	(void)options;
	TestRun& run = TestRun::Get();
	int elevatedBuildings = 0, elevatedZones = 0;

	auto checkLot = [&](const Lot* lot, const string& label) {
		float z = lot->GetZ();
		run.Check(fabsf(z) > kTolerance, "an elevated lot has a non-zero z", label + " z=" + to_string(z));
		// 平台中心格：construction为真，但不是z=0的平地。
		int cx = static_cast<int>(floorf(lot->GetPosX())), cy = static_cast<int>(floorf(lot->GetPosY()));
		run.Check(world.map->IsConstruction(cx, cy), "the plateau cell under an elevated lot is construction", label);
		run.Check(!world.map->IsGroundConstruction(cx, cy), "the plateau cell is not a z=0 ground cell (road mods never build there)", label);
		run.Check(IsNear(world.map->SampleHeight(lot->GetPosX(), lot->GetPosY()), z), "the terrain under the lot is at the lot's z",
			label + " terrain=" + to_string(world.map->SampleHeight(lot->GetPosX(), lot->GetPosY())) + " lot=" + to_string(z));
		for (const auto& [direction, road] : lot->GetBoundaryRoads()) {
			if (!road) continue;
			run.Check(IsNear(road->GetStart().GetZ(), z) && IsNear(road->GetEnd().GetZ(), z), "a boundary road of an elevated lot is level with it",
				label + " dir " + to_string(direction));
		}
	};

	for (const auto& [name, building] : world.map->GetBuildings()) {
		const Lot* lot = building ? building->GetParentLot() : nullptr;
		if (!lot || fabsf(lot->GetZ()) <= kTolerance) continue;
		elevatedBuildings++;
		checkLot(lot, name);
		run.Check(IsNear(building->GetGroundZ(), lot->GetZ()), "building ground z equals its lot z", name);
		run.Check(IsNear(building->GetFloorWorldZ(0), lot->GetZ() + building->GetFloorBaseZ(0)), "world floor z = ground + relative floor z", name);
		for (const Room* room : building->GetRooms()) {
			const Node* node = room ? room->GetNavigationNode() : nullptr;
			if (!node) continue;
			run.Check(IsNear(node->GetZ(), building->GetFloorWorldZ(room->GetLayer())), "room navigation node sits on the world floor z", name);
		}
		for (const Door* door : building->GetDoorEntities()) {
			if (!door) continue;
			run.Check(door->GetZ() >= lot->GetZ() + building->GetFloorBaseZ(-building->GetBasementCount()) - kTolerance, "building door is not below the building", name);
		}
	}
	for (const auto& [name, zone] : world.map->GetZones()) {
		const Lot* lot = zone ? zone->GetParentLot() : nullptr;
		if (!lot || fabsf(lot->GetZ()) <= kTolerance) continue;
		elevatedZones++;
		checkLot(lot, name);
		run.Check(IsNear(zone->GetGroundZ(), lot->GetZ()), "zone ground z equals its lot z", name);
		for (const Door* door : zone->GetDoorEntities()) {
			if (door) run.Check(IsNear(door->GetZ(), lot->GetZ()), "zone gate door sits on the zone ground", name);
		}
		for (const Road* road : zone->GetInternalRoads()) {
			if (road) run.Check(IsNear(road->GetStart().GetZ(), lot->GetZ()) && IsNear(road->GetEnd().GetZ(), lot->GetZ()), "zone internal road sits on the zone ground", name);
		}
	}
	run.Check(elevatedBuildings + elevatedZones > 0, "the fixture produced at least one building/zone on an elevated lot",
		to_string(elevatedBuildings) + " buildings, " + to_string(elevatedZones) + " zones");
}

void RunPlateauSuite(const TestOptions& options) {
	TestRun& run = TestRun::Get();
	run.Begin("plateau", 0, options.selfCommand + " --suite plateau");
	filesystem::path samples = filesystem::path(options.sourceRoot).parent_path() / "Tools" / "MapEditor" / "samples";
	const filesystem::path roadnet = samples / "plateau.roadnet", terrain = samples / "plateau.terrain";
	if (!filesystem::exists(roadnet) || !filesystem::exists(terrain)) {
		run.Check(false, "plateau map files exist", roadnet.string() + " / " + terrain.string());
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
	JsonValue roadnets(DATA_ARRAY), terrains(DATA_ARRAY);
	roadnets.append(JsonValue("roadnet_file --path=" + slashes(roadnet.string())));
	terrains.append(JsonValue("terrain_file --path=" + slashes(terrain.string())));
	root["roadnet_mods"] = roadnets;
	root["terrain_mods"] = terrains;
	error_code ec;
	const filesystem::path dir = filesystem::path(options.outDir) / "child";
	filesystem::create_directories(dir, ec);
	const filesystem::path derived = dir / "config_plateau.json";
	{
		ofstream out(derived, ios::binary);
		StyledWriter writer;
		out << writer.Write(root);
	}
	TestOptions copy = options;
	copy.configPath = derived.string();
	RunSuiteInChildren(copy, "plateau", 256);
}
