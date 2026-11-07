#pragma once

#include <string>
#include <functional>

#include "common/json.h"
#include "map/terrain_mod.h"
#include "map/roadnet_mod.h"
#include "map/zone_mod.h"
#include "map/building_mod.h"
#include "map/component_mod.h"
#include "map/room_mod.h"
#include "player/asset_mod.h"
#include "player/app_mod.h"
#include "player/puzzle_mod.h"
#include "populace/name_mod.h"
#include "populace/scheduler_mod.h"
#include "society/job_mod.h"
#include "society/organization_mod.h"
#include "story/script_mod.h"
#include "industry/product_mod.h"
#include "industry/storage_mod.h"
#include "industry/manufacture_mod.h"
#include "traffic/route_mod.h"
#include "traffic/station_mod.h"
#include "traffic/vehicle_mod.h"

class EmptyTerrain : public TerrainMod {
public:
	EmptyTerrain(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "terrain_empty"; }
	virtual const char* GetType() const override { return "terrain_empty"; }
	virtual const char* GetName() override { return name.data(); }

	virtual float GetPriority() const override { return 0.0f; }
	virtual void SetupTexture() override {}
	virtual void DistributeTerrain(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<bool(int, int, const std::string&)>& setTerrain,
		const std::function<float(int, int)>& getHeight,
		const std::function<bool(int, int, float)>& setHeight,
		const std::function<bool(int, int, bool, float)>& setWater,
		const std::function<bool(int, int, const float*)>& setFineHeights) const override {}

private:
	std::string name;
};

class EmptyRoadnet : public RoadnetMod {
public:
	EmptyRoadnet(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "roadnet_empty"; }
	virtual const char* GetType() const override { return "roadnet_empty"; }
	virtual const char* GetName() override { return name.data(); }

	virtual void DistributeRoadnet(int width, int height,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		const std::function<float(float, float)>& getHeight,
		int nodeStaticCount) override {}

private:
	std::string name;
};

class EmptyZone : public ZoneMod {
public:
	EmptyZone() { lastName = std::string("empty") + std::to_string(count++); }

	static const char* GetId() { return "zone_empty"; }
	virtual const char* GetType() const override { return "zone_empty"; }
	virtual const char* GetName() override { return lastName.c_str(); }

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {}
	virtual void Layout(int direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override {}

private:
	std::string lastName;
public:
	// 存档保存/恢复命名计数器count，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
};

class EmptyBuilding : public BuildingMod {
public:
	EmptyBuilding() { lastName = std::string("empty") + std::to_string(count++); }

	static const char* GetId() { return "building_empty"; }
	virtual const char* GetType() const override { return "building_empty"; }
	virtual const char* GetName() override { return lastName.c_str(); }

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {}
	static float RandomAcreage() { return 0.f; }
	static float GetAcreageMin() { return 0.f; }
	static float GetAcreageMax() { return 0.f; }
	static float GetPower(AREA_TYPE area) { return 0.f; }

private:
	std::string lastName;
public:
	// 存档保存/恢复命名计数器count，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
};

class EmptyComponent : public ComponentMod {
public:
	EmptyComponent(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "component_empty"; }
	virtual const char* GetType() const override { return "component_empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};

class EmptyRoom : public RoomMod {
public:
	EmptyRoom(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "room_empty"; }
	virtual const char* GetType() const override { return "room_empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};

class EmptyAsset : public AssetMod {
public:
	EmptyAsset(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "asset_empty"; }
	virtual const char* GetType() const override { return "asset_empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};

class EmptyApp : public AppMod {
public:
	EmptyApp(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "app_empty"; }
	virtual const char* GetType() const override { return "app_empty"; }
	virtual const char* GetName() override { return name.data(); }

	// 空占位：不产生任何画面/逻辑。
	virtual void Init(Canvas* canvas, PostHandle* post) override {}
	virtual void Loop(Canvas* canvas, int ms, PostHandle* post) override {}
	virtual void Back(Canvas* canvas, PostHandle* post) override {}
	virtual void Refresh(Canvas* canvas, PostHandle* post) override {}

private:
	std::string name;
};

class EmptyPuzzle : public PuzzleMod {
public:
	EmptyPuzzle(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "puzzle_empty"; }
	virtual const char* GetType() const override { return "puzzle_empty"; }
	virtual const char* GetName() override { return name.data(); }

	// 记下剧情脚本start_puzzle传来的本次启动参数(跟构造时config.json的args不是一回事)，
	// 供Loop打印出来，用来验证param确实在Init之前到达。
	virtual void SetParam(const std::string& param) override { this->param = param; }

	// 空占位：没有任何游戏逻辑，只在窗口最左上角打印本次启动的param，Loop固定返回nullptr(永不结束)。
	virtual void Init(Canvas* canvas, PostHandle* post) override {}
	virtual JsonValue* Loop(Canvas* canvas, int ms, PostHandle* post) override {
		canvas->ClearScreen();
		canvas->SetColor(255, 255, 255);
		canvas->SetFontSize(24);
		canvas->PutString("param: " + param, 0, 0);
		return nullptr;
	}

private:
	std::string name;
	std::string param;
};

class EmptyName : public NameMod {
public:
	EmptyName(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "name_empty"; }
	virtual const char* GetType() const override { return "name_empty"; }
	virtual const char* GetName() override { return name.data(); }

	// 空占位，不持有任何姓名词库，按接口约定的"失败"语义统一不调用setResult。
	virtual void GetSurname(const std::string& fullName,
		const std::function<void(const std::string&)>& setResult) const override {}
	virtual void GenerateName(bool allowMale, bool allowFemale, bool allowNeutral,
		const std::function<void(const std::string&)>& setResult) const override {}
	virtual void GenerateName(const std::string& surname,
		bool allowMale, bool allowFemale, bool allowNeutral,
		const std::function<void(const std::string&)>& setResult) const override {}

private:
	std::string name;
};

class EmptyScheduler : public SchedulerMod {
public:
	EmptyScheduler(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "scheduler_empty"; }
	virtual const char* GetType() const override { return "scheduler_empty"; }
	virtual const char* GetName() override { return name.data(); }

	static float GetPower() { return 0.f; }
	virtual void DailyPlan(const Time&, PostHandle*) override {}

private:
	std::string name;
};

class EmptyJob : public JobMod {
public:
	EmptyJob(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "job_empty"; }
	virtual const char* GetType() const override { return "job_empty"; }
	virtual const char* GetName() override { return name.data(); }

	virtual void DailyPlan(const Time&, PostHandle*) override {}

private:
	std::string name;
};

class EmptyOrganization : public OrganizationMod {
public:
	EmptyOrganization(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "organization_empty"; }
	virtual const char* GetType() const override { return "organization_empty"; }
	virtual const char* GetName() override { return name.data(); }

	// GetRequirements返回空表，Society::Init会跳过它。
	static float GetPower() { return 0.f; }
	static const std::unordered_map<std::string, std::pair<int, int>>& GetRequirements() {
		static const std::unordered_map<std::string, std::pair<int, int>> requirements;
		return requirements;
	}
	virtual void DesignJobsForRoom(const std::string&, const std::string&,
		const std::string&, const std::string&, int) override {}

private:
	std::string name;
};

class EmptyScript : public ScriptMod {
public:
	EmptyScript(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "script_empty"; }
	virtual const char* GetType() const override { return "script_empty"; }
	virtual const char* GetName() override { return name.data(); }

	virtual void WrapScript(const Event* event, const std::vector<ScriptAction>& actions,
		const ScriptContext& context, PostHandle* post) override {
		AutoCopy(actions);

		int idx = FindLabel("control", context);
		if (idx < 0 || !post) return;

		JsonValue request(DATA_OBJECT);
		request["post"] = "random citizen";
		post->Post(request);

		const JsonValue& result = post->GetResult();
		if (result["result"].AsString() != "success") return;

		controlChange.SetName(result["name"].AsString());

		actionStack.back()[idx] = static_cast<const Change*>(&controlChange);
	}

private:
	std::string name;
	ChangeControlChange controlChange;
};

class EmptyProduct : public ProductMod {
public:
	EmptyProduct(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "product_empty"; }
	virtual const char* GetType() const override { return "product_empty"; }
	virtual const char* GetName() override { return name.data(); }

	// 空占位：字段保持基类默认值。
	virtual void SetProperty() override {}

private:
	std::string name;
};

class EmptyStorage : public StorageMod {
public:
	EmptyStorage(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "storage_empty"; }
	virtual const char* GetType() const override { return "storage_empty"; }
	virtual const char* GetName() override { return name.data(); }

	// 空占位：categories/capacity保持基类默认值不填。
	virtual void SetProperty() override {}

private:
	std::string name;
};

class EmptyManufacture : public ManufactureMod {
public:
	EmptyManufacture(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "manufacture_empty"; }
	virtual const char* GetType() const override { return "manufacture_empty"; }
	virtual const char* GetName() override { return name.data(); }

	// 空占位：targets保持基类默认值(空map)不填。
	virtual void SetTargets() override {}

private:
	std::string name;
};

class EmptyRoute : public RouteMod {
public:
	EmptyRoute(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "route_empty"; }
	virtual const char* GetType() const override { return "route_empty"; }
	virtual const char* GetName() override { return name.data(); }

	// 空占位：stationType/vehicleType等选项字段保持基类默认值不填。
	virtual void SetProperty() override {}

	// 空占位：不连任何图，edgeStations/links/lines保持基类默认值(全空)。
	virtual void LayoutRoute(const std::vector<RouteStationInfo>&, int, int) override {}

private:
	std::string name;
};

class EmptyStation : public StationMod {
public:
	EmptyStation(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "station_empty"; }
	virtual const char* GetType() const override { return "station_empty"; }
	virtual const char* GetName() override { return name.data(); }

	// 空占位：stationType保持基类默认值不填。
	virtual void SetProperty() override {}

	// 空占位：不贴任何路，见station_mod.h StationMod::AssignRoads的约定(static方法)。
	static void AssignRoads(const std::vector<Road*>&, RoadStationEmitFunc, void*) {}

private:
	std::string name;
};

class EmptyVehicle : public VehicleMod {
public:
	EmptyVehicle(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "vehicle_empty"; }
	static const char* GetCategory() { return "empty"; } // 占位种类，不会被任何停车位选中
	virtual const char* GetType() const override { return "vehicle_empty"; }
	virtual const char* GetName() override { return name.data(); }

	// 空占位：blueprintPath/scriptModName等字段保持基类默认值不填。
	virtual void SetProperty() override {}

private:
	std::string name;
};
