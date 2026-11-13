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

#include "map/terrain_mountain.h"
#include "map/terrain_ocean.h"
#include "map/terrain_river.h"
#include "map/roadnet_jing.h"
#include "map/roadnet_checkboard.h"
#include "map/zone_residence_high.h"
#include "map/zone_residence_low.h"
#include "map/zone_residence_middle.h"
#include "map/building_airport.h"
#include "map/building_commerce_high.h"
#include "map/building_commerce_low.h"
#include "map/building_commerce_middle.h"
#include "map/building_industry_high.h"
#include "map/building_industry_low.h"
#include "map/building_industry_middle.h"
#include "map/building_mohar.h"
#include "map/building_office_high.h"
#include "map/building_office_low.h"
#include "map/building_office_middle.h"
#include "map/building_park.h"
#include "map/building_residence_high.h"
#include "map/building_residence_low.h"
#include "map/building_residence_middle.h"
#include "map/building_train_station.h"
#include "map/component_airport.h"
#include "map/component_manufacture.h"
#include "map/component_office.h"
#include "map/component_residence.h"
#include "map/component_shop.h"
#include "map/component_train_station.h"
#include "map/room_airport.h"
#include "map/room_farm.h"
#include "map/room_food_factory.h"
#include "map/room_office.h"
#include "map/room_parking.h"
#include "map/room_ranch.h"
#include "map/room_residence.h"
#include "map/room_restaurant.h"
#include "map/room_shop.h"
#include "map/room_train_station.h"
#include "map/room_warehouse.h"
#include "populace/name_chinese.h"
#include "populace/scheduler_basic.h"
#include "society/job_factory_worker.h"
#include "society/job_shop_saler.h"
#include "society/job_warehouse_keeper.h"
#include "society/organization_manufacture.h"
#include "society/organization_shop.h"
#include "story/script_basic.h"
#include "industry/product_beef.h"
#include "industry/product_burger.h"
#include "industry/product_wheat.h"
#include "industry/storage_beef.h"
#include "industry/storage_burger.h"
#include "industry/storage_shop.h"
#include "industry/storage_wheat.h"
#include "industry/manufacture_farm.h"
#include "industry/manufacture_food.h"
#include "industry/manufacture_ranch.h"
#include "traffic/route_air.h"
#include "traffic/route_bus.h"
#include "traffic/route_train.h"
#include "traffic/station_air.h"
#include "traffic/station_bus.h"
#include "traffic/station_train.h"
#include "traffic/vehicle_sedan.h"
#include "traffic/vehicle_bus.h"
#include "traffic/vehicle_plane.h"
#include "traffic/vehicle_train.h"
#include "traffic/vehicle_truck.h"
#include "player/asset_ammo_pistol.h"
#include "player/asset_ammo_rifle.h"
#include "player/asset_ammo_shotgun.h"
#include "player/asset_ammo_sniper.h"
#include "player/asset_beef.h"
#include "player/asset_burger.h"
#include "player/asset_camera.h"
#include "player/asset_container.h"
#include "player/asset_grenade.h"
#include "player/asset_pistol.h"
#include "player/asset_rifle.h"
#include "player/asset_shotgun.h"
#include "player/asset_smoke_grenade.h"
#include "player/asset_sniper.h"
#include "player/asset_wheat.h"
#include "player/app_bounce.h"
#include "player/app_notes.h"
#include "player/puzzle_tetris.h"
#include "player/puzzle_anykey.h"
#include "player/puzzle_poker.h"
#include "player/puzzle_burger.h"
#include "player/puzzle_miner.h"
#include "player/puzzle_tower.h"
#include "player/puzzle_chase.h"
#include "player/puzzle_rhythm.h"
#include "player/puzzle_tubes.h"
#include "player/puzzle_coins.h"
#include "player/puzzle_dance.h"
#include "player/puzzle_dodge.h"
#include "player/weapon_camera.h"
#include "player/weapon_pistol.h"
#include "player/weapon_rifle.h"
#include "player/weapon_shotgun.h"
#include "player/weapon_sniper.h"

#pragma comment(lib, "Dependence.lib")


using namespace std;

// 每个mod id声明依赖的mod id列表，约束见basic.md。
extern "C" __declspec(dllexport) void* GetModDependencies() {
	// Forever_Mod/Empty里的占位mod，见basic.md。
	static const string kScriptEmpty = "script_empty";
	static const string kRoomEmpty = "room_empty";

	static vector<pair<string, vector<string>>> dependencies = {
		{ RiverTerrain::GetId(), { MountainTerrain::GetId() } }, // 河从山体里流出来

		{ ResidenceHighZone::GetId(), { ResidenceHighBuilding::GetId() } },
		{ ResidenceMiddleZone::GetId(), { ResidenceMiddleBuilding::GetId() } },
		{ ResidenceLowZone::GetId(), { ResidenceLowBuilding::GetId() } },

		{ ResidenceHighBuilding::GetId(), { ResidenceComponent::GetId(), ResidenceRoom::GetId(), ParkingRoom::GetId(), kScriptEmpty } }, // 电梯脚本；地下室停车库
		{ ResidenceMiddleBuilding::GetId(), { ResidenceComponent::GetId(), ResidenceRoom::GetId(), kRoomEmpty, kScriptEmpty } }, // 电梯脚本
		{ ResidenceLowBuilding::GetId(), { ResidenceComponent::GetId(), ResidenceRoom::GetId(), kRoomEmpty, kScriptEmpty } }, // 电梯脚本
		{ CommerceHighBuilding::GetId(), { ShopComponent::GetId(), ShopRoom::GetId(), RestaurantRoom::GetId(), WarehouseRoom::GetId(), kRoomEmpty, kScriptEmpty } }, // 电梯脚本
		{ CommerceMiddleBuilding::GetId(), { ShopComponent::GetId(), ShopRoom::GetId(), WarehouseRoom::GetId(), ParkingRoom::GetId(), kScriptEmpty } }, // 大门脚本
		{ CommerceLowBuilding::GetId(), { ShopComponent::GetId(), ShopRoom::GetId(), WarehouseRoom::GetId(), ParkingRoom::GetId(), kScriptEmpty } }, // 大门脚本
		{ IndustryHighBuilding::GetId(), { ManufactureComponent::GetId(), FarmRoom::GetId(), RanchRoom::GetId(), FoodFactoryRoom::GetId(), ParkingRoom::GetId() } },
		{ IndustryMiddleBuilding::GetId(), { ManufactureComponent::GetId(), FarmRoom::GetId(), RanchRoom::GetId(), FoodFactoryRoom::GetId(), ParkingRoom::GetId() } },
		{ IndustryLowBuilding::GetId(), { ManufactureComponent::GetId(), FarmRoom::GetId(), RanchRoom::GetId(), FoodFactoryRoom::GetId(), ParkingRoom::GetId() } },
		{ OfficeHighBuilding::GetId(), { OfficeComponent::GetId(), OfficeRoom::GetId(), ParkingRoom::GetId(), kRoomEmpty } }, // 地下室停车库；占位房间槽
		{ OfficeMiddleBuilding::GetId(), { OfficeComponent::GetId(), OfficeRoom::GetId(), kRoomEmpty, kScriptEmpty } }, // 电梯脚本
		{ OfficeLowBuilding::GetId(), { OfficeComponent::GetId(), OfficeRoom::GetId(), kRoomEmpty, kScriptEmpty } }, // 电梯脚本
		{ TrainStationBuilding::GetId(), { TrainStationComponent::GetId(), TrainStationRoom::GetId(), kScriptEmpty } }, // 电梯脚本
		{ AirportBuilding::GetId(), { AirportComponent::GetId(), AirportRoom::GetId() } },
		{ MoharBuilding::GetId(), { ResidenceComponent::GetId(), ResidenceRoom::GetId() } },

		{ ShopRoom::GetId(), { kScriptEmpty } }, // 房间门脚本

		{ SchedulerBasic::GetId(), { kScriptEmpty } },
		{ ShopSalerJob::GetId(), { kScriptEmpty } },
		{ FactoryWorkerJob::GetId(), { kScriptEmpty } },
		{ ShopOrganization::GetId(), { ShopComponent::GetId(), ShopRoom::GetId(), WarehouseRoom::GetId(), ShopSalerJob::GetId(), WarehouseKeeperJob::GetId(), kScriptEmpty } }, // 按component/room类型生成岗位
		{ ManufactureOrganization::GetId(), { ManufactureComponent::GetId(), FarmRoom::GetId(), RanchRoom::GetId(), FoodFactoryRoom::GetId(), FactoryWorkerJob::GetId() } },

		// 托管型：Manufacture只从房间的manufactureTypes生成、Storage只从房间的storageType生成，没有房间就不会出现，所以它们依赖房间。
		{ FarmManufacture::GetId(), { FarmRoom::GetId(), WheatProduct::GetId() } },
		{ RanchManufacture::GetId(), { RanchRoom::GetId(), BeefProduct::GetId() } },
		{ FoodFactoryManufacture::GetId(), { FoodFactoryRoom::GetId(), BurgerProduct::GetId() } },
		{ ShopStorage::GetId(), { WarehouseRoom::GetId() } }, // storageType="storage_shop"
		{ BurgerProduct::GetId(), { WheatProduct::GetId(), BeefProduct::GetId() } }, // 配方：小麦+牛肉做汉堡
		// 仓库和产品靠分类标签(categories)匹配，不按id引用。
		{ WheatStorage::GetId(), { WheatProduct::GetId() } },
		{ BeefStorage::GetId(), { BeefProduct::GetId() } },
		{ BurgerStorage::GetId(), { BurgerProduct::GetId() } },

		// 线路按stationType匹配站点、按vehicleType(id)生成车辆。
		{ BusRoute::GetId(), { BusStation::GetId(), BusVehicle::GetId() } },
		{ TrainRoute::GetId(), { TrainStation::GetId(), TrainVehicle::GetId() } },
		{ AirRoute::GetId(), { AirStation::GetId(), PlaneVehicle::GetId() } },
		// 楼里的站点只从带stationMod的楼生成(Traffic::InitStations)，没有楼就不会出现站点，所以站点依赖楼；楼没有站点mod只是少个站点。公交站是路边站，不依赖楼。
		{ TrainStation::GetId(), { TrainStationBuilding::GetId() } },
		{ AirStation::GetId(), { AirportBuilding::GetId() } },
		// 每辆车都会创建自己的Script。
		{ SedanVehicle::GetId(), { kScriptEmpty } },
		{ BusVehicle::GetId(), { kScriptEmpty } },
		{ TruckVehicle::GetId(), { kScriptEmpty } },
		{ TrainVehicle::GetId(), { kScriptEmpty } },
		{ PlaneVehicle::GetId(), { kScriptEmpty } },

		{ BurgerAsset::GetId(), { BurgerProduct::GetId() } }, // productType：商店零售价
		{ PistolWeapon::GetId(), { PistolAmmoAsset::GetId() } }, // 换弹消耗的备弹
		{ RifleWeapon::GetId(), { RifleAmmoAsset::GetId() } },
		{ ShotgunWeapon::GetId(), { ShotgunAmmoAsset::GetId() } },
		{ SniperWeapon::GetId(), { SniperAmmoAsset::GetId() } },
		{ PistolWeaponAsset::GetId(), { PistolWeapon::GetId() } }, // 在肩膀上激活时装备的武器(AssetMod::weaponType)
		{ RifleWeaponAsset::GetId(), { RifleWeapon::GetId() } },
		{ ShotgunWeaponAsset::GetId(), { ShotgunWeapon::GetId() } },
		{ SniperWeaponAsset::GetId(), { SniperWeapon::GetId() } },
		{ CameraWeaponAsset::GetId(), { CameraWeapon::GetId() } }, // 相机不用弹药，没有对应的弹药资产依赖
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
	static vector<string> mods = { JingRoadnet::GetId(), CheckboardRoadnet::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRoadnets(RoadnetFactory* factory) {
	factory->RegisterRoadnet(JingRoadnet::GetId(),
		[](const std::string& args) -> RoadnetMod* { return new JingRoadnet(args); },
		[](RoadnetMod* m) { delete m; });
	factory->RegisterRoadnet(CheckboardRoadnet::GetId(),
		[](const std::string& args) -> RoadnetMod* { return new CheckboardRoadnet(args); },
		[](RoadnetMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModRoadnets(RoadnetFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModZones() {
	static vector<string> mods = { ResidenceHighZone::GetId(), ResidenceMiddleZone::GetId(), ResidenceLowZone::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModZones(ZoneFactory* factory) {
	factory->RegisterZone(ResidenceHighZone::GetId(),
		[](const std::string&) -> ZoneMod* { return new ResidenceHighZone(); },
		[](ZoneMod* m) { delete m; },
		&ResidenceHighZone::Assign);
	factory->RegisterZone(ResidenceMiddleZone::GetId(),
		[](const std::string&) -> ZoneMod* { return new ResidenceMiddleZone(); },
		[](ZoneMod* m) { delete m; },
		&ResidenceMiddleZone::Assign);
	factory->RegisterZone(ResidenceLowZone::GetId(),
		[](const std::string&) -> ZoneMod* { return new ResidenceLowZone(); },
		[](ZoneMod* m) { delete m; },
		&ResidenceLowZone::Assign);
}
extern "C" __declspec(dllexport) void FinishModZones(ZoneFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModBuildings() {
	static vector<string> mods = {
		ResidenceHighBuilding::GetId(), ResidenceMiddleBuilding::GetId(), ResidenceLowBuilding::GetId(),
		CommerceHighBuilding::GetId(), CommerceMiddleBuilding::GetId(), CommerceLowBuilding::GetId(),
		IndustryHighBuilding::GetId(), IndustryMiddleBuilding::GetId(), IndustryLowBuilding::GetId(),
		OfficeHighBuilding::GetId(), OfficeMiddleBuilding::GetId(), OfficeLowBuilding::GetId(),
		TrainStationBuilding::GetId(), AirportBuilding::GetId(), ParkBuilding::GetId(),
		MoharBuilding::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModBuildings(BuildingFactory* factory) {
	factory->RegisterBuilding(ResidenceHighBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new ResidenceHighBuilding(); },
		[](BuildingMod* m) { delete m; },
		&ResidenceHighBuilding::RandomAcreage, &ResidenceHighBuilding::GetAcreageMin, &ResidenceHighBuilding::GetAcreageMax,
		&ResidenceHighBuilding::GetPower, &ResidenceHighBuilding::Assign);
	factory->RegisterBuilding(ResidenceMiddleBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new ResidenceMiddleBuilding(); },
		[](BuildingMod* m) { delete m; },
		&ResidenceMiddleBuilding::RandomAcreage, &ResidenceMiddleBuilding::GetAcreageMin, &ResidenceMiddleBuilding::GetAcreageMax,
		&ResidenceMiddleBuilding::GetPower, &ResidenceMiddleBuilding::Assign);
	factory->RegisterBuilding(ResidenceLowBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new ResidenceLowBuilding(); },
		[](BuildingMod* m) { delete m; },
		&ResidenceLowBuilding::RandomAcreage, &ResidenceLowBuilding::GetAcreageMin, &ResidenceLowBuilding::GetAcreageMax,
		&ResidenceLowBuilding::GetPower, &ResidenceLowBuilding::Assign);
	factory->RegisterBuilding(CommerceHighBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new CommerceHighBuilding(); },
		[](BuildingMod* m) { delete m; },
		&CommerceHighBuilding::RandomAcreage, &CommerceHighBuilding::GetAcreageMin, &CommerceHighBuilding::GetAcreageMax,
		&CommerceHighBuilding::GetPower, &CommerceHighBuilding::Assign);
	factory->RegisterBuilding(CommerceMiddleBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new CommerceMiddleBuilding(); },
		[](BuildingMod* m) { delete m; },
		&CommerceMiddleBuilding::RandomAcreage, &CommerceMiddleBuilding::GetAcreageMin, &CommerceMiddleBuilding::GetAcreageMax,
		&CommerceMiddleBuilding::GetPower, &CommerceMiddleBuilding::Assign);
	factory->RegisterBuilding(CommerceLowBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new CommerceLowBuilding(); },
		[](BuildingMod* m) { delete m; },
		&CommerceLowBuilding::RandomAcreage, &CommerceLowBuilding::GetAcreageMin, &CommerceLowBuilding::GetAcreageMax,
		&CommerceLowBuilding::GetPower, &CommerceLowBuilding::Assign);
	factory->RegisterBuilding(IndustryHighBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new IndustryHighBuilding(); },
		[](BuildingMod* m) { delete m; },
		&IndustryHighBuilding::RandomAcreage, &IndustryHighBuilding::GetAcreageMin, &IndustryHighBuilding::GetAcreageMax,
		&IndustryHighBuilding::GetPower, &IndustryHighBuilding::Assign);
	factory->RegisterBuilding(IndustryMiddleBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new IndustryMiddleBuilding(); },
		[](BuildingMod* m) { delete m; },
		&IndustryMiddleBuilding::RandomAcreage, &IndustryMiddleBuilding::GetAcreageMin, &IndustryMiddleBuilding::GetAcreageMax,
		&IndustryMiddleBuilding::GetPower, &IndustryMiddleBuilding::Assign);
	factory->RegisterBuilding(IndustryLowBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new IndustryLowBuilding(); },
		[](BuildingMod* m) { delete m; },
		&IndustryLowBuilding::RandomAcreage, &IndustryLowBuilding::GetAcreageMin, &IndustryLowBuilding::GetAcreageMax,
		&IndustryLowBuilding::GetPower, &IndustryLowBuilding::Assign);
	factory->RegisterBuilding(OfficeHighBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new OfficeHighBuilding(); },
		[](BuildingMod* m) { delete m; },
		&OfficeHighBuilding::RandomAcreage, &OfficeHighBuilding::GetAcreageMin, &OfficeHighBuilding::GetAcreageMax,
		&OfficeHighBuilding::GetPower, &OfficeHighBuilding::Assign);
	factory->RegisterBuilding(OfficeMiddleBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new OfficeMiddleBuilding(); },
		[](BuildingMod* m) { delete m; },
		&OfficeMiddleBuilding::RandomAcreage, &OfficeMiddleBuilding::GetAcreageMin, &OfficeMiddleBuilding::GetAcreageMax,
		&OfficeMiddleBuilding::GetPower, &OfficeMiddleBuilding::Assign);
	factory->RegisterBuilding(OfficeLowBuilding::GetId(),
		[](const std::string&) -> BuildingMod* { return new OfficeLowBuilding(); },
		[](BuildingMod* m) { delete m; },
		&OfficeLowBuilding::RandomAcreage, &OfficeLowBuilding::GetAcreageMin, &OfficeLowBuilding::GetAcreageMax,
		&OfficeLowBuilding::GetPower, &OfficeLowBuilding::Assign);
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
	static vector<string> mods = { ResidenceComponent::GetId(), ShopComponent::GetId(), ManufactureComponent::GetId(), OfficeComponent::GetId(),
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
	factory->RegisterComponent(ManufactureComponent::GetId(),
		[](const std::string&) -> ComponentMod* { return new ManufactureComponent(); },
		[](ComponentMod* m) { delete m; });
	factory->RegisterComponent(OfficeComponent::GetId(),
		[](const std::string&) -> ComponentMod* { return new OfficeComponent(); },
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
		ResidenceRoom::GetId(), OfficeRoom::GetId(), ShopRoom::GetId(), RestaurantRoom::GetId(), WarehouseRoom::GetId(), ParkingRoom::GetId(),
		FarmRoom::GetId(), RanchRoom::GetId(), FoodFactoryRoom::GetId(),
		TrainStationRoom::GetId(), AirportRoom::GetId()
	};
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModRooms(RoomFactory* factory) {
	factory->RegisterRoom(ResidenceRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new ResidenceRoom(); },
		[](RoomMod* m) { delete m; });
	factory->RegisterRoom(OfficeRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new OfficeRoom(); },
		[](RoomMod* m) { delete m; });
	factory->RegisterRoom(ShopRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new ShopRoom(); },
		[](RoomMod* m) { delete m; });
	factory->RegisterRoom(RestaurantRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new RestaurantRoom(); },
		[](RoomMod* m) { delete m; });
	factory->RegisterRoom(WarehouseRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new WarehouseRoom(); },
		[](RoomMod* m) { delete m; });
	factory->RegisterRoom(ParkingRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new ParkingRoom(); },
		[](RoomMod* m) { delete m; });
	factory->RegisterRoom(FarmRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new FarmRoom(); },
		[](RoomMod* m) { delete m; });
	factory->RegisterRoom(RanchRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new RanchRoom(); },
		[](RoomMod* m) { delete m; });
	factory->RegisterRoom(FoodFactoryRoom::GetId(),
		[](const std::string&) -> RoomMod* { return new FoodFactoryRoom(); },
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
	static vector<string> mods = { ShopSalerJob::GetId(), WarehouseKeeperJob::GetId(), FactoryWorkerJob::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModJobs(JobFactory* factory) {
	factory->RegisterJob(WarehouseKeeperJob::GetId(),
		[](const std::string&) -> JobMod* { return new WarehouseKeeperJob(); },
		[](JobMod* m) { delete m; });
	factory->RegisterJob(FactoryWorkerJob::GetId(),
		[](const std::string&) -> JobMod* { return new FactoryWorkerJob(); },
		[](JobMod* m) { delete m; });
	factory->RegisterJob(ShopSalerJob::GetId(),
		[](const std::string&) -> JobMod* { return new ShopSalerJob(); },
		[](JobMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModJobs(JobFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModOrganizations() {
	static vector<string> mods = { ShopOrganization::GetId(), ManufactureOrganization::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModOrganizations(OrganizationFactory* factory) {
	factory->RegisterOrganization(ManufactureOrganization::GetId(),
		[](const std::string&) -> OrganizationMod* { return new ManufactureOrganization(); },
		[](OrganizationMod* m) { delete m; },
		&ManufactureOrganization::GetPower, &ManufactureOrganization::GetRequirements);
	factory->RegisterOrganization(ShopOrganization::GetId(),
		[](const std::string&) -> OrganizationMod* { return new ShopOrganization(); },
		[](OrganizationMod* m) { delete m; },
		&ShopOrganization::GetPower, &ShopOrganization::GetRequirements);
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
	static vector<string> mods = { WheatStorage::GetId(), BeefStorage::GetId(), BurgerStorage::GetId(), ShopStorage::GetId() };
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
	factory->RegisterStorage(ShopStorage::GetId(),
		[](const std::string&) -> StorageMod* { return new ShopStorage(); },
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
	static vector<string> mods = { SedanVehicle::GetId(), BusVehicle::GetId(), TruckVehicle::GetId(), TrainVehicle::GetId(), PlaneVehicle::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModVehicles(VehicleFactory* factory) {
	factory->RegisterVehicle(SedanVehicle::GetId(),
		[](const std::string&) -> VehicleMod* { return new SedanVehicle(); },
		[](VehicleMod* m) { delete m; }, &SedanVehicle::GetCategory);
	factory->RegisterVehicle(BusVehicle::GetId(),
		[](const std::string&) -> VehicleMod* { return new BusVehicle(); },
		[](VehicleMod* m) { delete m; }, &BusVehicle::GetCategory);
	factory->RegisterVehicle(TruckVehicle::GetId(),
		[](const std::string&) -> VehicleMod* { return new TruckVehicle(); },
		[](VehicleMod* m) { delete m; }, &TruckVehicle::GetCategory);
	factory->RegisterVehicle(TrainVehicle::GetId(),
		[](const std::string&) -> VehicleMod* { return new TrainVehicle(); },
		[](VehicleMod* m) { delete m; }, &TrainVehicle::GetCategory);
	factory->RegisterVehicle(PlaneVehicle::GetId(),
		[](const std::string&) -> VehicleMod* { return new PlaneVehicle(); },
		[](VehicleMod* m) { delete m; }, &PlaneVehicle::GetCategory);
}
extern "C" __declspec(dllexport) void FinishModVehicles(VehicleFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModAssets() {
	static vector<string> mods = { ContainerAsset::GetId(), WheatAsset::GetId(), BeefAsset::GetId(),
		BurgerAsset::GetId(), PistolAmmoAsset::GetId(), RifleAmmoAsset::GetId(),
		ShotgunAmmoAsset::GetId(), SniperAmmoAsset::GetId(),
		PistolWeaponAsset::GetId(), RifleWeaponAsset::GetId(),
		ShotgunWeaponAsset::GetId(), SniperWeaponAsset::GetId(), CameraWeaponAsset::GetId(),
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
	factory->RegisterAsset(ShotgunAmmoAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new ShotgunAmmoAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(SniperAmmoAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new SniperAmmoAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(PistolWeaponAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new PistolWeaponAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(RifleWeaponAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new RifleWeaponAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(ShotgunWeaponAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new ShotgunWeaponAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(SniperWeaponAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new SniperWeaponAsset(); },
		[](AssetMod* m) { delete m; });
	factory->RegisterAsset(CameraWeaponAsset::GetId(),
		[](const std::string&) -> AssetMod* { return new CameraWeaponAsset(); },
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
	static vector<string> mods = { TetrisPuzzle::GetId(), AnyKeyPuzzle::GetId(), PokerPuzzle::GetId(), BurgerPuzzle::GetId(), MinerPuzzle::GetId(), TowerPuzzle::GetId(), ChasePuzzle::GetId(), RhythmPuzzle::GetId(), TubesPuzzle::GetId(), CoinsPuzzle::GetId(), DancePuzzle::GetId(), DodgePuzzle::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModPuzzles(PuzzleFactory* factory) {
	factory->RegisterPuzzle(TetrisPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new TetrisPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	factory->RegisterPuzzle(AnyKeyPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new AnyKeyPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	factory->RegisterPuzzle(DodgePuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new DodgePuzzle(); },
		[](PuzzleMod* m) { delete m; });
	factory->RegisterPuzzle(DancePuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new DancePuzzle(); },
		[](PuzzleMod* m) { delete m; });
	factory->RegisterPuzzle(CoinsPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new CoinsPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	factory->RegisterPuzzle(TubesPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new TubesPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	factory->RegisterPuzzle(RhythmPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new RhythmPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	factory->RegisterPuzzle(ChasePuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new ChasePuzzle(); },
		[](PuzzleMod* m) { delete m; });
	factory->RegisterPuzzle(TowerPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new TowerPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	factory->RegisterPuzzle(MinerPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new MinerPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	factory->RegisterPuzzle(BurgerPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new BurgerPuzzle(); },
		[](PuzzleMod* m) { delete m; });
	factory->RegisterPuzzle(PokerPuzzle::GetId(),
		[](const std::string&) -> PuzzleMod* { return new PokerPuzzle(); },
		[](PuzzleMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModPuzzles(PuzzleFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModWeapons() {
	static vector<string> mods = { PistolWeapon::GetId(), RifleWeapon::GetId(),
		ShotgunWeapon::GetId(), SniperWeapon::GetId(), CameraWeapon::GetId() };
	return (void*)&mods;
}
extern "C" __declspec(dllexport) void RegisterModWeapons(WeaponFactory* factory) {
	factory->RegisterWeapon(PistolWeapon::GetId(),
		[](const std::string&) -> WeaponMod* { return new PistolWeapon(); },
		[](WeaponMod* m) { delete m; });
	factory->RegisterWeapon(RifleWeapon::GetId(),
		[](const std::string&) -> WeaponMod* { return new RifleWeapon(); },
		[](WeaponMod* m) { delete m; });
	factory->RegisterWeapon(ShotgunWeapon::GetId(),
		[](const std::string&) -> WeaponMod* { return new ShotgunWeapon(); },
		[](WeaponMod* m) { delete m; });
	factory->RegisterWeapon(SniperWeapon::GetId(),
		[](const std::string&) -> WeaponMod* { return new SniperWeapon(); },
		[](WeaponMod* m) { delete m; });
	factory->RegisterWeapon(CameraWeapon::GetId(),
		[](const std::string&) -> WeaponMod* { return new CameraWeapon(); },
		[](WeaponMod* m) { delete m; });
}
extern "C" __declspec(dllexport) void FinishModWeapons(WeaponFactory* factory) {
}
