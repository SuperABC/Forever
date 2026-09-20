#include "ForeverModSubsystem.h"

#include "common/config.h"
#include "common/loader.h"

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

#include "Misc/Paths.h"

namespace {

	// 把Config::GetConceptMods()返回的(id, 参数)列表转成Factory::SetModArgs要的id->参数表。
	std::unordered_map<std::string, std::string> ToArgsMap(const std::vector<std::pair<std::string, std::string>>& entries) {
		std::unordered_map<std::string, std::string> map;
		for (const auto& [id, args] : entries) {
			map[id] = args;
		}
		return map;
	}

	// 20个concept共用的校验逻辑:遍历factory里已注册的id,创建一个临时实例(Create<Concept>
	// 内部会查SetModArgs预先设置好的参数表,把参数字符串直接传给creator创建实例),把
	// id/GetType()/GetName()打进日志(GetName()对Empty<Concept>会把参数字符串带回来,用于
	// 验证参数传递链路),再销毁。create/destroy用lambda传入,因为20个<Concept>Factory的
	// Create/Destroy方法名各不相同,没有共同基类可以多态调用。
	template <typename FactoryT, typename CreateFn, typename DestroyFn>
	void ValidateFactory(FactoryT& factory, const TCHAR* label, CreateFn create, DestroyFn destroy) {
		for (const std::string& id : factory.GetRegisteredIds()) {
			auto* instance = create(factory, id);
			if (!instance)
				continue;

			UE_LOG(LogTemp, Log, TEXT("ForeverModSubsystem: [%s] id=%s type=%s name=%s"),
				label, UTF8_TO_TCHAR(id.c_str()), UTF8_TO_TCHAR(instance->GetType()), UTF8_TO_TCHAR(instance->GetName()));
			destroy(factory, instance);
		}
	}

} // namespace

void UForeverModSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const FString configPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Resource/Config/config.json"));
	Config::ReadConfig(TCHAR_TO_UTF8(*configPath));

	std::vector<std::string> mods = Config::GetMods();
	if (mods.empty()) {
		// config.json缺失、解析失败或dll_paths为空时,回退扫描默认的Forever_Mod/目录
		// (和ForeverKeyBindingSubsystem"缺配置就用硬编码默认值"的容错风格一致)。
		const FString defaultModDir = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectDir(), TEXT("../Forever_Mod")));
		UE_LOG(LogTemp, Warning, TEXT("ForeverModSubsystem: config.json 未提供任何dll_paths,回退扫描默认目录 %s。"), *defaultModDir);
		Config::AddDllPath(TCHAR_TO_UTF8(*defaultModDir));
		mods = Config::GetMods();
	}

	if (Config::GetLayouts().empty()) {
		// config.json没有配置layout_paths(或者根本没有config.json)时,回退扫描默认的
		// Resource/Layouts/目录——和上面dll_paths的回退同一个容错风格。
		const FString defaultLayoutDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Resource/Layouts"));
		UE_LOG(LogTemp, Warning, TEXT("ForeverModSubsystem: config.json 未提供任何layout_paths,回退扫描默认目录 %s。"), *defaultLayoutDir);
		Config::AddLayoutPath(TCHAR_TO_UTF8(*defaultLayoutDir));
	}

	if (!Config::HasResourcePaths()) {
		// config.json没有配置resource_path(或者根本没有config.json)时,回退扫描默认的
		// Resource/Story/目录——和上面layout_paths的回退同一个容错风格。
		const FString defaultResourceDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Resource/Story"));
		UE_LOG(LogTemp, Warning, TEXT("ForeverModSubsystem: config.json 未提供任何resource_path,回退扫描默认目录 %s。"), *defaultResourceDir);
		Config::AddResourcePath(TCHAR_TO_UTF8(*defaultResourceDir));
	}

	ModLoader modLoader;

	{
		ComponentFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("component_mods")));
		modLoader.RegisterConcept<ComponentFactory>(mods, "RegisterModComponents", "FinishModComponents", &factory);
		ValidateFactory(factory, TEXT("Component"),
			[](ComponentFactory& f, const std::string& id) { return f.CreateComponent(id); },
			[](ComponentFactory& f, ComponentMod* m) { f.DestroyComponent(m); });
	}

	{
		RoomFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("room_mods")));
		modLoader.RegisterConcept<RoomFactory>(mods, "RegisterModRooms", "FinishModRooms", &factory);
		ValidateFactory(factory, TEXT("Room"),
			[](RoomFactory& f, const std::string& id) { return f.CreateRoom(id); },
			[](RoomFactory& f, RoomMod* m) { f.DestroyRoom(m); });
	}

	{
		AssetFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("asset_mods")));
		modLoader.RegisterConcept<AssetFactory>(mods, "RegisterModAssets", "FinishModAssets", &factory);
		ValidateFactory(factory, TEXT("Asset"),
			[](AssetFactory& f, const std::string& id) { return f.CreateAsset(id); },
			[](AssetFactory& f, AssetMod* m) { f.DestroyAsset(m); });
	}

	{
		AppFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("app_mods")));
		modLoader.RegisterConcept<AppFactory>(mods, "RegisterModApps", "FinishModApps", &factory);
		ValidateFactory(factory, TEXT("App"),
			[](AppFactory& f, const std::string& id) { return f.CreateApp(id); },
			[](AppFactory& f, AppMod* m) { f.DestroyApp(m); });
	}

	{
		PuzzleFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("puzzle_mods")));
		modLoader.RegisterConcept<PuzzleFactory>(mods, "RegisterModPuzzles", "FinishModPuzzles", &factory);
		ValidateFactory(factory, TEXT("Puzzle"),
			[](PuzzleFactory& f, const std::string& id) { return f.CreatePuzzle(id); },
			[](PuzzleFactory& f, PuzzleMod* m) { f.DestroyPuzzle(m); });
	}

	{
		NameFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("name_mods")));
		modLoader.RegisterConcept<NameFactory>(mods, "RegisterModNames", "FinishModNames", &factory);
		ValidateFactory(factory, TEXT("Name"),
			[](NameFactory& f, const std::string& id) { return f.CreateName(id); },
			[](NameFactory& f, NameMod* m) { f.DestroyName(m); });
	}

	{
		SchedulerFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("scheduler_mods")));
		modLoader.RegisterConcept<SchedulerFactory>(mods, "RegisterModSchedulers", "FinishModSchedulers", &factory);
		ValidateFactory(factory, TEXT("Scheduler"),
			[](SchedulerFactory& f, const std::string& id) { return f.CreateScheduler(id); },
			[](SchedulerFactory& f, SchedulerMod* m) { f.DestroyScheduler(m); });
	}

	{
		JobFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("job_mods")));
		modLoader.RegisterConcept<JobFactory>(mods, "RegisterModJobs", "FinishModJobs", &factory);
		ValidateFactory(factory, TEXT("Job"),
			[](JobFactory& f, const std::string& id) { return f.CreateJob(id); },
			[](JobFactory& f, JobMod* m) { f.DestroyJob(m); });
	}

	{
		OrganizationFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("organization_mods")));
		modLoader.RegisterConcept<OrganizationFactory>(mods, "RegisterModOrganizations", "FinishModOrganizations", &factory);
		ValidateFactory(factory, TEXT("Organization"),
			[](OrganizationFactory& f, const std::string& id) { return f.CreateOrganization(id); },
			[](OrganizationFactory& f, OrganizationMod* m) { f.DestroyOrganization(m); });
	}

	{
		ScriptFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("script_mods")));
		modLoader.RegisterConcept<ScriptFactory>(mods, "RegisterModScripts", "FinishModScripts", &factory);
		ValidateFactory(factory, TEXT("Script"),
			[](ScriptFactory& f, const std::string& id) { return f.CreateScript(id); },
			[](ScriptFactory& f, ScriptMod* m) { f.DestroyScript(m); });
	}

	{
		ProductFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("product_mods")));
		modLoader.RegisterConcept<ProductFactory>(mods, "RegisterModProducts", "FinishModProducts", &factory);
		ValidateFactory(factory, TEXT("Product"),
			[](ProductFactory& f, const std::string& id) { return f.CreateProduct(id); },
			[](ProductFactory& f, ProductMod* m) { f.DestroyProduct(m); });
	}

	{
		StorageFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("storage_mods")));
		modLoader.RegisterConcept<StorageFactory>(mods, "RegisterModStorages", "FinishModStorages", &factory);
		ValidateFactory(factory, TEXT("Storage"),
			[](StorageFactory& f, const std::string& id) { return f.CreateStorage(id); },
			[](StorageFactory& f, StorageMod* m) { f.DestroyStorage(m); });
	}

	{
		ManufactureFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("manufacture_mods")));
		modLoader.RegisterConcept<ManufactureFactory>(mods, "RegisterModManufactures", "FinishModManufactures", &factory);
		ValidateFactory(factory, TEXT("Manufacture"),
			[](ManufactureFactory& f, const std::string& id) { return f.CreateManufacture(id); },
			[](ManufactureFactory& f, ManufactureMod* m) { f.DestroyManufacture(m); });
	}

	{
		RouteFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("route_mods")));
		modLoader.RegisterConcept<RouteFactory>(mods, "RegisterModRoutes", "FinishModRoutes", &factory);
		ValidateFactory(factory, TEXT("Route"),
			[](RouteFactory& f, const std::string& id) { return f.CreateRoute(id); },
			[](RouteFactory& f, RouteMod* m) { f.DestroyRoute(m); });
	}

	{
		StationFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("station_mods")));
		modLoader.RegisterConcept<StationFactory>(mods, "RegisterModStations", "FinishModStations", &factory);
		ValidateFactory(factory, TEXT("Station"),
			[](StationFactory& f, const std::string& id) { return f.CreateStation(id); },
			[](StationFactory& f, StationMod* m) { f.DestroyStation(m); });
	}

	{
		VehicleFactory factory;
		factory.SetModArgs(ToArgsMap(Config::GetConceptMods("vehicle_mods")));
		modLoader.RegisterConcept<VehicleFactory>(mods, "RegisterModVehicles", "FinishModVehicles", &factory);
		ValidateFactory(factory, TEXT("Vehicle"),
			[](VehicleFactory& f, const std::string& id) { return f.CreateVehicle(id); },
			[](VehicleFactory& f, VehicleMod* m) { f.DestroyVehicle(m); });
	}

	modLoader.UnloadAll();
}
