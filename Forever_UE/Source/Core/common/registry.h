#pragma once

#include "map/terrain_factory.h"
#include "map/roadnet_factory.h"
#include "map/zone_factory.h"
#include "map/green_factory.h"
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
#include "polit/party_factory.h"

#include "common/loader.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>


// 23个concept的mod注册表(全局单例)，设计见registry.md
class Registry {
public:
	static Registry& Get();

	// 补加载config里还没加载的mod dll并刷新启用表，见registry.md。一局开始时调；别处要用Factory之前
	// (比如菜单里读档前的校验)也要先调它——dll只在一局期间留在进程里，平时是卸载掉的。
	void RefreshMods();

	// 卸载的结果。
	struct UnloadReport {
		std::vector<std::string> unloadedDlls;
		std::vector<std::string> keptDlls;                      // 还有存活的mod实例，没卸载
		std::vector<std::pair<std::string, int>> liveInstances; // (mod id, 存活实例数)，按id排序
	};
	// 一局结束、各域都销毁之后调：注销并卸载全部mod dll，见registry.md"卸载"。
	// 还有存活mod实例的dll(=有东西泄漏了)不卸载，它的注册项也留着，并打警告；其余照常卸载。
	UnloadReport UnloadMods();

	// 各mod id当前存活的实例数(只列非0的，按id排序)。一局结束后应该是空的。
	std::vector<std::pair<std::string, int>> GetLiveInstanceCounts();

	// 当前留在进程里的mod dll路径。
	std::vector<std::string> GetLoadedDllPaths() const;

	Registry(const Registry&) = delete;
	Registry& operator=(const Registry&) = delete;

	// 只刷新"mod id -> 参数字符串"表，不重新加载dll
	void ReloadModArgs();

	// 任意一个Factory认得这个id即true(不看id的格式，直接问全部Factory)
	bool CheckModRegistered(const std::string& id);

	// ---- 23个concept的唯一清单 ----
	// 对每个concept调一次fn(conceptKey, factory, &Factory::CreateXxx, &Factory::DestroyXxx)，fn用泛型lambda。
	// 注册、灌参数、查询、数活实例、注销(本类)和存档的逐concept操作(save.cpp)全都从这里遍历，别处不再各列一遍。
	// 新增concept：Dependence的mod_concepts.cpp加一行(导出符号)，这里加一行，下面加getter和成员。这份清单漏了或者
	// 和mod_concepts对不上，构造时的自检会抛异常(CheckConceptList)，不会悄悄不对。
	template <typename Fn>
	void ForEachConcept(Fn&& fn) {
		fn("Terrains", terrainFactory, &TerrainFactory::CreateTerrain, &TerrainFactory::DestroyTerrain);
		fn("Roadnets", roadnetFactory, &RoadnetFactory::CreateRoadnet, &RoadnetFactory::DestroyRoadnet);
		fn("Zones", zoneFactory, &ZoneFactory::CreateZone, &ZoneFactory::DestroyZone);
		fn("Greens", greenFactory, &GreenFactory::CreateGreen, &GreenFactory::DestroyGreen);
		fn("Buildings", buildingFactory, &BuildingFactory::CreateBuilding, &BuildingFactory::DestroyBuilding);
		fn("Components", componentFactory, &ComponentFactory::CreateComponent, &ComponentFactory::DestroyComponent);
		fn("Rooms", roomFactory, &RoomFactory::CreateRoom, &RoomFactory::DestroyRoom);
		fn("Names", nameFactory, &NameFactory::CreateName, &NameFactory::DestroyName);
		fn("Schedulers", schedulerFactory, &SchedulerFactory::CreateScheduler, &SchedulerFactory::DestroyScheduler);
		fn("Jobs", jobFactory, &JobFactory::CreateJob, &JobFactory::DestroyJob);
		fn("Organizations", organizationFactory, &OrganizationFactory::CreateOrganization, &OrganizationFactory::DestroyOrganization);
		fn("Scripts", scriptFactory, &ScriptFactory::CreateScript, &ScriptFactory::DestroyScript);
		fn("Products", productFactory, &ProductFactory::CreateProduct, &ProductFactory::DestroyProduct);
		fn("Storages", storageFactory, &StorageFactory::CreateStorage, &StorageFactory::DestroyStorage);
		fn("Manufactures", manufactureFactory, &ManufactureFactory::CreateManufacture, &ManufactureFactory::DestroyManufacture);
		fn("Vehicles", vehicleFactory, &VehicleFactory::CreateVehicle, &VehicleFactory::DestroyVehicle);
		fn("Stations", stationFactory, &StationFactory::CreateStation, &StationFactory::DestroyStation);
		fn("Routes", routeFactory, &RouteFactory::CreateRoute, &RouteFactory::DestroyRoute);
		fn("Assets", assetFactory, &AssetFactory::CreateAsset, &AssetFactory::DestroyAsset);
		fn("Puzzles", puzzleFactory, &PuzzleFactory::CreatePuzzle, &PuzzleFactory::DestroyPuzzle);
		fn("Apps", appFactory, &AppFactory::CreateApp, &AppFactory::DestroyApp);
		fn("Weapons", weaponFactory, &WeaponFactory::CreateWeapon, &WeaponFactory::DestroyWeapon);
		fn("Parties", partyFactory, &PartyFactory::CreateParty, &PartyFactory::DestroyParty);
	}
	// 上面清单里的conceptKey，按清单顺序。
	std::vector<std::string> GetConceptKeys();

	TerrainFactory& GetTerrainFactory() { return terrainFactory; }
	RoadnetFactory& GetRoadnetFactory() { return roadnetFactory; }
	ZoneFactory& GetZoneFactory() { return zoneFactory; }
	GreenFactory& GetGreenFactory() { return greenFactory; }
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

	PartyFactory& GetPartyFactory() { return partyFactory; }

private:
	Registry();

	// 自检：ForEachConcept的清单必须和Dependence的mod_concepts表一一对上，否则抛InternalException。
	void CheckConceptList();

	// 对paths里的dll做23个concept的RegisterConcept，成功加载的记进loadedDllPaths
	void RegisterMods(const std::vector<std::string>& paths);

	// 已经成功加载并注册过的dll路径，RefreshMods只处理不在这里的。
	std::unordered_set<std::string> loadedDllPaths;
	// 每个已加载的dll提供的mod id(加载时从Config抄一份)，卸载时用它把"还活着的实例"对应回dll。
	std::unordered_map<std::string, std::vector<std::string>> dllModIds;

	// 必须活得比23个Factory一样长(creator/deleter指向dll代码段)
	ModLoader modLoader;

	TerrainFactory terrainFactory;
	RoadnetFactory roadnetFactory;
	ZoneFactory zoneFactory;
	GreenFactory greenFactory;
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

	PartyFactory partyFactory;

};
