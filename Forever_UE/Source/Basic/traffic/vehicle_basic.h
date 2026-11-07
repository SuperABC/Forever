#pragma once

#include "traffic/vehicle_mod.h"

#include <string>


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

class TruckVehicle : public VehicleMod {
public:
	TruckVehicle();

	static const char* GetId() { return "vehicle_truck"; }
	static const char* GetCategory() { return "truck"; }
	virtual const char* GetType() const override { return "vehicle_truck"; }
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
