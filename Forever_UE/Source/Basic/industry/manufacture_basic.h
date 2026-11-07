#pragma once

#include "industry/manufacture_mod.h"

#include <string>


class FarmManufacture : public ManufactureMod {
public:
	FarmManufacture();

	static const char* GetId() { return "manufacture_farm"; }
	virtual const char* GetType() const override { return "manufacture_farm"; }
	virtual const char* GetName() override;
	virtual void SetTargets() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class RanchManufacture : public ManufactureMod {
public:
	RanchManufacture();

	static const char* GetId() { return "manufacture_ranch"; }
	virtual const char* GetType() const override { return "manufacture_ranch"; }
	virtual const char* GetName() override;
	virtual void SetTargets() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class FoodFactoryManufacture : public ManufactureMod {
public:
	FoodFactoryManufacture();

	static const char* GetId() { return "manufacture_food_factory"; }
	virtual const char* GetType() const override { return "manufacture_food_factory"; }
	virtual const char* GetName() override;
	virtual void SetTargets() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
