#pragma once

#include "traffic/vehicle_mod.h"
#include "traffic/vehicle_factory.h"

#include <string>
#include <vector>

class Room;
class Script;
class ScriptFactory;

// Vehicle：一辆车的实体，独占持有一个VehicleMod实例——照抄Job持有JobMod的模式(见
// Source/Core/society/job.h)。阶段4-3只做到"能上下车、能开动"：只存名字、mod、当前世界
// transform(由UE侧AVehicleElement::Tick每帧写回，供将来Traffic/Room等系统查询用)，不考虑
// 车辆和房间/停车位的关系，不存driver。这次补上：
// - 一份自己的Script（和Job/Scheduler同一套机制，构造时按mod->scriptModName创建、按
//   mod->milestoneNames加载milestone），驱动"靠近车辆弹出'上车'选项"这个效果；
// - options列表（照抄Citizen::GetOptions/AddOption），供AddOptionChange/MeetOption UI使用；
// - 停车位信息(Room*+房间局部坐标+旋转)，由Traffic::Init()在生成时填入，供UE层
//   GenerateVehicles()换算世界坐标时读取。车辆本身的世界transform继续用现有的
//   SetTransform/GetTransform，不受这几个新字段影响。
// "谁在开这辆车"这份信息由UE侧AVehicleElement::previousPawn自己保留(上车前的pawn，下车时
// 要恢复谁)，不需要Core重复记一份，见vehicle.md。
class Vehicle {
public:
	Vehicle(VehicleFactory* factory, ScriptFactory* scriptFactory, const std::string& id, const std::string& name);
	~Vehicle(); // factory->DestroyVehicle(mod)；delete script

	// mod为空(CreateVehicle传入的id没有被注册/没有在config.json"vehicle_mods"里启用)
	// 说明这次构造失败，调用方应当整个丢弃这个Vehicle，见Traffic::CreateVehicle。
	bool IsValid() const;

	const std::string& GetName() const;
	std::string GetType() const; // mod->GetType()，mod为空返回空字符串
	const std::string& GetBlueprintPath() const; // mod->blueprintPath，mod为空返回空字符串
	float GetExitOffsetX() const;
	float GetExitOffsetY() const;
	float GetExitOffsetZ() const;

	Script* GetScript() const;
	const std::vector<std::string>& GetOptions() const;
	void AddOption(const std::string& option);

	// 停车位信息：由Traffic::Init()在创建这辆预置车辆时设置一次，之后不会再变——车辆被
	// 开走之后"停在哪"这件事不再有意义，这几个字段只服务于"生成时应该摆在世界哪个位置"
	// 这一次性用途。
	void SetParking(Room* room, float localX, float localY, float localRotationDegrees);
	Room* GetRoom() const;
	void GetParkingLocalPosition(float& outX, float& outY) const;
	float GetParkingRotationDegrees() const;

	void SetTransform(float x, float y, float z, float yaw);
	void GetTransform(float& outX, float& outY, float& outZ, float& outYaw) const;

private:
	VehicleFactory* factory;
	VehicleMod* mod;
	std::string name;
	Script* script;
	std::vector<std::string> options;

	Room* room = nullptr;
	float parkingLocalX = 0.f;
	float parkingLocalY = 0.f;
	float parkingRotationDegrees = 0.f;

	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float yaw = 0.0f;
};
