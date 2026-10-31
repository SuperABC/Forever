#pragma once

#include "traffic/vehicle_mod.h"

#include <string>


// 测试车型——构造函数里把meshPath写死指向项目里现成的测试小汽车静态网格
// (Content/3rdParty/Cars_for_Arcade_Demolition_Racing_Games/StaticMeshes/Car.uasset)，
// 供AVehicleElement::Init()加载。Basic编译为DynamicLibrary(Basic.dll)，和Forever_Mod下的
// Mod一样由Config/ModLoader在运行时扫描加载,不会静态链进Forever.Build.cs,见
// Source/Basic/README.md。
//
// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+GetName里现拼，和
// Source/Basic/map/terrain_basic.h/.cpp的OceanTerrain同一个模式。
class VehicleBasic : public VehicleMod {
public:
	VehicleBasic();

	static const char* GetId() { return "vehicle_basic"; }
	static const char* GetCategory() { return "car"; }
	virtual const char* GetType() const override { return "vehicle_basic"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// BusVehicle/TrainVehicle/PlaneVehicle：公共交通线路车辆——blueprintPath留空，UE层
// ATransitVehicleElement按sizeX/Y/Z缩放/Engine/BasicShapes/Cube占位；drivable=boardable=
// false(不做操控/搭乘，只预留字段)。三个类型合并进同一份vehicle_basic.h/.cpp，和
// VehicleBasic同一个文件组织约定。
class BusVehicle : public VehicleMod {
public:
	BusVehicle();

	static const char* GetId() { return "vehicle_bus"; }
	static const char* GetCategory() { return "bus"; }
	virtual const char* GetType() const override { return "vehicle_bus"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class TrainVehicle : public VehicleMod {
public:
	TrainVehicle();

	static const char* GetId() { return "vehicle_train"; }
	static const char* GetCategory() { return "train"; }
	virtual const char* GetType() const override { return "vehicle_train"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class PlaneVehicle : public VehicleMod {
public:
	PlaneVehicle();

	static const char* GetId() { return "vehicle_plane"; }
	static const char* GetCategory() { return "plane"; }
	virtual const char* GetType() const override { return "vehicle_plane"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
