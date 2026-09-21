#include "registry.h"

#include "common/config.h"


using namespace std;

namespace {
	unordered_map<string, string> ToArgsMap(const vector<pair<string, string>>& entries) {
		unordered_map<string, string> args;
		for (const auto& [id, value] : entries) {
			args[id] = value;
		}
		return args;
	}
}

Registry& Registry::Get() {
	static Registry instance;
	return instance;
}

Registry::Registry() {
	vector<string> mods = Config::GetMods();

	modLoader.RegisterConcept<TerrainFactory>(mods, "RegisterModTerrains", "FinishModTerrains", &terrainFactory);
	modLoader.RegisterConcept<RoadnetFactory>(mods, "RegisterModRoadnets", "FinishModRoadnets", &roadnetFactory);
	modLoader.RegisterConcept<ZoneFactory>(mods, "RegisterModZones", "FinishModZones", &zoneFactory);
	modLoader.RegisterConcept<BuildingFactory>(mods, "RegisterModBuildings", "FinishModBuildings", &buildingFactory);
	modLoader.RegisterConcept<ComponentFactory>(mods, "RegisterModComponents", "FinishModComponents", &componentFactory);
	modLoader.RegisterConcept<RoomFactory>(mods, "RegisterModRooms", "FinishModRooms", &roomFactory);
	modLoader.RegisterConcept<AssetFactory>(mods, "RegisterModAssets", "FinishModAssets", &assetFactory);
	modLoader.RegisterConcept<AppFactory>(mods, "RegisterModApps", "FinishModApps", &appFactory);
	modLoader.RegisterConcept<PuzzleFactory>(mods, "RegisterModPuzzles", "FinishModPuzzles", &puzzleFactory);
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

	// 构造时先按当前已经读进内存的config内容(Config::ReadConfig)灌一遍参数,保证Get()刚拿到
	// 实例时每个Factory就有参数可用,不强制调用方第一次也要记得手动调ReloadModArgs()。
	ReloadModArgs();
}

void Registry::ReloadModArgs() {
	terrainFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("terrain_mods")));
	roadnetFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("roadnet_mods")));
	zoneFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("zone_mods")));
	buildingFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("building_mods")));
	componentFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("component_mods")));
	roomFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("room_mods")));
	assetFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("asset_mods")));
	appFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("app_mods")));
	puzzleFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("puzzle_mods")));
	nameFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("name_mods")));
	schedulerFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("scheduler_mods")));
	jobFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("job_mods")));
	organizationFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("organization_mods")));
	scriptFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("script_mods")));
	productFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("product_mods")));
	storageFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("storage_mods")));
	manufactureFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("manufacture_mods")));
	routeFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("route_mods")));
	stationFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("station_mods")));
	vehicleFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("vehicle_mods")));
}

bool Registry::CheckModRegistered(const string& id) const {
	return terrainFactory.CheckRegistered(id)
		|| roadnetFactory.CheckRegistered(id)
		|| zoneFactory.CheckRegistered(id)
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
		|| puzzleFactory.CheckRegistered(id);
}
