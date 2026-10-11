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


// 21个concept(对照表见common/loader.md)的mod注册表：mod dll的发现/注册
// (LoadLibrary+RegisterConcept)只应该在整个UE进程生命周期里发生一次，和"每个持有者
// (Map/Populace/Story/……)每次开局实际生成一遍对象、结束时整个释放、下次开局重新生成"这件
// 事完全分开——Map/Populace/Story不自己持有ModLoader+Factory，改成持有一个引用成员，指向
// 这里对应的Factory。common/是Core里各域都可以依赖的顶层位置(implement.h的PostHandle
// 同款定位)。
//
// 用函数内static实现懒汉单例(Get()首次调用时构造，构造函数里跑完全部21个RegisterConcept)，
// 不挂在任何UE Subsystem的生命周期上——Core要保持engine-agnostic(见CONVENTIONS.md)，纯
// C++单例足够满足"全局只注册一次"：Core.lib静态链接进UnrealEditor-Forever.dll，只要这个
// dll不被卸载/热重载，函数内static对象就活得比任何一次PIE开的Map/Populace/Story都长。
//
// 21个Factory对"已注册mod id列表"之外没有会跨局累积的状态——CreateXxx/DestroyXxx只是按
// 注册时存的creator/deleter函数指针创建/销毁一个实例，跟调用了多少次、上一局是不是已经开过
// 完全无关，所以多局共用同一个Factory实例是安全的。
//
// **`SetModArgs`不在这份"只做一次"的范围内**——`RegisterConcept`(dll发现/加载/把
// creator/deleter函数指针记进Factory)和`SetModArgs`(把这次config.json"<concept>_mods"
// 数组解析出的id->参数字符串表交给Factory，`CreateXxx(id)`创建实例时查这张表、把参数字符串
// 直接传给creator函数)是两件不同频率的事：前者只要mod dll本身没变就不需要重新扫描，
// 后者每次实际开局都可能不一样（不同局可以指向不同的config文件，参数也可能不同）。构造
// 函数只调一次`RegisterConcept`，`SetModArgs`拆到`ReloadModArgs()`里单独暴露，调用方
// (`AForeverFrameworkActor::BeginPlay()`，在7个`Ensure*Generated()`之前)在每次真正开局、
// 确认这一局要用的config已经被`Config::ReadConfig`读进内存之后，调一次
// `Registry::Get().ReloadModArgs()`刷新参数表，不需要也不应该重新触发dll扫描。
class Registry {
public:
	static Registry& Get();

	Registry(const Registry&) = delete;
	Registry& operator=(const Registry&) = delete;

	// 重新按当前Config::GetConceptMods()的内容给全部21个Factory调一次SetModArgs——不重新
	// LoadLibrary、不重新RegisterConcept，只刷新"mod id -> 参数字符串"这张表。每次真正开局
	// (读了新的config文件之后)都应该调一次，因为不同局可能用不同的config.json，见
	// Source/Core/common/registry.md"SetModArgs的调用频率"一节。
	void ReloadModArgs();

	// 对全部21个Factory各查一次CheckRegistered(id)，只要有任意一个认得这个id就返回
	// true——mod id不带concept前缀，不能靠字符串猜它属于哪个concept("building_clean"
	// 完全可能是个Job mod)，所以这里不按concept分流，直接全局匹配，供
	// AForeverFrameworkActor::ValidateMainStoryDependencies()校验mod_dependences用，
	// 见script.md"mod_dependences"一节的设计决策。
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

	// 持有mod dll句柄,必须活得比它注册进的21个Factory一样长——它们存的creator/deleter函数
	// 指针指向这些dll的代码段,句柄一旦被FreeLibrary这些指针就悬空;21个Factory都是这一个
	// 单例的成员,生命周期天然一致。
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
