#include "empty_mods.h"

#include "map/terrain_factory.h"
#include "map/roadnet_factory.h"
#include "map/zone_factory.h"
#include "map/building_factory.h"
#include "map/component_factory.h"
#include "map/room_factory.h"
#include "player/asset_factory.h"
#include "player/app_factory.h"
#include "player/puzzle_factory.h"
#include "populace/name_factory.h"
#include "populace/scheduler_factory.h"
#include "society/job_factory.h"
#include "society/organization_factory.h"
#include "story/script_factory.h"
#include "industry/product_factory.h"
#include "industry/storage_factory.h"
#include "industry/manufacture_factory.h"
#include "traffic/route_factory.h"
#include "traffic/station_factory.h"
#include "traffic/vehicle_factory.h"

#pragma comment(lib, "Dependence.lib")

int EmptyZone::count = 0;
int EmptyBuilding::count = 0;

extern "C" __declspec(dllexport) void* GetModTerrains() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModTerrains(TerrainFactory* factory) {
	factory->RegisterTerrain(EmptyTerrain::GetId(),
		[](const std::string& args) -> TerrainMod* { return new EmptyTerrain(args); },
		[](TerrainMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModTerrains(TerrainFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModRoadnets() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRoadnets(RoadnetFactory* factory) {
	factory->RegisterRoadnet(EmptyRoadnet::GetId(),
		[](const std::string& args) -> RoadnetMod* { return new EmptyRoadnet(args); },
		[](RoadnetMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModRoadnets(RoadnetFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModZones() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModZones(ZoneFactory* factory) {
	factory->RegisterZone(EmptyZone::GetId(),
		[](const std::string&) -> ZoneMod* { return new EmptyZone(); },
		[](ZoneMod* m) { delete m; },
		&EmptyZone::Assign);
}
extern "C" __declspec(dllexport) void FinishModZones(ZoneFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModBuildings() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModBuildings(BuildingFactory* factory) {
	factory->RegisterBuilding(EmptyBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new EmptyBuilding(); },
		[](BuildingMod* m) { delete m; },
		&EmptyBuilding::RandomAcreage, &EmptyBuilding::GetAcreageMin, &EmptyBuilding::GetAcreageMax,
		&EmptyBuilding::GetPower, &EmptyBuilding::Assign);
}
extern "C" __declspec(dllexport) void FinishModBuildings(BuildingFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModComponents() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModComponents(ComponentFactory* factory) {
	factory->RegisterComponent(EmptyComponent::GetId(),
		[](const std::string& args) -> ComponentMod* { return new EmptyComponent(args); },
		[](ComponentMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModComponents(ComponentFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModRooms() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRooms(RoomFactory* factory) {
	factory->RegisterRoom(EmptyRoom::GetId(),
		[](const std::string& args) -> RoomMod* { return new EmptyRoom(args); },
		[](RoomMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModRooms(RoomFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModAssets() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModAssets(AssetFactory* factory) {
	factory->RegisterAsset(EmptyAsset::GetId(),
		[](const std::string& args) -> AssetMod* { return new EmptyAsset(args); },
		[](AssetMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModAssets(AssetFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModApps() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModApps(AppFactory* factory) {
	factory->RegisterApp(EmptyApp::GetId(),
		[](const std::string& args) -> AppMod* { return new EmptyApp(args); },
		[](AppMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModApps(AppFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModPuzzles() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModPuzzles(PuzzleFactory* factory) {
	factory->RegisterPuzzle(EmptyPuzzle::GetId(),
		[](const std::string& args) -> PuzzleMod* { return new EmptyPuzzle(args); },
		[](PuzzleMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModPuzzles(PuzzleFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModNames() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModNames(NameFactory* factory) {
	factory->RegisterName(EmptyName::GetId(),
		[](const std::string& args) -> NameMod* { return new EmptyName(args); },
		[](NameMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModNames(NameFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModSchedulers() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModSchedulers(SchedulerFactory* factory) {
	factory->RegisterScheduler(EmptyScheduler::GetId(),
		[](const std::string& args) -> SchedulerMod* { return new EmptyScheduler(args); },
		[](SchedulerMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModSchedulers(SchedulerFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModJobs() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModJobs(JobFactory* factory) {
	factory->RegisterJob(EmptyJob::GetId(),
		[](const std::string& args) -> JobMod* { return new EmptyJob(args); },
		[](JobMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModJobs(JobFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModOrganizations() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModOrganizations(OrganizationFactory* factory) {
	factory->RegisterOrganization(EmptyOrganization::GetId(),
		[](const std::string& args) -> OrganizationMod* { return new EmptyOrganization(args); },
		[](OrganizationMod* m) { delete m; },
		&EmptyOrganization::GetPower);
}
extern "C" __declspec(dllexport) void FinishModOrganizations(OrganizationFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModScripts() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModScripts(ScriptFactory* factory) {
	factory->RegisterScript(EmptyScript::GetId(),
		[](const std::string& args) -> ScriptMod* { return new EmptyScript(args); },
		[](ScriptMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModScripts(ScriptFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModProducts() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModProducts(ProductFactory* factory) {
	factory->RegisterProduct(EmptyProduct::GetId(),
		[](const std::string& args) -> ProductMod* { return new EmptyProduct(args); },
		[](ProductMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModProducts(ProductFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModStorages() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModStorages(StorageFactory* factory) {
	factory->RegisterStorage(EmptyStorage::GetId(),
		[](const std::string& args) -> StorageMod* { return new EmptyStorage(args); },
		[](StorageMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModStorages(StorageFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModManufactures() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModManufactures(ManufactureFactory* factory) {
	factory->RegisterManufacture(EmptyManufacture::GetId(),
		[](const std::string& args) -> ManufactureMod* { return new EmptyManufacture(args); },
		[](ManufactureMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModManufactures(ManufactureFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModRoutes() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRoutes(RouteFactory* factory) {
	factory->RegisterRoute(EmptyRoute::GetId(),
		[](const std::string& args) -> RouteMod* { return new EmptyRoute(args); },
		[](RouteMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModRoutes(RouteFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModStations() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModStations(StationFactory* factory) {
	factory->RegisterStation(EmptyStation::GetId(),
		[](const std::string& args) -> StationMod* { return new EmptyStation(args); },
		[](StationMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModStations(StationFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModVehicles() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModVehicles(VehicleFactory* factory) {
	factory->RegisterVehicle(EmptyVehicle::GetId(),
		[](const std::string& args) -> VehicleMod* { return new EmptyVehicle(args); },
		[](VehicleMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModVehicles(VehicleFactory* factory) {
}
