#pragma once

#include "map/component_mod.h"

#include <string>


// Component域默认内容：五个类型都是无字段无逻辑的trivial占位，合并在本文件，详见component_basic.md。
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

class OfficeComponent : public ComponentMod {
public:
	OfficeComponent();

	static const char* GetId() { return "component_office"; }
	virtual const char* GetType() const override { return "component_office"; }
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

// TrainStationComponent/AirportComponent：火车站/机场建筑用的占位component。
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
