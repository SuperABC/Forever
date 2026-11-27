#include "world_build.h"
#include "common/error.h"

#include "test_framework.h"

#include "common/config.h"
#include "common/json.h"
#include "common/random.h"
#include "common/save_archive.h"
#include "common/registry.h"
#include "common/save.h"
#include "map/map.h"
#include "map/building.h"
#include "map/room.h"
#include "populace/populace.h"
#include "society/society.h"
#include "society/organization.h"
#include "society/settlement.h"
#include "industry/industry.h"
#include "traffic/traffic.h"
#include "story/story.h"
#include "story/script_save.h"
#include "map/map_save.h"
#include "populace/populace_save.h"
#include "society/society_save.h"
#include "polit/polit.h"
#include "polit/polit_save.h"
#include "common/implement.h"
#include "industry/industry_save.h"
#include "traffic/traffic_save.h"
#include "player/player_save.h"
#include "player/player.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>


using namespace std;

bool LoadTestConfig(const string& basePath, unsigned long long seed, const string& workDir, string& error) {
	string path = basePath;
	if (seed != 0) {
		ifstream in(basePath);
		if (!in) { error = "cannot open config " + basePath; return false; }
		JsonReader reader;
		JsonValue root;
		if (!reader.Parse(in, root)) { error = "bad config json: " + reader.GetErrorMessages(); return false; }
		in.close();
		root["seed"] = JsonValue(to_string(seed));
		// 顶层seed只控制Core各域；mod流只认条目里的--seed，不给就是random_device。要让整局可复现，
		// 给每个没写--seed的mod条目注入一个由(种子,mod id)派生的--seed。
		const string suffix = "_mods";
		for (const string& key : root.GetMembers()) {
			if (key.size() <= suffix.size() || key.compare(key.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
			if (!root[key].IsArray()) continue;
			JsonValue rewritten(DATA_ARRAY);
			for (const auto& entry : root[key]) {
				string raw = entry.AsString();
				if (raw.find("--seed") == string::npos) {
					const string id = raw.substr(0, raw.find(' '));
					unsigned long long derived = Fnv1a(id.data(), id.size(), seed * 0x9e3779b97f4a7c15ULL + 1469598103934665603ULL);
					if (derived == 0) derived = 1;
					raw += " --seed " + to_string(derived);
				}
				rewritten.append(JsonValue(raw));
			}
			root[key] = rewritten;
		}
		error_code ec;
		filesystem::create_directories(workDir, ec);
		path = (filesystem::path(workDir) / ("config_seed" + to_string(seed) + ".json")).string();
		ofstream out(path, ios::binary);
		if (!out) { error = "cannot write " + path; return false; }
		StyledWriter writer;
		out << writer.Write(root);
	}
	Config::ReadConfig(path);
	Registry::Get().RefreshMods();
	return true;
}

namespace {
	// 阶段结束时的状态签名：所有随机流的状态 + 已有各域的计数 + 地形(高度/类型)哈希。
	string StageSignature(TestWorld& world) {
		unsigned long long h = 1469598103934665603ULL;
		auto mix = [&h](unsigned long long v) { h = Fnv1a(&v, sizeof(v), h); };
		for (const RandomRegistry::Entry& entry : RandomRegistry::Snapshot()) {
			h = Fnv1a(entry.key.data(), entry.key.size(), h);
			for (int i = 0; i < 4; i++) mix(entry.state[i]);
		}
		if (world.map) {
			const auto size = world.map->GetSize();
			for (int y = 0; y < size.second; y++) {
				for (int x = 0; x < size.first; x++) {
					const float height = world.map->GetHeight(x, y);
					h = Fnv1a(&height, sizeof(height), h);
					const string& terrain = world.map->GetTerrainRef(x, y);
					h = Fnv1a(terrain.data(), terrain.size(), h);
				}
			}
			mix(world.map->GetRoads().size());
			mix(world.map->GetIntersections().size());
			mix(world.map->GetLots().size());
			mix(world.map->GetZones().size());
			mix(world.map->GetBuildings().size());
		}
		if (world.populace) mix(world.populace->GetCitizens().size());
		if (world.society) mix(world.society->GetOrganizations().size());
		if (world.traffic) { mix(world.traffic->GetVehicles().size()); mix(world.traffic->GetStations().size()); }
		return HexString(h);
	}

	bool RunStage(TestWorld& world, const string& name, const function<void()>& fn, string& error) {
		size_t traceLength = 0;
		char traceFlag[4];
		if (getenv_s(&traceLength, traceFlag, sizeof(traceFlag), "CORETEST_TRACE") == 0 && traceLength > 0) fprintf(stderr, "[stage] %s\n", name.c_str());
		const unsigned long long before = RandomRegistry::UnscopedDraws();
		const auto begin = chrono::steady_clock::now();
		// 和游戏里一样：登记"当时在做哪个阶段"，出错时报告里带上阶段、mod、文件。
		ErrorReport report;
		const string stageName = name;
		if (!GuardCall([&] { ErrorScope stageScope(kErrorScopeStep, stageName); fn(); }, report)) {
			error = stageName + ": " + report.ToLine();
			return false;
		}
		WorldStage stage;
		stage.name = name;
		stage.seconds = chrono::duration<double>(chrono::steady_clock::now() - begin).count();
		stage.unscopedDelta = RandomRegistry::UnscopedDraws() - before;
		stage.privateMB = ReadProcessMemory().privateMB;
		stage.signature = StageSignature(world);
		world.stages.push_back(stage);
		return true;
	}

	// 导航图指纹：按节点id排序后，把邻接表(含顺序)和端点坐标哈进去。
	string GraphFingerprint(const NavGraph& graph) {
		vector<int> ids;
		for (const auto& entry : graph) ids.push_back(entry.first);
		sort(ids.begin(), ids.end());
		unsigned long long h = 1469598103934665603ULL;
		auto mix = [&h](unsigned long long v) { h = Fnv1a(&v, sizeof(v), h); };
		for (int id : ids) {
			mix(static_cast<unsigned long long>(id));
			for (const auto& [to, conn] : graph.at(id)) {
				mix(static_cast<unsigned long long>(to));
				mix(static_cast<unsigned long long>(static_cast<long long>(conn->GetStart().GetX() * 100.f)));
				mix(static_cast<unsigned long long>(static_cast<long long>(conn->GetEnd().GetY() * 100.f)));
			}
		}
		return to_string(ids.size()) + ":" + HexString(h);
	}
}

bool BuildWorld(TestWorld& world, int mapSize, string& error) {
	world.mapSize = mapSize;
	auto stage = [&](const string& name, const function<void()>& fn) { return RunStage(world, name, fn, error); };

	if (!stage("map.terrains", [&] { world.map = new Map(mapSize, mapSize); world.map->InitTerrains(); })) return false;
	if (!stage("map.roadnet", [&] { world.map->InitRoadnet(); })) return false;
	if (!stage("map.zones", [&] { world.map->InitZones(); })) return false;
	if (!stage("map.buildings", [&] { world.map->InitBuildings(); })) return false;
	if (!stage("map.greens", [&] { world.map->InitGreens(); })) return false;
	if (!stage("map.vegetation", [&] { world.map->InitVegetation(); })) return false;
	if (!stage("map.lamps", [&] { world.map->InitLamps(); })) return false;
	// 交通在地形网格之前、人口之前构建(和UE同序)；载具分配在人口/社会/资产就绪之后。
	if (!stage("traffic", [&] {
		world.traffic = new Traffic();
		world.traffic->Init(world.map);
	})) return false;
	if (!stage("populace", [&] {
		world.populace = new Populace();
		world.populace->Init(world.map->ComputeAccommodationTarget(), world.map->ComputeWorkCapacity());
		world.map->Checkin(*world.populace);
	})) return false;
	if (!stage("society", [&] {
		world.society = new Society();
		world.society->Init(world.map->GetAllComponents());
		const Time startTime(world.populace->GetCurrentYear(), 1, 1, Player::kStartHour);
		world.society->RecruitCitizens(world.populace->GetCitizens(), startTime);
		world.society->GenerateEmploymentHistory(world.populace->GetCitizens(), world.populace->GetCurrentYear());
		world.society->AssignShareholders(world.populace->GetCitizens(), startTime);
		Settlement::InitFunds(*world.society, world.populace->GetCitizens());
	})) return false;
	if (!stage("player", [&] {
		world.player = new Player();
		world.player->Init();
		world.player->SetTime(Time(world.populace->GetCurrentYear(), 1, 1, Player::kStartHour));
	})) return false;
	if (!stage("industry", [&] {
		world.industry = new Industry();
		world.industry->Init(world.map->GetAllComponents());
		world.industry->SeedInitialStock();
	})) return false;
	if (!stage("assets", [&] {
		world.map->BuildEstateAssets();
		world.society->SetGovernmentAssets(world.map->GetStatedEstateAssets());
		world.traffic->AssignVehicles(*world.populace, *world.society);
	})) return false;
	if (!stage("story", [&] {
		world.story = new Story();
		world.story->Init();
	})) return false;
	// 政治域放最后：党派随机流独立，不影响前面各阶段的签名。没启用任何党派时只是绑定默认政策。
	if (!stage("polit", [&] {
		PostImplement post(world.map, world.populace, world.society, world.story, world.industry, world.traffic, world.player);
		world.polit = new Polit();
		world.polit->Bind(*world.map, *world.populace, *world.society);
		world.polit->Init(post, *world.player->GetTime());
	})) return false;
	return true;
}

map<string, string> ComputeFingerprint(TestWorld& world) {
	map<string, string> fp;
	if (world.map) {
		fp["count.roads"] = to_string(world.map->GetRoads().size());
		fp["count.intersections"] = to_string(world.map->GetIntersections().size());
		fp["count.lots"] = to_string(world.map->GetLots().size());
		fp["count.zones"] = to_string(world.map->GetZones().size());
		fp["count.buildings"] = to_string(world.map->GetBuildings().size());
		fp["count.doors"] = to_string(world.map->GetAllDoors().size());
		size_t rooms = 0;
		for (const auto& [name, building] : world.map->GetBuildings()) rooms += building->GetRooms().size();
		fp["count.rooms"] = to_string(rooms);
		fp["nav.vehicle"] = GraphFingerprint(world.map->GetVehicleNavGraph());
		fp["nav.pedestrian"] = GraphFingerprint(world.map->GetPedestrianNavGraph());
	}
	if (world.populace) fp["count.citizens"] = to_string(world.populace->GetCitizens().size());
	if (world.society) fp["count.organizations"] = to_string(world.society->GetOrganizations().size());
	if (world.traffic) {
		fp["count.vehicles"] = to_string(world.traffic->GetVehicles().size());
		fp["count.stations"] = to_string(world.traffic->GetStations().size());
		fp["count.routes"] = to_string(world.traffic->GetRoutes().size());
	}

	SaveWorld save;
	save.map = world.map;
	save.populace = world.populace;
	save.society = world.society;
	save.industry = world.industry;
	save.traffic = world.traffic;
	save.story = world.story;
	save.player = world.player;
	save.polit = world.polit;

	// 和SaveGame::BuildArchive同一组codec，但逐个捕获异常，失败时能指出是哪个域。
	SaveArchive archive;
	auto section = [&](const char* name, const function<void()>& fn) {
		size_t traceLength = 0;
		char traceFlag[4];
		if (getenv_s(&traceLength, traceFlag, sizeof(traceFlag), "CORETEST_TRACE") == 0 && traceLength > 0) fprintf(stderr, "[fp] %s\n", name);
		ErrorReport report;
		if (!GuardCall(fn, report)) fp[string("error.") + name] = report.ToLine();
	};
	section("manifest", [&] { archive.SetJson(SAVE_SECTION_MANIFEST, 1, SaveGame::BuildManifest(save).ToJson().WriteCompact()); });
	section("map", [&] { MapSaveCodec::Save(*world.map, archive); });
	if (world.populace) section("populace", [&] { PopulaceSaveCodec::Save(*world.populace, *world.map, world.society, archive); });
	if (world.populace && world.society) section("society", [&] { SocietySaveCodec::Save(*world.society, *world.map, *world.populace, archive); });
	if (world.populace && world.polit) section("polit", [&] { PolitSaveCodec::Save(*world.polit, *world.populace, archive); });
	if (world.industry) section("industry", [&] { IndustrySaveCodec::Save(*world.industry, archive); });
	if (world.traffic) section("traffic", [&] { TrafficSaveCodec::Save(*world.traffic, *world.map, world.populace, world.society, archive); });
	section("scripts", [&] { ScriptSaveCodec::Save(save, archive); });
	if (world.player) section("player", [&] { PlayerSaveCodec::Save(*world.player, *world.map, archive); });
	for (const SaveSection& sec : archive.GetSections()) {
		fp["section." + sec.name] = to_string(sec.data.size()) + ":" + HexString(Fnv1a(sec.data.data(), sec.data.size()));
	}

	// 每条随机流的种子和当前状态(指纹时点固定，所以连状态一起比)。
	for (const RandomRegistry::Entry& entry : RandomRegistry::Snapshot()) {
		unsigned long long h = Fnv1a(entry.state, sizeof(entry.state));
		fp["random." + entry.key] = to_string(entry.seed) + ":" + HexString(h);
	}
	return fp;
}
