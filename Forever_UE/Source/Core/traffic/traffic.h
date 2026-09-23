#pragma once

#include "traffic/vehicle.h"
#include "traffic/vehicle_factory.h"

#include "common/class.h"
#include "common/handle.h"

#include <string>
#include <unordered_map>

struct ScriptContext;

// 阶段4-3：Route/Station两个Mod扩展点仍然是空骨架，业务聚合逻辑留到后续再做（见
// PHASE4_PLAN.md）；Vehicle这一份先落地"能上下车、能开动"这个最小闭环——按name索引持有
// 全部当前存在的Vehicle*，供UE层上下车时创建/销毁。注意：这个类和
// UForeverTrafficFrameworkComponent（UE层Traffic域组件，上下车的Possess切换逻辑在那边，
// 见ForeverTrafficFrameworkComponent.md）不是一回事，Traffic不知道UE Actor/Controller的
// 存在，两者没有互相持有关系。
class Traffic {
public:
	Traffic(); // 绑定Registry::Get().GetVehicleFactory()，mod注册不在这里做
	~Traffic(); // delete全部vehicles

	// 用VehicleFactory以id创建一辆新车、按name存进vehicles(name已存在会先delete旧的，这一
	// 阶段不要求全局唯一性校验之外的东西，纯测试用途)。modId这一阶段先由调用方固定传
	// "vehicle_basic"（VehicleBasic测试车型），将来做车辆种类选择时改成参数。创建失败
	// (id未注册/未在config.json"vehicle_mods"启用)返回nullptr，不会往vehicles里塞半成品。
	Vehicle* CreateVehicle(const std::string& modId, const std::string& name);
	void DestroyVehicle(const std::string& name);
	Vehicle* FindVehicleByName(const std::string& name) const;
	const std::unordered_map<std::string, Vehicle*>& GetVehicles() const;

	// 阶段占位：Traffic域这次还没有真正迁移每帧逻辑，空实现——和Map/Populace/Society/
	// Industry/Story一起被AForeverFrameworkActor::Tick统一调用一遍，保持"每个域都有Tick"
	// 这个形状一致，等Traffic域真正落地时再补内容。
	void Tick(const Time& currentTime, bool crossedDay, PostHandle* post);

	// 阶段占位：目前没有任何Change子类是Traffic域自己认识、需要处理的，空实现——
	// AForeverFrameworkActor::ApplyChange会把同一个Change转发给全部六个域，这里不打"未实现"
	// 警告（避免同一个Change被六个域各打一遍重复警告），唯一的兜底警告在Story::ApplyChange。
	void ApplyChange(const Change* change, const ScriptContext& context);

private:
	VehicleFactory& vehicleFactory;

	std::unordered_map<std::string, Vehicle*> vehicles; // 持有所有权
};
