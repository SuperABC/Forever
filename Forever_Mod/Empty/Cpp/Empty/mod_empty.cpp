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
#include "society/calendar_factory.h"
#include "society/organization_factory.h"
#include "story/script_factory.h"
#include "industry/product_factory.h"
#include "industry/storage_factory.h"
#include "industry/manufacture_factory.h"
#include "traffic/route_factory.h"
#include "traffic/station_factory.h"
#include "traffic/vehicle_factory.h"

#pragma comment(lib, "Dependence.lib")

extern "C" __declspec(dllexport) void* GetModTerrains() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModTerrains(TerrainFactory* factory) {
	factory->RegisterTerrain(EmptyTerrain::GetId(),
		[]() -> TerrainMod* { return new EmptyTerrain(); },
		[](TerrainMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModTerrains(TerrainFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModRoadnets() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRoadnets(RoadnetFactory* factory) {
	factory->RegisterRoadnet(EmptyRoadnet::GetId(),
		[]() -> RoadnetMod* { return new EmptyRoadnet(); },
		[](RoadnetMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModRoadnets(RoadnetFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModZones() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModZones(ZoneFactory* factory) {
	factory->RegisterZone(EmptyZone::GetId(),
		[]() -> ZoneMod* { return new EmptyZone(); },
		[](ZoneMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModZones(ZoneFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModBuildings() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModBuildings(BuildingFactory* factory) {
	factory->RegisterBuilding(EmptyBuilding::GetId(),
		[]() -> BuildingMod* { return new EmptyBuilding(); },
		[](BuildingMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModBuildings(BuildingFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModComponents() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModComponents(ComponentFactory* factory) {
	factory->RegisterComponent(EmptyComponent::GetId(),
		[]() -> ComponentMod* { return new EmptyComponent(); },
		[](ComponentMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModComponents(ComponentFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModRooms() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRooms(RoomFactory* factory) {
	factory->RegisterRoom(EmptyRoom::GetId(),
		[]() -> RoomMod* { return new EmptyRoom(); },
		[](RoomMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModRooms(RoomFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModAssets() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModAssets(AssetFactory* factory) {
	factory->RegisterAsset(EmptyAsset::GetId(),
		[]() -> AssetMod* { return new EmptyAsset(); },
		[](AssetMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModAssets(AssetFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModApps() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModApps(AppFactory* factory) {
	factory->RegisterApp(EmptyApp::GetId(),
		[]() -> AppMod* { return new EmptyApp(); },
		[](AppMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModApps(AppFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModPuzzles() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModPuzzles(PuzzleFactory* factory) {
	factory->RegisterPuzzle(EmptyPuzzle::GetId(),
		[]() -> PuzzleMod* { return new EmptyPuzzle(); },
		[](PuzzleMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModPuzzles(PuzzleFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModNames() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModNames(NameFactory* factory) {
	factory->RegisterName(EmptyName::GetId(),
		[]() -> NameMod* { return new EmptyName(); },
		[](NameMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModNames(NameFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModSchedulers() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModSchedulers(SchedulerFactory* factory) {
	factory->RegisterScheduler(EmptyScheduler::GetId(),
		[]() -> SchedulerMod* { return new EmptyScheduler(); },
		[](SchedulerMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModSchedulers(SchedulerFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModJobs() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModJobs(JobFactory* factory) {
	factory->RegisterJob(EmptyJob::GetId(),
		[]() -> JobMod* { return new EmptyJob(); },
		[](JobMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModJobs(JobFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModCalendars() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModCalendars(CalendarFactory* factory) {
	factory->RegisterCalendar(EmptyCalendar::GetId(),
		[]() -> CalendarMod* { return new EmptyCalendar(); },
		[](CalendarMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModCalendars(CalendarFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModOrganizations() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModOrganizations(OrganizationFactory* factory) {
	factory->RegisterOrganization(EmptyOrganization::GetId(),
		[]() -> OrganizationMod* { return new EmptyOrganization(); },
		[](OrganizationMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModOrganizations(OrganizationFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModScripts() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModScripts(ScriptFactory* factory) {
	factory->RegisterScript(EmptyScript::GetId(),
		[]() -> ScriptMod* { return new EmptyScript(); },
		[](ScriptMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModScripts(ScriptFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModProducts() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModProducts(ProductFactory* factory) {
	factory->RegisterProduct(EmptyProduct::GetId(),
		[]() -> ProductMod* { return new EmptyProduct(); },
		[](ProductMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModProducts(ProductFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModStorages() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModStorages(StorageFactory* factory) {
	factory->RegisterStorage(EmptyStorage::GetId(),
		[]() -> StorageMod* { return new EmptyStorage(); },
		[](StorageMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModStorages(StorageFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModManufactures() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModManufactures(ManufactureFactory* factory) {
	factory->RegisterManufacture(EmptyManufacture::GetId(),
		[]() -> ManufactureMod* { return new EmptyManufacture(); },
		[](ManufactureMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModManufactures(ManufactureFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModRoutes() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRoutes(RouteFactory* factory) {
	factory->RegisterRoute(EmptyRoute::GetId(),
		[]() -> RouteMod* { return new EmptyRoute(); },
		[](RouteMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModRoutes(RouteFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModStations() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModStations(StationFactory* factory) {
	factory->RegisterStation(EmptyStation::GetId(),
		[]() -> StationMod* { return new EmptyStation(); },
		[](StationMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModStations(StationFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModVehicles() {
	static std::vector<std::string> mods = { "empty" };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModVehicles(VehicleFactory* factory) {
	factory->RegisterVehicle(EmptyVehicle::GetId(),
		[]() -> VehicleMod* { return new EmptyVehicle(); },
		[](VehicleMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModVehicles(VehicleFactory* factory) {
	factory->CleanTemp();
}
