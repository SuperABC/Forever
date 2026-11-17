#include "suites.h"

#include "test_framework.h"
#include "world_build.h"

#include "common/config.h"
#include "common/mod_test.h"
#include "common/random.h"
#include "common/registry.h"
#include "traffic/vehicle_mod.h"
#include "player/weapon_mod.h"
#include "society/job_mod.h"
#include "map/room_mod.h"
#include "map/terrain_mod.h"
#include "populace/scheduler_mod.h"
#include "player/asset_mod.h"
#include "map/building_mod.h"
#include "traffic/station_mod.h"
#include "industry/product_mod.h"
#include "industry/storage_mod.h"
#include "industry/manufacture_mod.h"

#include <cstring>
#include <exception>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>


using namespace std;

namespace {
	string g_currentModId; // 正在测的mod id，供ValidateConcept跳过占位模板
	bool IsEmptyTemplateId(const string& id) { return id.size() > 6 && id.compare(id.size() - 6, 6, "_empty") == 0; }
	struct ModOutcome {
		bool created = false;
		bool passed = false;
		bool nameUnique = true;
		unsigned long long unscoped = 0;
		string error;
		vector<string> problems; // 按concept的数值/字段合法性检查(宿主一侧，不改mod)
	};

	// 各concept的字段合法性检查：默认空实现，下面按基类重载。mod的SetProperty里填字段，这里统一校验。
	template <typename Mod> void ValidateConcept(Mod*, vector<string>&) {}
	void ValidateConcept(VehicleMod* mod, vector<string>& problems) {
		mod->SetProperty();
		if (mod->sizeX <= 0 || mod->sizeY <= 0 || mod->sizeZ <= 0) problems.push_back("vehicle size must be positive");
		if (mod->capacity < 0) problems.push_back("vehicle capacity is negative");
		if (mod->boardable && mod->capacity == 0) problems.push_back("boardable vehicle has capacity 0");
		if (mod->meshTransform.scale <= 0) problems.push_back("vehicle mesh scale must be positive");
		if (mod->blueprintPath.empty() && mod->transitMeshPath.empty() && mod->drivable) problems.push_back("drivable vehicle has neither blueprintPath nor transitMeshPath");
	}
	void ValidateConcept(ProductMod* mod, vector<string>& problems) {
		mod->SetProperty();
		// 占位mod(product_empty)故意不填价格，Core::Product构造时判无效，不会进游戏。
		if (mod->retailPrice == 0 && mod->importPrice == 0 && mod->exportPrice == 0 && mod->ingredients.empty()) return;
		if (mod->batchSize <= 0) problems.push_back("product batchSize must be positive");
		// 价格规则见product_mod.h：retail > import > export > 0。
		if (!(mod->retailPrice > mod->importPrice && mod->importPrice > mod->exportPrice && mod->exportPrice > 0)) problems.push_back("product prices must satisfy retail > import > export > 0");
		for (const auto& [type, amount] : mod->ingredients) if (type.empty() || amount <= 0) problems.push_back("product ingredient needs a type and a positive amount");
		for (const auto& [type, amount] : mod->byproducts) if (type.empty() || amount <= 0) problems.push_back("product byproduct needs a type and a positive amount");
		if (!mod->serviceType.empty() && mod->serviceQuality <= 0) problems.push_back("product with a serviceType needs a positive serviceQuality");
	}
	void ValidateConcept(JobMod* mod, vector<string>& problems) {
		mod->SetProperty();
		if (!mod->scriptModName.empty() && !Registry::Get().CheckModRegistered(mod->scriptModName)) problems.push_back("job scriptModName '" + mod->scriptModName + "' is not a registered mod");
		for (const string& milestone : mod->milestoneNames) if (milestone.empty()) problems.push_back("job has an empty milestone name");
	}
	void ValidateConcept(BuildingMod* mod, vector<string>& problems) {
		if (mod->stateOwnedChance < 0.f || mod->stateOwnedChance > 1.f) problems.push_back("building stateOwnedChance must be within [0,1]");
		if (!mod->scriptModName.empty() && !Registry::Get().CheckModRegistered(mod->scriptModName)) problems.push_back("building scriptModName '" + mod->scriptModName + "' is not a registered mod");
		if (!mod->stationMod.empty() && !Registry::Get().CheckModRegistered(mod->stationMod)) problems.push_back("building stationMod '" + mod->stationMod + "' is not a registered mod");
		for (const auto& cabin : mod->cabins) {
			if (cabin.minFloor > cabin.maxFloor) problems.push_back("elevator cabin minFloor > maxFloor");
			if (!cabin.scriptModName.empty() && !Registry::Get().CheckModRegistered(cabin.scriptModName)) problems.push_back("elevator cabin scriptModName is not a registered mod");
		}
	}
	void ValidateConcept(StationMod* mod, vector<string>& problems) {
		mod->SetProperty();
		mod->Layout(0, 60.f, 60.f); // 端口在Layout里按站台尺寸摆
		if (mod->stationType.empty() && mod->stops.empty() && mod->inlets.empty() && mod->outlets.empty()) return; // station_empty占位
		if (mod->stationType.empty()) problems.push_back("station stationType is empty");
		// 公交站贴路设置，没有站台端口；其余站点必须有端口。
		if (string(mod->stationType) != "bus" && mod->stops.empty() && mod->inlets.empty() && mod->outlets.empty()) problems.push_back("station has no ports");
	}
	bool IsRegistered(const string& id) { return Registry::Get().CheckModRegistered(id); }
	void ValidateConcept(RoomMod* mod, vector<string>& problems) {
		mod->SetProperty();
		if (mod->residenceCapacity < 0 || mod->workspaceCapacity < 0) problems.push_back("room capacity is negative");
		if (mod->isResidence && mod->residenceCapacity == 0) problems.push_back("residence room has 0 beds");
		if (mod->isWorkspace && mod->workspaceCapacity == 0) problems.push_back("workspace room has 0 seats");
		if (mod->rent < 0) problems.push_back("room rent is negative");
		if (mod->isStorage && !mod->storageType.empty() && !IsRegistered(mod->storageType)) problems.push_back("room storageType '" + mod->storageType + "' is not a registered mod");
		if (mod->isManufacture) for (const string& type : mod->manufactureTypes) if (!IsRegistered(type)) problems.push_back("room manufactureType '" + type + "' is not a registered mod");
		if (!mod->scriptModName.empty() && !IsRegistered(mod->scriptModName)) problems.push_back("room scriptModName '" + mod->scriptModName + "' is not a registered mod");
	}
	void ValidateConcept(SchedulerMod* mod, vector<string>& problems) {
		mod->SetProperty();
		if (!mod->scriptModName.empty() && !IsRegistered(mod->scriptModName)) problems.push_back("scheduler scriptModName '" + mod->scriptModName + "' is not a registered mod");
	}
	void ValidateConcept(AssetMod* mod, vector<string>& problems) {
		mod->SetProperty();
		if (mod->weight < 0 || mod->size < 0 || mod->volume < 0) problems.push_back("asset weight/size/volume is negative");
		if (mod->weapon && mod->weaponType.empty()) problems.push_back("weapon asset has no weaponType");
		if (!mod->weaponType.empty() && !IsRegistered(mod->weaponType)) problems.push_back("asset weaponType '" + mod->weaponType + "' is not a registered mod");
		if (!mod->productType.empty() && !IsRegistered(mod->productType)) problems.push_back("asset productType '" + mod->productType + "' is not a registered mod");
	}
	void ValidateConcept(TerrainMod* mod, vector<string>& problems) {
		mod->SetupTexture();
		if (mod->diffusePath.empty() && !IsEmptyTemplateId(g_currentModId)) problems.push_back("terrain diffusePath is empty after SetupTexture");
	}
	void ValidateConcept(StorageMod* mod, vector<string>& problems) {
		mod->SetProperty();
		if (mod->capacity < 0) problems.push_back("storage capacity is negative");
		if (!mod->canTake && !mod->canPut) problems.push_back("storage can neither take nor put");
		for (const auto& [type, demand] : mod->serviceTypeDemand) if (type.empty() || demand < 0) problems.push_back("storage serviceTypeDemand entry invalid");
		for (const auto& [type, demand] : mod->productDemand) if (type.empty() || demand < 0) problems.push_back("storage productDemand entry invalid");
	}
	void ValidateConcept(ManufactureMod* mod, vector<string>& problems) {
		mod->SetTargets();
		for (const auto& [type, batches] : mod->targets) if (type.empty() || batches <= 0) problems.push_back("manufacture target needs a product type and a positive batch count");
	}
	void ValidateConcept(WeaponMod* mod, vector<string>& problems) {
		mod->SetProperty();
		// 不开火的工具(如相机)弹匣/射程都是0，不按枪检查。
		if (mod->magazineCapacity == 0 && mod->maxRange == 0) return;
		if (mod->magazineCapacity <= 0) problems.push_back("weapon magazine capacity must be positive");
		if (mod->maxRange <= 0) problems.push_back("weapon maxRange must be positive");
		if (mod->modelScale <= 0) problems.push_back("weapon modelScale must be positive");
	}

	// 创建实例 -> 在和Core包装类相同的随机作用域里跑UnitTest -> 再创建第二个实例检查名字唯一 -> 销毁。
	template <typename Factory, typename Mod>
	void RunMod(Factory& factory, Mod* (Factory::* create)(const string&), void (Factory::* destroy)(Mod*),
		const string& id, ModOutcome& outcome) {
		Mod* mod = (factory.*create)(id);
		if (!mod) return;
		outcome.created = true;
		const unsigned long long before = RandomRegistry::UnscopedDraws();
		{
			RandomScope scope(RandomRegistry::GetInstanceStream(mod));
			try {
				outcome.passed = mod->UnitTest(TestPrintLine);
			}
			catch (const exception& e) {
				outcome.passed = false;
				outcome.error = e.what();
			}
		}
		outcome.unscoped = RandomRegistry::UnscopedDraws() - before;
		ValidateConcept(mod, outcome.problems);

		const string firstName = mod->GetName() ? mod->GetName() : "";
		Mod* second = (factory.*create)(id);
		if (second) {
			const string secondName = second->GetName() ? second->GetName() : "";
			outcome.nameUnique = firstName != secondName;
			(factory.*destroy)(second);
		}
		(factory.*destroy)(mod);
	}

	struct ConceptTester {
		const char* conceptKey;
		function<void(const string&, ModOutcome&)> run;
	};

	vector<ConceptTester> BuildTesters() {
		Registry& r = Registry::Get();
		return {
			{ "Terrains", [&r](const string& id, ModOutcome& o) { RunMod(r.GetTerrainFactory(), &TerrainFactory::CreateTerrain, &TerrainFactory::DestroyTerrain, id, o); } },
			{ "Roadnets", [&r](const string& id, ModOutcome& o) { RunMod(r.GetRoadnetFactory(), &RoadnetFactory::CreateRoadnet, &RoadnetFactory::DestroyRoadnet, id, o); } },
			{ "Zones", [&r](const string& id, ModOutcome& o) { RunMod(r.GetZoneFactory(), &ZoneFactory::CreateZone, &ZoneFactory::DestroyZone, id, o); } },
			{ "Buildings", [&r](const string& id, ModOutcome& o) { RunMod(r.GetBuildingFactory(), &BuildingFactory::CreateBuilding, &BuildingFactory::DestroyBuilding, id, o); } },
			{ "Components", [&r](const string& id, ModOutcome& o) { RunMod(r.GetComponentFactory(), &ComponentFactory::CreateComponent, &ComponentFactory::DestroyComponent, id, o); } },
			{ "Rooms", [&r](const string& id, ModOutcome& o) { RunMod(r.GetRoomFactory(), &RoomFactory::CreateRoom, &RoomFactory::DestroyRoom, id, o); } },
			{ "Names", [&r](const string& id, ModOutcome& o) { RunMod(r.GetNameFactory(), &NameFactory::CreateName, &NameFactory::DestroyName, id, o); } },
			{ "Schedulers", [&r](const string& id, ModOutcome& o) { RunMod(r.GetSchedulerFactory(), &SchedulerFactory::CreateScheduler, &SchedulerFactory::DestroyScheduler, id, o); } },
			{ "Jobs", [&r](const string& id, ModOutcome& o) { RunMod(r.GetJobFactory(), &JobFactory::CreateJob, &JobFactory::DestroyJob, id, o); } },
			{ "Organizations", [&r](const string& id, ModOutcome& o) { RunMod(r.GetOrganizationFactory(), &OrganizationFactory::CreateOrganization, &OrganizationFactory::DestroyOrganization, id, o); } },
			{ "Scripts", [&r](const string& id, ModOutcome& o) { RunMod(r.GetScriptFactory(), &ScriptFactory::CreateScript, &ScriptFactory::DestroyScript, id, o); } },
			{ "Products", [&r](const string& id, ModOutcome& o) { RunMod(r.GetProductFactory(), &ProductFactory::CreateProduct, &ProductFactory::DestroyProduct, id, o); } },
			{ "Storages", [&r](const string& id, ModOutcome& o) { RunMod(r.GetStorageFactory(), &StorageFactory::CreateStorage, &StorageFactory::DestroyStorage, id, o); } },
			{ "Manufactures", [&r](const string& id, ModOutcome& o) { RunMod(r.GetManufactureFactory(), &ManufactureFactory::CreateManufacture, &ManufactureFactory::DestroyManufacture, id, o); } },
			{ "Routes", [&r](const string& id, ModOutcome& o) { RunMod(r.GetRouteFactory(), &RouteFactory::CreateRoute, &RouteFactory::DestroyRoute, id, o); } },
			{ "Stations", [&r](const string& id, ModOutcome& o) { RunMod(r.GetStationFactory(), &StationFactory::CreateStation, &StationFactory::DestroyStation, id, o); } },
			{ "Vehicles", [&r](const string& id, ModOutcome& o) { RunMod(r.GetVehicleFactory(), &VehicleFactory::CreateVehicle, &VehicleFactory::DestroyVehicle, id, o); } },
			{ "Assets", [&r](const string& id, ModOutcome& o) { RunMod(r.GetAssetFactory(), &AssetFactory::CreateAsset, &AssetFactory::DestroyAsset, id, o); } },
			{ "Apps", [&r](const string& id, ModOutcome& o) { RunMod(r.GetAppFactory(), &AppFactory::CreateApp, &AppFactory::DestroyApp, id, o); } },
			{ "Puzzles", [&r](const string& id, ModOutcome& o) { RunMod(r.GetPuzzleFactory(), &PuzzleFactory::CreatePuzzle, &PuzzleFactory::DestroyPuzzle, id, o); } },
			{ "Weapons", [&r](const string& id, ModOutcome& o) { RunMod(r.GetWeaponFactory(), &WeaponFactory::CreateWeapon, &WeaponFactory::DestroyWeapon, id, o); } },
		};
	}
}

void RunModsSuite(const TestOptions& options) {
	TestRun& run = TestRun::Get();
	run.Begin("mods", 0, options.selfCommand + " --suite mods");
	string error;
	if (!LoadTestConfig(options.configPath, 0, options.outDir, error)) {
		run.Check(false, "load config", error);
		return;
	}

	const vector<ConceptTester> testers = BuildTesters();
	map<string, ConceptTester> byConcept;
	for (const ConceptTester& tester : testers) byConcept.emplace(tester.conceptKey, tester);

	const unsigned long long testSeed = options.EffectiveSeeds()[0];
	const vector<Config::ModEntry> entries = Config::GetModEnables();
	run.Metric("mods discovered", static_cast<double>(entries.size()), "");

	set<string> seenIds;
	map<string, int> perConcept;
	for (const Config::ModEntry& entry : entries) {
		const auto tester = byConcept.find(entry.conceptKey);
		if (tester == byConcept.end()) {
			run.Check(false, "known concept: " + entry.conceptKey, "mod " + entry.modId);
			continue;
		}
		run.Check(seenIds.insert(entry.modId).second, "mod id is globally unique: " + entry.modId);

		// 只启用这一个(单选concept会顶掉别的)，重载参数表(会重置随机登记)，再给它固定种子。
		Config::SetModEnabled(entry.conceptKey, entry.modId, true);
		Registry::Get().ReloadModArgs();
		RandomRegistry::SetGlobalSeed(true, testSeed);
		RandomRegistry::SetModSeed(entry.modId, true, Fnv1a(entry.modId.data(), entry.modId.size(), testSeed));

		ModOutcome outcome;
		g_currentModId = entry.modId;
		tester->second.run(entry.modId, outcome);
		run.Check(outcome.created, "mod creates: " + entry.modId, "CreateXxx returned null although the mod is discovered and enabled");
		if (!outcome.created) continue;
		perConcept[entry.conceptKey]++;
		run.Check(outcome.passed, "UnitTest passes: " + entry.modId, outcome.error);
		{
			string joined;
			for (const string& problem : outcome.problems) joined += (joined.empty() ? "" : "; ") + problem;
			run.Check(outcome.problems.empty(), "concept field validation: " + entry.modId, joined);
		}
		run.Check(outcome.unscoped == 0, "UnitTest has no unscoped GetRandom: " + entry.modId,
			to_string(outcome.unscoped) + " draws outside any RandomScope");
		// Forever_Mod/Empty里的*_empty是给mod作者抄的占位模板，名字固定，不受"GetName全局唯一"约定约束。
		const bool isEmptyTemplate = entry.modId.size() > 6 && entry.modId.compare(entry.modId.size() - 6, 6, "_empty") == 0;
		if (!isEmptyTemplate) run.Check(outcome.nameUnique, "two instances get different GetName(): " + entry.modId);
		run.Check(Registry::Get().CheckModRegistered(entry.modId), "mod is registered in its Factory: " + entry.modId);
	}
	for (const auto& [conceptKey, count] : perConcept) run.Metric("mods tested: " + conceptKey, count, "");
}
