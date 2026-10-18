#pragma once

#include "map/component_mod.h"

#include <string>


// ResidenceComponent/ShopComponent/FactoryComponent：Component域默认内容，三个都是trivial
// 占位(InitComponent逻辑本来就是空的)，合并进同一份component_basic.h/.cpp(不按
// residence/shop/plant各开一个文件)，和terrain_basic.h/.cpp里OceanTerrain/MountainTerrain
// 合并的方式一样。
// Basic编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由Config/ModLoader在
// 运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
//
// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+GetName里现拼，和
// Source/Basic/map/terrain_basic.h/.cpp的OceanTerrain同一个模式。
class ResidenceComponent : public ComponentMod {
public:
	ResidenceComponent();

	static const char* GetId() { return "component_residence"; }
	virtual const char* GetType() const override { return "component_residence"; }
	virtual const char* GetName() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class ShopComponent : public ComponentMod {
public:
	ShopComponent();

	static const char* GetId() { return "component_shop"; }
	virtual const char* GetType() const override { return "component_shop"; }
	virtual const char* GetName() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class FactoryComponent : public ComponentMod {
public:
	FactoryComponent();

	static const char* GetId() { return "component_factory"; }
	virtual const char* GetType() const override { return "component_factory"; }
	virtual const char* GetName() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// TrainStationComponent/AirportComponent：公共交通站点建筑用的trivial占位component，和上面
// 三个一样InitComponent逻辑本来就是空的(公交站不挂建筑，不需要这个，见
// station_basic.h的BusStation)。
class TrainStationComponent : public ComponentMod {
public:
	TrainStationComponent();

	static const char* GetId() { return "component_train_station"; }
	virtual const char* GetType() const override { return "component_train_station"; }
	virtual const char* GetName() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class AirportComponent : public ComponentMod {
public:
	AirportComponent();

	static const char* GetId() { return "component_airport"; }
	virtual const char* GetType() const override { return "component_airport"; }
	virtual const char* GetName() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
