#pragma once

#include <string>
#include <functional>

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
#include "society/calendar_mod.h"
#include "society/organization_mod.h"
#include "story/script_mod.h"
#include "industry/product_mod.h"
#include "industry/storage_mod.h"
#include "industry/manufacture_mod.h"
#include "traffic/route_mod.h"
#include "traffic/station_mod.h"
#include "traffic/vehicle_mod.h"

// 阶段3示例mod:验证"config.json按mod id配置的命令行式参数"能透传到mod。21个concept各
// 一个EmptyXxx,id统一为"empty"(不同concept的Factory registries互相独立,id不冲突,和
// 旧工程"empty"这个占位mod id的用法一致)。参数不在注册时传给mod自己,而是走
// <Concept>Factory::SetModArgs+ApplyArgs这条统一链路(config.json"<concept>_mods"数组
// 里"empty --test true"这种写法解析出的参数字符串,由Create<Concept>创建实例后自动调用
// ApplyArgs传入)——mod自己不用关心参数从哪来,只需要重写ApplyArgs接收即可。
// GetName()把ApplyArgs收到的参数字符串拼进返回值,方便在ForeverModSubsystem的验证日志
// 里确认参数确实从config.json一路传到了这里。真正的业务逻辑本阶段不需要,阶段4对应
// 系统落地时,这类占位mod的定位和旧工程"empty"系列mod一致(空实现兜底)。

class EmptyTerrain : public TerrainMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

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
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyZone : public ZoneMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyBuilding : public BuildingMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyComponent : public ComponentMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyRoom : public RoomMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyAsset : public AssetMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyApp : public AppMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyPuzzle : public PuzzleMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyName : public NameMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyScheduler : public SchedulerMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyJob : public JobMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyCalendar : public CalendarMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyOrganization : public OrganizationMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyScript : public ScriptMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyProduct : public ProductMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyStorage : public StorageMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyManufacture : public ManufactureMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyRoute : public RouteMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyStation : public StationMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};

class EmptyVehicle : public VehicleMod {
public:
	static const char* GetId() { return "empty"; }
	virtual const char* GetType() const override { return "empty"; }
	virtual const char* GetName() override { return name.data(); }
	virtual void ApplyArgs(const std::string& args) override { name = "empty(" + args + ")"; }

private:
	std::string name;
};
