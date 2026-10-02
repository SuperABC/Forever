#pragma once

#include "traffic/vehicle.h"
#include "traffic/vehicle_factory.h"

#include "common/class.h"
#include "common/handle.h"

#include <string>
#include <unordered_map>

class Map;
class ScriptFactory;
struct ScriptContext;

// 阶段4-3：Route/Station两个Mod扩展点仍然是空骨架，业务聚合逻辑留到后续再做（见
// PHASE4_PLAN.md）；Vehicle这一份先落地"能上下车、能开动"这个最小闭环——按name索引持有
// 全部当前存在的Vehicle*。这次补上Init(Map*)：开局时遍历所有停车位房间，预置生成车辆
// （不再靠玩家按键临时生成），以及ApplyChange认识AddOptionChange(给车辆加"上车"选项)。
// 注意：这个类和UForeverTrafficFrameworkComponent（UE层Traffic域组件，上下车的Possess
// 切换逻辑在那边，见ForeverTrafficFrameworkComponent.md）不是一回事，Traffic不知道UE
// Actor/Controller的存在，两者没有互相持有关系。
class Traffic {
public:
	Traffic(); // 绑定Registry::Get().GetVehicleFactory()/GetScriptFactory()，mod注册不在这里做
	~Traffic(); // delete全部vehicles

	// 遍历map所有building的所有room，对每个IsParking()的room、每个ParkingSpot生成一辆预置
	// 车辆(车型从VehicleFactory::GetRegisteredIds()里排除"empty"后随机挑一个，没有可用车型
	// 就跳过这个车位，不报错，照抄Phone::BuildAppList跳过"empty"的思路)，调
	// vehicle->SetParking(room, localX, localY, spot.rotationDegrees)记下停车位，供UE层
	// GenerateVehicles()换算世界坐标。假定map已经生成好(EnsureMapGenerated()已经跑过)，
	// 调用方(AForeverFrameworkActor::EnsureTrafficGenerated)负责保证调用顺序。
	void Init(Map* map);

	// 用VehicleFactory以id创建一辆新车、按name存进vehicles(name已存在会先delete旧的)。
	// 创建失败(id未注册/未在config.json"vehicle_mods"启用)返回nullptr，不会往vehicles里塞
	// 半成品。
	Vehicle* CreateVehicle(const std::string& modId, const std::string& name);
	void DestroyVehicle(const std::string& name);
	Vehicle* FindVehicleByName(const std::string& name) const;
	const std::unordered_map<std::string, Vehicle*>& GetVehicles() const;

	// 阶段占位：Traffic域这次还没有真正迁移每帧逻辑，空实现——和Map/Populace/Society/
	// Industry/Story一起被AForeverFrameworkActor::Tick统一调用一遍，保持"每个域都有Tick"
	// 这个形状一致，等Traffic域真正落地时再补内容。
	void Tick(const Time& currentTime, bool crossedDay, PostHandle* post);

	// 认识AddOptionChange：按name找Vehicle、调vehicle->AddOption(option)，照抄
	// Populace::ApplyChange对应分支。EnterVehicleChange是UE-only效果(找AVehicleElement、
	// Possess)，不在这里处理，和ChangeControlChange一样直接在
	// AForeverFrameworkActor::ApplyChange里dynamic_cast转发给UE层。
	void ApplyChange(const Change* change, const ScriptContext& context);

private:
	VehicleFactory& vehicleFactory;
	ScriptFactory& scriptFactory;

	std::unordered_map<std::string, Vehicle*> vehicles; // 持有所有权
	int vehicleCounter = 0; // 生成预置车辆时拼唯一name用，如"Vehicle0"
};
