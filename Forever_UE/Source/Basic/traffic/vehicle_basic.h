#pragma once

#include "traffic/vehicle_mod.h"

#include <string>


// 阶段4-3:第一个真正提供内容的测试车型——构造函数里把meshPath写死指向项目里现成的测试
// 小汽车静态网格(Content/3rdParty/Cars_for_Arcade_Demolition_Racing_Games/StaticMeshes/
// Car.uasset)，供AVehicleElement::Init()加载。Basic现在编译为DynamicLibrary(Basic.dll)，
// 和Forever_Mod下的Mod一样由Config/ModLoader在运行时扫描加载,不会静态链进
// Forever.Build.cs,见 Source/Basic/README.md。
//
// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+GetName里现拼，和
// Source/Basic/map/terrain_basic.h/.cpp的OceanTerrain同一个模式(之前这里是固定字符串，
// 没有任何唯一性)。
class VehicleBasic : public VehicleMod {
public:
	VehicleBasic();

	static const char* GetId() { return "vehicle_basic"; }
	virtual const char* GetType() const override { return "vehicle_basic"; }
	virtual const char* GetName() override;

private:
	static int count;
	int id;
	std::string name;
};
