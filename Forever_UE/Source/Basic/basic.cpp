#include "map/terrain_factory.h"
#include "map/roadnet_factory.h"
#include "map/zone_factory.h"
#include "map/building_factory.h"
#include "map/component_factory.h"
#include "map/room_factory.h"
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
#include "player/asset_factory.h"
#include "player/app_factory.h"
#include "player/puzzle_factory.h"

#include "map/terrain_basic.h"
#include "map/roadnet_basic.h"
#include "map/zone_basic.h"
#include "map/building_basic.h"
#include "map/component_basic.h"
#include "map/room_basic.h"
#include "populace/name_chinese.h"
#include "populace/scheduler_basic.h"
#include "society/job_basic.h"
#include "society/organization_basic.h"
#include "story/script_basic.h"
#include "industry/product_basic.h"
#include "industry/storage_basic.h"
#include "industry/manufacture_basic.h"
#include "traffic/route_basic.h"
#include "traffic/station_basic.h"
#include "traffic/vehicle_basic.h"
#include "player/asset_basic.h"
#include "player/app_basic.h"
#include "player/puzzle_basic.h"

#pragma comment(lib, "Dependence.lib")


// Basic编译为DynamicLibrary(Basic.dll),是内置的默认Mod集合——和Forever_Mod/Test、
// Forever_Mod/Wxdj地位相同,由Config/ModLoader在运行时通过本文件导出的20组
// GetMod<Concept>/RegisterMod<Concept>/FinishMod<Concept>符号扫描加载,不会静态链进
// Forever.Build.cs。详见 Source/Basic/README.md。
//
// 每个concept统一只对应一份Source/Basic/<domain>/<concept>_basic.h/.cpp,阶段3是trivial
// 占位XxxBasic,阶段4按系统迁移进度换成旧工程真正的默认内容,完整对照表见
// Source/Dependence/README.md 的20个concept对照表。
//
// map域的Zone/Building/Room/Component这4个文件里可能装不止一个具体类型（同一个concept下
// 不同业务场景各一个类，如ResidenceBuilding/ShopBuilding/FactoryBuilding都在
// building_basic.h/.cpp里）——和terrain_basic.h/.cpp里OceanTerrain/MountainTerrain合并
// 的方式一样，不再按"这个类型是哪种业务场景(residence/shop/factory)"拆文件，见
// map/building_basic.md/room_basic.md/component_basic.md。Room额外多了个WarehouseRoom/
// ParkingRoom(装在room_basic.h里，Factory的地下室也复用ParkingRoom)，这两个不是独立的
// building类型，只是Shop/Factory布局内部用到的辅助room。
//
// society域这次迁移了Job/Organization（不要Calendar）：job_basic.h/.cpp里的JobBasic占位
// 类换成了真正的ShopSalerJob，organization_basic.h/.cpp里的OrganizationBasic占位类换成了
// 真正的ShopOrganization，见Source/Core/society/job.md/organization.md。

using namespace std;

extern "C" __declspec(dllexport) void* GetModTerrains() {
	static vector<string> mods = { OceanTerrain::GetId(), MountainTerrain::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModTerrains(TerrainFactory* factory) {
	factory->RegisterTerrain(OceanTerrain::GetId(),
		[](const std::string&) -> TerrainMod* { return new OceanTerrain(); },
		[](TerrainMod* m) { delete m; });
	factory->RegisterTerrain(MountainTerrain::GetId(),
		[](const std::string&) -> TerrainMod* { return new MountainTerrain(); },
		[](TerrainMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModTerrains(TerrainFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModRoadnets() {
	static vector<string> mods = { JingRoadnet::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRoadnets(RoadnetFactory* factory) {
	factory->RegisterRoadnet(JingRoadnet::GetId(),
		[](const std::string&) -> RoadnetMod* { return new JingRoadnet(); },
		[](RoadnetMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModRoadnets(RoadnetFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModZones() {
	static vector<string> mods = { ResidenceZone::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModZones(ZoneFactory* factory) {
	factory->RegisterZone(ResidenceZone::GetId(),
		[](const std::string&) -> ZoneMod* { return new ResidenceZone(); },
		[](ZoneMod* m) { delete m; },
		&ResidenceZone::Assign);
}
extern "C" __declspec(dllexport) void FinishModZones(ZoneFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModBuildings() {
	static vector<string> mods = { ResidenceBuilding::GetId(), ShopBuilding::GetId(), FactoryBuilding::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModBuildings(BuildingFactory* factory) {
	factory->RegisterBuilding(ResidenceBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new ResidenceBuilding(); },
		[](BuildingMod* m) { delete m; },
		&ResidenceBuilding::RandomAcreage, &ResidenceBuilding::GetAcreageMin, &ResidenceBuilding::GetAcreageMax,
		&ResidenceBuilding::GetPower, &ResidenceBuilding::Assign);
	factory->RegisterBuilding(ShopBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new ShopBuilding(); },
		[](BuildingMod* m) { delete m; },
		&ShopBuilding::RandomAcreage, &ShopBuilding::GetAcreageMin, &ShopBuilding::GetAcreageMax,
		&ShopBuilding::GetPower, &ShopBuilding::Assign);
	factory->RegisterBuilding(FactoryBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new FactoryBuilding(); },
		[](BuildingMod* m) { delete m; },
		&FactoryBuilding::RandomAcreage, &FactoryBuilding::GetAcreageMin, &FactoryBuilding::GetAcreageMax,
		&FactoryBuilding::GetPower, &FactoryBuilding::Assign);
}
extern "C" __declspec(dllexport) void FinishModBuildings(BuildingFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModComponents() {
	static vector<string> mods = { ResidenceComponent::GetId(), ShopComponent::GetId(), FactoryComponent::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModComponents(ComponentFactory* factory) {
	factory->RegisterComponent(ResidenceComponent::GetId(),
		[](const std::string&) -> ComponentMod* { return new ResidenceComponent(); },
		[](ComponentMod* m) { delete m; });
	factory->RegisterComponent(ShopComponent::GetId(),
		[](const std::string&) -> ComponentMod* { return new ShopComponent(); },
		[](ComponentMod* m) { delete m; });
	factory->RegisterComponent(FactoryComponent::GetId(),
		[](const std::string&) -> ComponentMod* { return new FactoryComponent(); },
		[](ComponentMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModComponents(ComponentFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModRooms() {
	static vector<string> mods = {
		ResidenceRoom::GetId(), ShopRoom::GetId(), WarehouseRoom::GetId(), ParkingRoom::GetId(), FactoryRoom::GetId()
	};
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRooms(RoomFactory* factory) {
	factory->RegisterRoom(ResidenceRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new ResidenceRoom(); },
		[](RoomMod* m) { delete m; });
	factory->RegisterRoom(ShopRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new ShopRoom(); },
		[](RoomMod* m) { delete m; });
	factory->RegisterRoom(WarehouseRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new WarehouseRoom(); },
		[](RoomMod* m) { delete m; });
	factory->RegisterRoom(ParkingRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new ParkingRoom(); },
		[](RoomMod* m) { delete m; });
	factory->RegisterRoom(FactoryRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new FactoryRoom(); },
		[](RoomMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModRooms(RoomFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModNames() {
	static vector<string> mods = { ChineseName::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModNames(NameFactory* factory) {
	factory->RegisterName(ChineseName::GetId(),
		[](const std::string&) -> NameMod* { return new ChineseName(); },
		[](NameMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModNames(NameFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModSchedulers() {
	static vector<string> mods = { SchedulerBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModSchedulers(SchedulerFactory* factory) {
	factory->RegisterScheduler(SchedulerBasic::GetId(),
		[](const std::string&) -> SchedulerMod* { return new SchedulerBasic(); },
		[](SchedulerMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModSchedulers(SchedulerFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModJobs() {
	static vector<string> mods = { ShopSalerJob::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModJobs(JobFactory* factory) {
	factory->RegisterJob(ShopSalerJob::GetId(),
		[](const std::string&) -> JobMod* { return new ShopSalerJob(); },
		[](JobMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModJobs(JobFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModOrganizations() {
	static vector<string> mods = { ShopOrganization::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModOrganizations(OrganizationFactory* factory) {
	factory->RegisterOrganization(ShopOrganization::GetId(),
		[](const std::string&) -> OrganizationMod* { return new ShopOrganization(); },
		[](OrganizationMod* m) { delete m; },
		&ShopOrganization::GetPower);
}
extern "C" __declspec(dllexport) void FinishModOrganizations(OrganizationFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModScripts() {
	static vector<string> mods = { ScriptBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModScripts(ScriptFactory* factory) {
	factory->RegisterScript(ScriptBasic::GetId(),
		[](const std::string&) -> ScriptMod* { return new ScriptBasic(); },
		[](ScriptMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModScripts(ScriptFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModProducts() {
	static vector<string> mods = { ProductBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModProducts(ProductFactory* factory) {
	factory->RegisterProduct(ProductBasic::GetId(),
		[](const std::string&) -> ProductMod* { return new ProductBasic(); },
		[](ProductMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModProducts(ProductFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModStorages() {
	static vector<string> mods = { StorageBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModStorages(StorageFactory* factory) {
	factory->RegisterStorage(StorageBasic::GetId(),
		[](const std::string&) -> StorageMod* { return new StorageBasic(); },
		[](StorageMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModStorages(StorageFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModManufactures() {
	static vector<string> mods = { ManufactureBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModManufactures(ManufactureFactory* factory) {
	factory->RegisterManufacture(ManufactureBasic::GetId(),
		[](const std::string&) -> ManufactureMod* { return new ManufactureBasic(); },
		[](ManufactureMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModManufactures(ManufactureFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModRoutes() {
	static vector<string> mods = { RouteBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRoutes(RouteFactory* factory) {
	factory->RegisterRoute(RouteBasic::GetId(),
		[](const std::string&) -> RouteMod* { return new RouteBasic(); },
		[](RouteMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModRoutes(RouteFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModStations() {
	static vector<string> mods = { StationBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModStations(StationFactory* factory) {
	factory->RegisterStation(StationBasic::GetId(),
		[](const std::string&) -> StationMod* { return new StationBasic(); },
		[](StationMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModStations(StationFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModVehicles() {
	static vector<string> mods = { VehicleBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModVehicles(VehicleFactory* factory) {
	factory->RegisterVehicle(VehicleBasic::GetId(),
		[](const std::string&) -> VehicleMod* { return new VehicleBasic(); },
		[](VehicleMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModVehicles(VehicleFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModAssets() {
	static vector<string> mods = { AssetBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModAssets(AssetFactory* factory) {
	factory->RegisterAsset(AssetBasic::GetId(),
		[](const std::string&) -> AssetMod* { return new AssetBasic(); },
		[](AssetMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModAssets(AssetFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModApps() {
	static vector<string> mods = { AppBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModApps(AppFactory* factory) {
	factory->RegisterApp(AppBasic::GetId(),
		[](const std::string&) -> AppMod* { return new AppBasic(); },
		[](AppMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModApps(AppFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModPuzzles() {
	static vector<string> mods = { PuzzleBasic::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModPuzzles(PuzzleFactory* factory) {
	factory->RegisterPuzzle(PuzzleBasic::GetId(),
		[](const std::string&) -> PuzzleMod* { return new PuzzleBasic(); },
		[](PuzzleMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModPuzzles(PuzzleFactory* factory) {
}
