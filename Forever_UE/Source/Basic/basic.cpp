// Basic编译为DynamicLibrary(Basic.dll),是内置的默认Mod集合——和Forever_Mod/Test、
// Forever_Mod/Wxdj地位相同,由Config/ModLoader在运行时通过本文件导出的21组
// GetMod<Concept>/RegisterMod<Concept>/FinishMod<Concept>符号扫描加载,不会静态链进
// Forever.Build.cs。详见 Source/Basic/README.md。

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

#include "map/terrain_basic.h"
#include "map/roadnet_basic.h"
#include "map/zone_residence.h"
#include "map/building_residence.h"
#include "map/component_residence.h"
#include "map/room_residence.h"
#include "player/asset_basic.h"
#include "player/app_basic.h"
#include "player/puzzle_basic.h"
#include "populace/name_chinese.h"
#include "populace/scheduler_basic.h"
#include "society/job_basic.h"
#include "society/calendar_basic.h"
#include "society/organization_basic.h"
#include "story/script_basic.h"
#include "industry/product_basic.h"
#include "industry/storage_basic.h"
#include "industry/manufacture_basic.h"
#include "traffic/route_basic.h"
#include "traffic/station_basic.h"
#include "traffic/vehicle_basic.h"

#pragma comment(lib, "Dependence.lib")

// 阶段3每个概念只有一个trivial占位XxxBasic,阶段4按系统迁移进度替换成旧工程真正的默认内容
// 目录(如map域Zone/Building/Room/Component这4个已经在进入populace域时改名成
// ResidenceZone/ResidenceBuilding/ResidenceRoom/ResidenceComponent，其余概念暂时仍是
// XxxBasic命名，等各自域真正迁移时再按需改名),完整对照表见
// Source/Dependence/README.md 的21个concept对照表。

using namespace std;

extern "C" __declspec(dllexport) void* GetModTerrains() {
	static vector<string> mods = { OceanTerrain::GetId(), MountainTerrain::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModTerrains(TerrainFactory* factory) {
	factory->RegisterTerrain(OceanTerrain::GetId(),
		[]() -> TerrainMod* { return new OceanTerrain(); },
		[](TerrainMod* m) { delete m; });
	factory->RegisterTerrain(MountainTerrain::GetId(),
		[]() -> TerrainMod* { return new MountainTerrain(); },
		[](TerrainMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModTerrains(TerrainFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModRoadnets() {
	static vector<string> mods = { JingRoadnet::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRoadnets(RoadnetFactory* factory) {
	factory->RegisterRoadnet(JingRoadnet::GetId(),
		[]() -> RoadnetMod* { return new JingRoadnet(); },
		[](RoadnetMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModRoadnets(RoadnetFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModZones() {
	static vector<string> mods = { ResidenceZone::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModZones(ZoneFactory* factory) {
	factory->RegisterZone(ResidenceZone::GetId(),
		[]() -> ZoneMod* { return new ResidenceZone(); },
		[](ZoneMod* m) { delete m; },
		&ResidenceZone::Assign);
}
extern "C" __declspec(dllexport) void FinishModZones(ZoneFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModBuildings() {
	static vector<string> mods = { ResidenceBuilding::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModBuildings(BuildingFactory* factory) {
	factory->RegisterBuilding(ResidenceBuilding::GetId(),
		[]() -> BuildingMod* { return new ResidenceBuilding(); },
		[](BuildingMod* m) { delete m; },
		&ResidenceBuilding::RandomAcreage, &ResidenceBuilding::GetAcreageMin, &ResidenceBuilding::GetAcreageMax,
		&ResidenceBuilding::GetPower, &ResidenceBuilding::Assign);
}
extern "C" __declspec(dllexport) void FinishModBuildings(BuildingFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModComponents() {
	static vector<string> mods = { ResidenceComponent::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModComponents(ComponentFactory* factory) {
	factory->RegisterComponent(ResidenceComponent::GetId(),
		[]() -> ComponentMod* { return new ResidenceComponent(); },
		[](ComponentMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModComponents(ComponentFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModRooms() {
	static vector<string> mods = { ResidenceRoom::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRooms(RoomFactory* factory) {
	factory->RegisterRoom(ResidenceRoom::GetId(),
		[]() -> RoomMod* { return new ResidenceRoom(); },
		[](RoomMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModRooms(RoomFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModAssets() {
	static vector<string> mods = { AssetBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModAssets(AssetFactory* factory) {
	factory->RegisterAsset(AssetBasic::GetId(),
		[]() -> AssetMod* { return new AssetBasic(); },
		[](AssetMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModAssets(AssetFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModApps() {
	static vector<string> mods = { AppBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModApps(AppFactory* factory) {
	factory->RegisterApp(AppBasic::GetId(),
		[]() -> AppMod* { return new AppBasic(); },
		[](AppMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModApps(AppFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModPuzzles() {
	static vector<string> mods = { PuzzleBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModPuzzles(PuzzleFactory* factory) {
	factory->RegisterPuzzle(PuzzleBasic::GetId(),
		[]() -> PuzzleMod* { return new PuzzleBasic(); },
		[](PuzzleMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModPuzzles(PuzzleFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModNames() {
	static vector<string> mods = { ChineseName::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModNames(NameFactory* factory) {
	factory->RegisterName(ChineseName::GetId(),
		[]() -> NameMod* { return new ChineseName(); },
		[](NameMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModNames(NameFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModSchedulers() {
	static vector<string> mods = { SchedulerBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModSchedulers(SchedulerFactory* factory) {
	factory->RegisterScheduler(SchedulerBasic::GetId(),
		[]() -> SchedulerMod* { return new SchedulerBasic(); },
		[](SchedulerMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModSchedulers(SchedulerFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModJobs() {
	static vector<string> mods = { JobBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModJobs(JobFactory* factory) {
	factory->RegisterJob(JobBasic::GetId(),
		[]() -> JobMod* { return new JobBasic(); },
		[](JobMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModJobs(JobFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModCalendars() {
	static vector<string> mods = { CalendarBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModCalendars(CalendarFactory* factory) {
	factory->RegisterCalendar(CalendarBasic::GetId(),
		[]() -> CalendarMod* { return new CalendarBasic(); },
		[](CalendarMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModCalendars(CalendarFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModOrganizations() {
	static vector<string> mods = { OrganizationBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModOrganizations(OrganizationFactory* factory) {
	factory->RegisterOrganization(OrganizationBasic::GetId(),
		[]() -> OrganizationMod* { return new OrganizationBasic(); },
		[](OrganizationMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModOrganizations(OrganizationFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModScripts() {
	static vector<string> mods = { ScriptBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModScripts(ScriptFactory* factory) {
	factory->RegisterScript(ScriptBasic::GetId(),
		[]() -> ScriptMod* { return new ScriptBasic(); },
		[](ScriptMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModScripts(ScriptFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModProducts() {
	static vector<string> mods = { ProductBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModProducts(ProductFactory* factory) {
	factory->RegisterProduct(ProductBasic::GetId(),
		[]() -> ProductMod* { return new ProductBasic(); },
		[](ProductMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModProducts(ProductFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModStorages() {
	static vector<string> mods = { StorageBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModStorages(StorageFactory* factory) {
	factory->RegisterStorage(StorageBasic::GetId(),
		[]() -> StorageMod* { return new StorageBasic(); },
		[](StorageMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModStorages(StorageFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModManufactures() {
	static vector<string> mods = { ManufactureBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModManufactures(ManufactureFactory* factory) {
	factory->RegisterManufacture(ManufactureBasic::GetId(),
		[]() -> ManufactureMod* { return new ManufactureBasic(); },
		[](ManufactureMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModManufactures(ManufactureFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModRoutes() {
	static vector<string> mods = { RouteBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRoutes(RouteFactory* factory) {
	factory->RegisterRoute(RouteBasic::GetId(),
		[]() -> RouteMod* { return new RouteBasic(); },
		[](RouteMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModRoutes(RouteFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModStations() {
	static vector<string> mods = { StationBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModStations(StationFactory* factory) {
	factory->RegisterStation(StationBasic::GetId(),
		[]() -> StationMod* { return new StationBasic(); },
		[](StationMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModStations(StationFactory* factory) {
	factory->CleanTemp();
}

extern "C" __declspec(dllexport) void* GetModVehicles() {
	static vector<string> mods = { VehicleBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModVehicles(VehicleFactory* factory) {
	factory->RegisterVehicle(VehicleBasic::GetId(),
		[]() -> VehicleMod* { return new VehicleBasic(); },
		[](VehicleMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModVehicles(VehicleFactory* factory) {
	factory->CleanTemp();
}
