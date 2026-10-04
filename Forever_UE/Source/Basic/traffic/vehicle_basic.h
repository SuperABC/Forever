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

// BusVehicle/TrainVehicle/PlaneVehicle：公共交通线路车辆——blueprintPath留空，UE层
// ATransitVehicleElement按sizeX/Y/Z缩放/Engine/BasicShapes/Cube占位；drivable=boardable=
// false(这次不做操控/搭乘，只预留字段，见public_transport_plan.md"5."一节)。三个类型合并进
// 同一份vehicle_basic.h/.cpp，和VehicleBasic同一个文件组织约定(见上VehicleBasic注释)。
class BusVehicle : public VehicleMod {
public:
	BusVehicle();

	static const char* GetId() { return "vehicle_bus"; }
	virtual const char* GetType() const override { return "vehicle_bus"; }
	virtual const char* GetName() override;

private:
	static int count;
	int id;
	std::string name;
};

class TrainVehicle : public VehicleMod {
public:
	TrainVehicle();

	static const char* GetId() { return "vehicle_train"; }
	virtual const char* GetType() const override { return "vehicle_train"; }
	virtual const char* GetName() override;

private:
	static int count;
	int id;
	std::string name;
};

class PlaneVehicle : public VehicleMod {
public:
	PlaneVehicle();

	static const char* GetId() { return "vehicle_plane"; }
	virtual const char* GetType() const override { return "vehicle_plane"; }
	virtual const char* GetName() override;

private:
	static int count;
	int id;
	std::string name;
};
