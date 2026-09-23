#pragma once

#include "traffic/vehicle_mod.h"
#include "traffic/vehicle_factory.h"

#include <string>


// Vehicle：一辆车的实体，独占持有一个VehicleMod实例——照抄Job持有JobMod的模式(见
// Source/Core/society/job.h)。这一阶段(阶段4-3)只做到"能上下车、能开动"：只存名字、mod、
// 当前世界transform(由UE侧AVehicleElement::Tick每帧写回，供将来Traffic/Room等系统查询用)，
// 不考虑车辆和房间/停车位的关系，不存driver——"谁在开这辆车"这份信息由UE侧
// AVehicleElement::previousPawn自己保留(上车前的pawn，下车时要恢复谁)，不需要Core重复
// 记一份，见vehicle.md。
class Vehicle {
public:
	Vehicle(VehicleFactory* factory, const std::string& id, const std::string& name);
	~Vehicle(); // factory->DestroyVehicle(mod)

	// mod为空(CreateVehicle传入的id没有被注册/没有在config.json"vehicle_mods"里启用)
	// 说明这次构造失败，调用方应当整个丢弃这个Vehicle，见Traffic::CreateVehicle。
	bool IsValid() const;

	const std::string& GetName() const;
	std::string GetType() const; // mod->GetType()，mod为空返回空字符串
	const std::string& GetBlueprintPath() const; // mod->blueprintPath，mod为空返回空字符串

	void SetTransform(float x, float y, float z, float yaw);
	void GetTransform(float& outX, float& outY, float& outZ, float& outYaw) const;

private:
	VehicleFactory* factory;
	VehicleMod* mod;
	std::string name;

	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float yaw = 0.0f;
};
