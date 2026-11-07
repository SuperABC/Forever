#pragma once

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

#include "common/loader.h"

#include <string>
#include <unordered_set>
#include <vector>


// 21个concept的mod注册表(全局单例)，设计见registry.md
class Registry {
public:
	static Registry& Get();

	// 补加载config里新出现的mod dll并刷新启用表，见registry.md
	void RefreshMods();

	Registry(const Registry&) = delete;
	Registry& operator=(const Registry&) = delete;

	// 只刷新"mod id -> 参数字符串"表，不重新加载dll
	void ReloadModArgs();

	// 任意一个Factory认得这个id即true(id不带concept前缀)
	bool CheckModRegistered(const std::string& id) const;

	TerrainFactory& GetTerrainFactory() { return terrainFactory; }
	RoadnetFactory& GetRoadnetFactory() { return roadnetFactory; }
	ZoneFactory& GetZoneFactory() { return zoneFactory; }
	BuildingFactory& GetBuildingFactory() { return buildingFactory; }
	ComponentFactory& GetComponentFactory() { return componentFactory; }
	RoomFactory& GetRoomFactory() { return roomFactory; }

	NameFactory& GetNameFactory() { return nameFactory; }
	SchedulerFactory& GetSchedulerFactory() { return schedulerFactory; }

	JobFactory& GetJobFactory() { return jobFactory; }
	OrganizationFactory& GetOrganizationFactory() { return organizationFactory; }

	ScriptFactory& GetScriptFactory() { return scriptFactory; }

	ProductFactory& GetProductFactory() { return productFactory; }
	StorageFactory& GetStorageFactory() { return storageFactory; }
	ManufactureFactory& GetManufactureFactory() { return manufactureFactory; }

	RouteFactory& GetRouteFactory() { return routeFactory; }
	StationFactory& GetStationFactory() { return stationFactory; }
	VehicleFactory& GetVehicleFactory() { return vehicleFactory; }

	AssetFactory& GetAssetFactory() { return assetFactory; }
	AppFactory& GetAppFactory() { return appFactory; }
	PuzzleFactory& GetPuzzleFactory() { return puzzleFactory; }
	WeaponFactory& GetWeaponFactory() { return weaponFactory; }

private:
	Registry();

	// 对paths里的dll做21个concept的RegisterConcept，成功加载的记进loadedDllPaths
	void RegisterMods(const std::vector<std::string>& paths);

	// 已经成功加载并注册过的dll路径，RefreshMods只处理不在这里的。
	std::unordered_set<std::string> loadedDllPaths;

	// 必须活得比21个Factory一样长(creator/deleter指向dll代码段)
	ModLoader modLoader;

	TerrainFactory terrainFactory;
	RoadnetFactory roadnetFactory;
	ZoneFactory zoneFactory;
	BuildingFactory buildingFactory;
	ComponentFactory componentFactory;
	RoomFactory roomFactory;

	NameFactory nameFactory;
	SchedulerFactory schedulerFactory;

	JobFactory jobFactory;
	OrganizationFactory organizationFactory;

	ScriptFactory scriptFactory;

	ProductFactory productFactory;
	StorageFactory storageFactory;
	ManufactureFactory manufactureFactory;

	RouteFactory routeFactory;
	StationFactory stationFactory;
	VehicleFactory vehicleFactory;

	AssetFactory assetFactory;
	AppFactory appFactory;
	PuzzleFactory puzzleFactory;
	WeaponFactory weaponFactory;

};
