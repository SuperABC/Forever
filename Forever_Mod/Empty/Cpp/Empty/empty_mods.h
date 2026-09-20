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

// 阶段3示例mod:验证"config.json按mod id配置的命令行式参数"能透传到mod。20个concept各
// 一个EmptyXxx,id统一为"empty"(不同concept的Factory registries互相独立,id不冲突,和
// 旧工程"empty"这个占位mod id的用法一致)。参数不在ApplyArgs这个创建后钩子里传给mod,而是
// 直接由Create<Concept>喂给creator——config.json"<concept>_mods"数组里"empty --test true"
// 这种写法解析出的参数字符串,创建实例那一刻就作为构造函数参数交给EmptyXxx,mod自己决定怎么用
// (这里全部选择存进name成员)。GetName()把构造时收到的参数字符串拼进返回值,方便在
// ForeverModSubsystem的验证日志里确认参数确实从config.json一路传到了这里。真正的业务逻辑
// 本阶段不需要,阶段4对应系统落地时,这类占位mod的定位和旧工程"empty"系列mod一致(空实现兜底)。

class EmptyTerrain : public TerrainMod {
public:
	EmptyTerrain(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

	virtual float GetPriority() const override { return 0.0f; }
	virtual void SetupTexture() override {}
	virtual void DistributeTerrain(int width, int height,
		const std::function<std::string(int, int)>& getTerrain,
		const std::function<bool(int, int, std::string)>& setTerrain,
		const std::function<float(int, int)>& getHeight,
		const std::function<bool(int, int, float)>& setHeight) const override {}

private:
	std::string name;
};

class EmptyRoadnet : public RoadnetMod {
public:
	EmptyRoadnet(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

	virtual void DistributeRoadnet(int width, int height,
		const std::function<std::string(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		int nodeStaticCount) override {}

private:
	std::string name;
};

// EmptyZone/EmptyBuilding这次会话改回"一个本体独占一个mod实例"模型：Distribute()/
// explicitPlacements改成static Assign()（空实现，不参与显式占位）；寻址用的唯一名字计数器
// 直接写在构造函数里（老工程ResidentialZone::count同款），不再需要ApplyArgs把参数字符串拼进
// name（参数字符串如果还需要体现可以另外存一个字段，不影响寻址用的name）。
class EmptyZone : public ZoneMod {
public:
	EmptyZone() { lastName = std::string("empty") + std::to_string(count++); }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return lastName.c_str(); }

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {}
	virtual void Layout(int direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override {}

private:
	std::string lastName;
	static int count;
};

class EmptyBuilding : public BuildingMod {
public:
	EmptyBuilding() { lastName = std::string("empty") + std::to_string(count++); }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return lastName.c_str(); }

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {}
	static float RandomAcreage() { return 0.f; }
	static float GetAcreageMin() { return 0.f; }
	static float GetAcreageMax() { return 0.f; }
	static float GetPower(AREA_TYPE area) { return 0.f; }

private:
	std::string lastName;
	static int count;
};

class EmptyComponent : public ComponentMod {
public:
	EmptyComponent(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};

class EmptyRoom : public RoomMod {
public:
	EmptyRoom(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};

class EmptyAsset : public AssetMod {
public:
	EmptyAsset(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};

class EmptyApp : public AppMod {
public:
	EmptyApp(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};

class EmptyPuzzle : public PuzzleMod {
public:
	EmptyPuzzle(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};

class EmptyName : public NameMod {
public:
	EmptyName(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

	// 空占位，不持有任何姓名词库，按接口约定的"失败"语义统一返回空字符串。
	virtual std::string GetSurname(const std::string& fullName) const override { return ""; }
	virtual std::string GenerateName(bool allowMale, bool allowFemale, bool allowNeutral) const override { return ""; }
	virtual std::string GenerateName(const std::string& surname,
		bool allowMale, bool allowFemale, bool allowNeutral) const override { return ""; }

private:
	std::string name;
};

class EmptyScheduler : public SchedulerMod {
public:
	EmptyScheduler(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};

class EmptyJob : public JobMod {
public:
	EmptyJob(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

	// Society/Job域这次迁移新增的纯虚方法：这个占位mod不产生任何调度。
	virtual void DailyPlan(const Time&) override {}

private:
	std::string name;
};

class EmptyOrganization : public OrganizationMod {
public:
	EmptyOrganization(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

	// Society/Organization域这次迁移新增的两个纯虚方法：这个占位mod不参与地图上真实的
	// Component竞争，requirements留空即可(Society::Init跳过没有任何requirements的候选
	// 类型)，DesignJobsForRoom不需要配任何Job。
	static float GetPower() { return 0.f; }
	virtual void ComponentRequirements() override {}
	virtual void DesignJobsForRoom(const std::string&, const std::string&,
		const std::string&, const std::string&, int) override {}

private:
	std::string name;
};

// WrapScript这次真正接入Post查询：game_start那个milestone的changes数组最后一项是
// PlaceHolderChange{label:"control"}（见Resource/Story/test.json），命中后向Core查询
// "random citizen"，把结果姓名塞进controlChange（长期持有、永不delete，见script_mod.md
// "mod往actionStack里塞的指针必须是mod自己长期维护"的约定），原地替换掉actionStack顶层里
// 那个PlaceHolderChange。
class EmptyScript : public ScriptMod {
public:
	EmptyScript(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
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

		Expression citizenName;
		citizenName.Parse("\"" + result["name"].AsString() + "\"");
		controlChange.SetName(citizenName);

		actionStack.back()[idx] = static_cast<const Change*>(&controlChange);
	}

private:
	std::string name;
	ChangeControlChange controlChange;
};

class EmptyProduct : public ProductMod {
public:
	EmptyProduct(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};

class EmptyStorage : public StorageMod {
public:
	EmptyStorage(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};

class EmptyManufacture : public ManufactureMod {
public:
	EmptyManufacture(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};

class EmptyRoute : public RouteMod {
public:
	EmptyRoute(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};

class EmptyStation : public StationMod {
public:
	EmptyStation(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};

class EmptyVehicle : public VehicleMod {
public:
	EmptyVehicle(const std::string& args) { name = "empty(" + args + ")"; }

	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }

private:
	std::string name;
};
