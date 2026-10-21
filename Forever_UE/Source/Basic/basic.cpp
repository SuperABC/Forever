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
#include "player/weapon_factory.h"

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
#include "player/weapon_basic.h"

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

// 每个mod id自己声明依赖的mod id列表，见Source/Core/common/loader.h
// kModDependenciesSymbol注释。依赖id按字符串匹配、不区分concept(和IsModIdEnabled/
// Registry::CheckModRegistered同一套既有规则)。所有mod id都带自己concept的前缀(比如
// product_wheat和asset_wheat)，不同concept之间不会出现同一个字符串。不要声明成环(A依赖B、
// B又依赖A)：两个都启用后会互相锁定，配置界面里哪个都关不掉。
extern "C" __declspec(dllexport) void* GetModDependencies() {
	// Forever_Mod/Empty里的占位mod：会创建Script的mod(门/电梯/Scheduler/Job/Organization/Vehicle)
	// 默认用script_empty这个ScriptMod，住宅楼的占位房间槽用room_empty。
	static const string kScriptEmpty = "script_empty";
	static const string kRoomEmpty = "room_empty";

	static vector<pair<string, vector<string>>> dependencies = {
		{ RiverTerrain::GetId(), { MountainTerrain::GetId() } }, // 河从山体里流出来

		{ ResidenceZone::GetId(), { ResidenceBuilding::GetId() } },

		{ ResidenceBuilding::GetId(), { ResidenceComponent::GetId(), ResidenceRoom::GetId(), kRoomEmpty, kScriptEmpty } }, // 电梯脚本
		{ ShopBuilding::GetId(), { ShopComponent::GetId(), ShopRoom::GetId(), WarehouseRoom::GetId(), ParkingRoom::GetId(), kScriptEmpty } }, // 大门脚本
		{ FactoryBuilding::GetId(), { FactoryComponent::GetId(), FactoryRoom::GetId(), ParkingRoom::GetId() } },
		{ TrainStationBuilding::GetId(), { TrainStationComponent::GetId(), TrainStationRoom::GetId() } },
		{ AirportBuilding::GetId(), { AirportComponent::GetId(), AirportRoom::GetId() } },
		{ MoharBuilding::GetId(), { ResidenceComponent::GetId(), ResidenceRoom::GetId() } },

		{ ResidenceComponent::GetId(), { ResidenceRoom::GetId() } },
		{ ShopComponent::GetId(), { ShopRoom::GetId(), WarehouseRoom::GetId(), ParkingRoom::GetId() } },
		{ FactoryComponent::GetId(), { FactoryRoom::GetId(), ParkingRoom::GetId() } },
		{ TrainStationComponent::GetId(), { TrainStationRoom::GetId() } },
		{ AirportComponent::GetId(), { AirportRoom::GetId() } },

		{ ShopRoom::GetId(), { kScriptEmpty } }, // 房间门脚本

		{ SchedulerBasic::GetId(), { kScriptEmpty } },
		{ ShopSalerJob::GetId(), { ShopRoom::GetId(), kScriptEmpty } },
		{ ShopOrganization::GetId(), { ShopSalerJob::GetId(), kScriptEmpty } },

		{ FarmManufacture::GetId(), { WheatStorage::GetId(), WheatProduct::GetId() } },
		{ RanchManufacture::GetId(), { BeefStorage::GetId(), BeefProduct::GetId() } },
		{ FoodFactoryManufacture::GetId(), { BurgerStorage::GetId(), BurgerProduct::GetId(), WheatProduct::GetId(), BeefProduct::GetId() } }, // 小麦+牛肉做汉堡
		{ WheatStorage::GetId(), { WheatProduct::GetId() } },
		{ BeefStorage::GetId(), { BeefProduct::GetId() } },
		{ BurgerStorage::GetId(), { BurgerProduct::GetId() } },

		{ BusRoute::GetId(), { BusStation::GetId(), BusVehicle::GetId() } },
		{ TrainRoute::GetId(), { TrainStation::GetId(), TrainVehicle::GetId() } },
		{ AirRoute::GetId(), { AirStation::GetId(), PlaneVehicle::GetId() } },
		{ BusStation::GetId(), { BusVehicle::GetId() } },
		{ TrainStation::GetId(), { TrainVehicle::GetId(), TrainStationBuilding::GetId() } }, // 火车站建在火车站楼里
		{ AirStation::GetId(), { PlaneVehicle::GetId(), AirportBuilding::GetId() } }, // 机场站建在机场楼里
		// 每辆车都会创建自己的Script。
		{ VehicleBasic::GetId(), { kScriptEmpty } },
		{ BusVehicle::GetId(), { kScriptEmpty } },
		{ TrainVehicle::GetId(), { kScriptEmpty } },
		{ PlaneVehicle::GetId(), { kScriptEmpty } },

		{ PistolWeapon::GetId(), { PistolAmmoAsset::GetId() } }, // 换弹消耗的备弹
		{ RifleWeapon::GetId(), { RifleAmmoAsset::GetId() } },
		{ PistolWeaponAsset::GetId(), { PistolWeapon::GetId() } }, // 在肩膀上激活时装备的武器(AssetMod::weaponType)
		{ RifleWeaponAsset::GetId(), { RifleWeapon::GetId() } },
	};
	return (void*)&dependencies;
}

extern "C" __declspec(dllexport) void* GetModTerrains() {
	static vector<string> mods = { OceanTerrain::GetId(), MountainTerrain::GetId(), RiverTerrain::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModTerrains(TerrainFactory* factory) {
	factory->RegisterTerrain(OceanTerrain::GetId(),
		[](const std::string&) -> TerrainMod* { return new OceanTerrain(); },
		[](TerrainMod* m) { delete m; });
	factory->RegisterTerrain(MountainTerrain::GetId(),
		[](const std::string&) -> TerrainMod* { return new MountainTerrain(); },
		[](TerrainMod* m) { delete m; });
	factory->RegisterTerrain(RiverTerrain::GetId(),
		[](const std::string&) -> TerrainMod* { return new RiverTerrain(); },
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
	static vector<string> mods = { ResidenceBuilding::GetId(), ShopBuilding::GetId(), FactoryBuilding::GetId(),
		TrainStationBuilding::GetId(), AirportBuilding::GetId(), ParkBuilding::GetId(),
		MoharBuilding::GetId() };
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
	factory->RegisterBuilding(TrainStationBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new TrainStationBuilding(); },
		[](BuildingMod* m) { delete m; },
		&TrainStationBuilding::RandomAcreage, &TrainStationBuilding::GetAcreageMin, &TrainStationBuilding::GetAcreageMax,
		&TrainStationBuilding::GetPower, &TrainStationBuilding::Assign);
	factory->RegisterBuilding(AirportBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new AirportBuilding(); },
		[](BuildingMod* m) { delete m; },
		&AirportBuilding::RandomAcreage, &AirportBuilding::GetAcreageMin, &AirportBuilding::GetAcreageMax,
		&AirportBuilding::GetPower, &AirportBuilding::Assign);
	factory->RegisterBuilding(ParkBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new ParkBuilding(); },
		[](BuildingMod* m) { delete m; },
		&ParkBuilding::RandomAcreage, &ParkBuilding::GetAcreageMin, &ParkBuilding::GetAcreageMax,
		&ParkBuilding::GetPower, &ParkBuilding::Assign);
	factory->RegisterBuilding(MoharBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new MoharBuilding(); },
		[](BuildingMod* m) { delete m; },
		&MoharBuilding::RandomAcreage, &MoharBuilding::GetAcreageMin, &MoharBuilding::GetAcreageMax,
		&MoharBuilding::GetPower, &MoharBuilding::Assign);
}
extern "C" __declspec(dllexport) void FinishModBuildings(BuildingFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModComponents() {
	static vector<string> mods = { ResidenceComponent::GetId(), ShopComponent::GetId(), FactoryComponent::GetId(),
		TrainStationComponent::GetId(), AirportComponent::GetId() };
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
	factory->RegisterComponent(TrainStationComponent::GetId(),
		[](const std::string&) -> ComponentMod* { return new TrainStationComponent(); },
		[](ComponentMod* m) { delete m; });
	factory->RegisterComponent(AirportComponent::GetId(),
		[](const std::string&) -> ComponentMod* { return new AirportComponent(); },
		[](ComponentMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModComponents(ComponentFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModRooms() {
	static vector<string> mods = {
		ResidenceRoom::GetId(), ShopRoom::GetId(), WarehouseRoom::GetId(), ParkingRoom::GetId(), FactoryRoom::GetId(),
		TrainStationRoom::GetId(), AirportRoom::GetId()
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
	factory->RegisterRoom(TrainStationRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new TrainStationRoom(); },
		[](RoomMod* m) { delete m; });
	factory->RegisterRoom(AirportRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new AirportRoom(); },
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
		[](SchedulerMod* m) { delete m; },
		&SchedulerBasic::GetPower);
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
	static vector<string> mods = { WheatProduct::GetId(), BeefProduct::GetId(), BurgerProduct::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModProducts(ProductFactory* factory) {
	factory->RegisterProduct(WheatProduct::GetId(),
		[](const std::string&) -> ProductMod* { return new WheatProduct(); },
		[](ProductMod* m) { delete m; });
	factory->RegisterProduct(BeefProduct::GetId(),
		[](const std::string&) -> ProductMod* { return new BeefProduct(); },
		[](ProductMod* m) { delete m; });
	factory->RegisterProduct(BurgerProduct::GetId(),
		[](const std::string&) -> ProductMod* { return new BurgerProduct(); },
		[](ProductMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModProducts(ProductFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModStorages() {
	static vector<string> mods = { WheatStorage::GetId(), BeefStorage::GetId(), BurgerStorage::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModStorages(StorageFactory* factory) {
	factory->RegisterStorage(WheatStorage::GetId(),
		[](const std::string&) -> StorageMod* { return new WheatStorage(); },
		[](StorageMod* m) { delete m; });
	factory->RegisterStorage(BeefStorage::GetId(),
		[](const std::string&) -> StorageMod* { return new BeefStorage(); },
		[](StorageMod* m) { delete m; });
	factory->RegisterStorage(BurgerStorage::GetId(),
		[](const std::string&) -> StorageMod* { return new BurgerStorage(); },
		[](StorageMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModStorages(StorageFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModManufactures() {
	static vector<string> mods = { FarmManufacture::GetId(), RanchManufacture::GetId(), FoodFactoryManufacture::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModManufactures(ManufactureFactory* factory) {
	factory->RegisterManufacture(FarmManufacture::GetId(),
		[](const std::string&) -> ManufactureMod* { return new FarmManufacture(); },
		[](ManufactureMod* m) { delete m; });
	factory->RegisterManufacture(RanchManufacture::GetId(),
		[](const std::string&) -> ManufactureMod* { return new RanchManufacture(); },
		[](ManufactureMod* m) { delete m; });
	factory->RegisterManufacture(FoodFactoryManufacture::GetId(),
		[](const std::string&) -> ManufactureMod* { return new FoodFactoryManufacture(); },
		[](ManufactureMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModManufactures(ManufactureFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModRoutes() {
	static vector<string> mods = { BusRoute::GetId(), TrainRoute::GetId(), AirRoute::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRoutes(RouteFactory* factory) {
	factory->RegisterRoute(BusRoute::GetId(),
		[](const std::string&) -> RouteMod* { return new BusRoute(); },
		[](RouteMod* m) { delete m; });
	factory->RegisterRoute(TrainRoute::GetId(),
		[](const std::string&) -> RouteMod* { return new TrainRoute(); },
		[](RouteMod* m) { delete m; });
	factory->RegisterRoute(AirRoute::GetId(),
		[](const std::string&) -> RouteMod* { return new AirRoute(); },
		[](RouteMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModRoutes(RouteFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModStations() {
	static vector<string> mods = { BusStation::GetId(), TrainStation::GetId(), AirStation::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModStations(StationFactory* factory) {
	factory->RegisterStation(BusStation::GetId(),
		[](const std::string&) -> StationMod* { return new BusStation(); },
		[](StationMod* m) { delete m; },
		&BusStation::AssignRoads);
	factory->RegisterStation(TrainStation::GetId(),
		[](const std::string&) -> StationMod* { return new TrainStation(); },
		[](StationMod* m) { delete m; },
		&TrainStation::AssignRoads);
	factory->RegisterStation(AirStation::GetId(),
		[](const std::string&) -> StationMod* { return new AirStation(); },
		[](StationMod* m) { delete m; },
		&AirStation::AssignRoads);
}
extern "C" __declspec(dllexport) void FinishModStations(StationFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModVehicles() {
	static vector<string> mods = { VehicleBasic::GetId(), BusVehicle::GetId(), TrainVehicle::GetId(), PlaneVehicle::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModVehicles(VehicleFactory* factory) {
	factory->RegisterVehicle(VehicleBasic::GetId(),
		[](const std::string&) -> VehicleMod* { return new VehicleBasic(); },
		[](VehicleMod* m) { delete m; });
	factory->RegisterVehicle(BusVehicle::GetId(),
		[](const std::string&) -> VehicleMod* { return new BusVehicle(); },
		[](VehicleMod* m) { delete m; });
	factory->RegisterVehicle(TrainVehicle::GetId(),
		[](const std::string&) -> VehicleMod* { return new TrainVehicle(); },
		[](VehicleMod* m) { delete m; });
	factory->RegisterVehicle(PlaneVehicle::GetId(),
		[](const std::string&) -> VehicleMod* { return new PlaneVehicle(); },
		[](VehicleMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModVehicles(VehicleFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModAssets() {
	static vector<string> mods = { ContainerAsset::GetId(), WheatAsset::GetId(), BeefAsset::GetId(),
		BurgerAsset::GetId(), PistolAmmoAsset::GetId(), RifleAmmoAsset::GetId(),
		PistolWeaponAsset::GetId(), RifleWeaponAsset::GetId(),
		GrenadeAsset::GetId(), SmokeGrenadeAsset::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModAssets(AssetFactory* factory) {
	factory->RegisterAsset(ContainerAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new ContainerAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(WheatAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new WheatAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(BeefAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new BeefAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(BurgerAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new BurgerAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(PistolAmmoAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new PistolAmmoAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(RifleAmmoAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new RifleAmmoAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(PistolWeaponAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new PistolWeaponAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(RifleWeaponAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new RifleWeaponAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(GrenadeAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new GrenadeAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(SmokeGrenadeAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new SmokeGrenadeAsset(); },
		[](AssetMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModAssets(AssetFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModApps() {
	static vector<string> mods = { BounceApp::GetId(), NotesApp::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModApps(AppFactory* factory) {
	factory->RegisterApp(BounceApp::GetId(),
		[](const std::string&) -> AppMod* { return new BounceApp(); },
		[](AppMod* m) { delete m; });
	factory->RegisterApp(NotesApp::GetId(),
		[](const std::string&) -> AppMod* { return new NotesApp(); },
		[](AppMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModApps(AppFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModPuzzles() {
	static vector<string> mods = { TetrisPuzzle::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModPuzzles(PuzzleFactory* factory) {
	factory->RegisterPuzzle(TetrisPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new TetrisPuzzle(); },
		[](PuzzleMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModPuzzles(PuzzleFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModWeapons() {
	static vector<string> mods = { PistolWeapon::GetId(), RifleWeapon::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModWeapons(WeaponFactory* factory) {
	factory->RegisterWeapon(PistolWeapon::GetId(),
		[](const std::string&) -> WeaponMod* { return new PistolWeapon(); },
		[](WeaponMod* m) { delete m; });
	factory->RegisterWeapon(RifleWeapon::GetId(),
		[](const std::string&) -> WeaponMod* { return new RifleWeapon(); },
		[](WeaponMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModWeapons(WeaponFactory* factory) {
}
