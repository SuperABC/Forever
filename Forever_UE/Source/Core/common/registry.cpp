#include "registry.h"

#include "common/config.h"
#include "common/random.h"


using namespace std;

namespace {
	unordered_map<string, string> ToArgsMap(const vector<pair<string, string>>& entries) {
		unordered_map<string, string> args;
		for (const auto& [id, value] : entries) {
			args[id] = value;
		}
		return args;
	}

	// ToArgsMap + 剥离每个条目里的"--seed N"并登记到RandomRegistry(mod构造函数收到的参数里没有--seed)。
	unordered_map<string, string> ExtractSeeds(const vector<pair<string, string>>& entries) {
		unordered_map<string, string> args;
		for (const auto& [id, value] : entries) {
			bool hasSeed = false;
			uint64_t seed = 0;
			args[id] = RandomRegistry::ExtractSeed(value, hasSeed, seed);
			RandomRegistry::SetModSeed(id, hasSeed, seed);
		}
		return args;
	}
}

Registry& Registry::Get() {
	static Registry instance;
	return instance;
}

void Registry::RegisterMods(const vector<string>& mods) {
	modLoader.RegisterConcept<TerrainFactory>(mods, "RegisterModTerrains", "FinishModTerrains", &terrainFactory);
	modLoader.RegisterConcept<RoadnetFactory>(mods, "RegisterModRoadnets", "FinishModRoadnets", &roadnetFactory);
	modLoader.RegisterConcept<ZoneFactory>(mods, "RegisterModZones", "FinishModZones", &zoneFactory);
	modLoader.RegisterConcept<GreenFactory>(mods, "RegisterModGreens", "FinishModGreens", &greenFactory);
	modLoader.RegisterConcept<BuildingFactory>(mods, "RegisterModBuildings", "FinishModBuildings", &buildingFactory);
	modLoader.RegisterConcept<ComponentFactory>(mods, "RegisterModComponents", "FinishModComponents", &componentFactory);
	modLoader.RegisterConcept<RoomFactory>(mods, "RegisterModRooms", "FinishModRooms", &roomFactory);
	modLoader.RegisterConcept<AssetFactory>(mods, "RegisterModAssets", "FinishModAssets", &assetFactory);
	modLoader.RegisterConcept<AppFactory>(mods, "RegisterModApps", "FinishModApps", &appFactory);
	modLoader.RegisterConcept<PuzzleFactory>(mods, "RegisterModPuzzles", "FinishModPuzzles", &puzzleFactory);
	modLoader.RegisterConcept<WeaponFactory>(mods, "RegisterModWeapons", "FinishModWeapons", &weaponFactory);
	modLoader.RegisterConcept<PartyFactory>(mods, "RegisterModParties", "FinishModParties", &partyFactory);
	modLoader.RegisterConcept<NameFactory>(mods, "RegisterModNames", "FinishModNames", &nameFactory);
	modLoader.RegisterConcept<SchedulerFactory>(mods, "RegisterModSchedulers", "FinishModSchedulers", &schedulerFactory);
	modLoader.RegisterConcept<JobFactory>(mods, "RegisterModJobs", "FinishModJobs", &jobFactory);
	modLoader.RegisterConcept<OrganizationFactory>(mods, "RegisterModOrganizations", "FinishModOrganizations", &organizationFactory);
	modLoader.RegisterConcept<ScriptFactory>(mods, "RegisterModScripts", "FinishModScripts", &scriptFactory);
	modLoader.RegisterConcept<ProductFactory>(mods, "RegisterModProducts", "FinishModProducts", &productFactory);
	modLoader.RegisterConcept<StorageFactory>(mods, "RegisterModStorages", "FinishModStorages", &storageFactory);
	modLoader.RegisterConcept<ManufactureFactory>(mods, "RegisterModManufactures", "FinishModManufactures", &manufactureFactory);
	modLoader.RegisterConcept<RouteFactory>(mods, "RegisterModRoutes", "FinishModRoutes", &routeFactory);
	modLoader.RegisterConcept<StationFactory>(mods, "RegisterModStations", "FinishModStations", &stationFactory);
	modLoader.RegisterConcept<VehicleFactory>(mods, "RegisterModVehicles", "FinishModVehicles", &vehicleFactory);

	for (const string& path : mods) {
		if (modLoader.IsLoaded(path)) loadedDllPaths.insert(path);
	}
}

void Registry::RefreshMods() {
	vector<string> newMods;
	for (const string& path : Config::GetMods()) {
		if (!loadedDllPaths.count(path)) newMods.push_back(path);
	}
	if (!newMods.empty()) RegisterMods(newMods);
	ReloadModArgs();
}

Registry::Registry() {
	vector<string> mods = Config::GetMods();
	RegisterMods(mods);

	// 构造时先按当前已经读进内存的config内容(Config::ReadConfig)灌一遍参数,保证Get()刚拿到
	// 实例时每个Factory就有参数可用,不强制调用方第一次也要记得手动调ReloadModArgs()。
	ReloadModArgs();
}

void Registry::ReloadModArgs() {
	// 种子登记：新开局/读档都会走到这里，先清空旧流再按config重新登记；读档随后用存档状态覆盖流(见save)。
	RandomRegistry::Reset();
	unsigned long long globalSeed = 0;
	RandomRegistry::SetGlobalSeed(Config::GetGlobalSeed(globalSeed), globalSeed);
	terrainFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("terrain_mods")));
	roadnetFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("roadnet_mods")));
	zoneFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("zone_mods")));
	greenFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("green_mods")));
	buildingFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("building_mods")));
	componentFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("component_mods")));
	roomFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("room_mods")));
	assetFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("asset_mods")));
	appFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("app_mods")));
	puzzleFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("puzzle_mods")));
	weaponFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("weapon_mods")));
	partyFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("party_mods")));
	nameFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("name_mods")));
	schedulerFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("scheduler_mods")));
	jobFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("job_mods")));
	organizationFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("organization_mods")));
	scriptFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("script_mods")));
	productFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("product_mods")));
	storageFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("storage_mods")));
	manufactureFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("manufacture_mods")));
	routeFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("route_mods")));
	stationFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("station_mods")));
	vehicleFactory.SetModArgs(ExtractSeeds(Config::GetConceptMods("vehicle_mods")));
}

bool Registry::CheckModRegistered(const string& id) const {
	return terrainFactory.CheckRegistered(id)
		|| roadnetFactory.CheckRegistered(id)
		|| zoneFactory.CheckRegistered(id)
		|| greenFactory.CheckRegistered(id)
		|| partyFactory.CheckRegistered(id)
		|| buildingFactory.CheckRegistered(id)
		|| componentFactory.CheckRegistered(id)
		|| roomFactory.CheckRegistered(id)
		|| nameFactory.CheckRegistered(id)
		|| schedulerFactory.CheckRegistered(id)
		|| jobFactory.CheckRegistered(id)
		|| organizationFactory.CheckRegistered(id)
		|| scriptFactory.CheckRegistered(id)
		|| productFactory.CheckRegistered(id)
		|| storageFactory.CheckRegistered(id)
		|| manufactureFactory.CheckRegistered(id)
		|| routeFactory.CheckRegistered(id)
		|| stationFactory.CheckRegistered(id)
		|| vehicleFactory.CheckRegistered(id)
		|| assetFactory.CheckRegistered(id)
		|| appFactory.CheckRegistered(id)
		|| puzzleFactory.CheckRegistered(id)
		|| weaponFactory.CheckRegistered(id);
}
