#pragma once

#include "industry/manufacture_mod.h"

#include <string>


// 阶段4-5工业系统测试案例——三个工坊：农场(无原料产小麦)/牧场(无原料产牛肉)/
// 食品加工厂(小麦+牛肉→汉堡，配方写在BurgerProduct身上，见product_basic.h)，
// 用来验证Industry::Tick()每日三阶段(WorkAccount→GlobalAllocate→StartProduce)，
// 见Source/Core/industry/industry.md。targets的value(每天产几批)是占位测试值。
//
// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+GetName里现拼。

class FarmManufacture : public ManufactureMod {
public:
	FarmManufacture();

	static const char* GetId() { return "manufacture_farm"; }
	virtual const char* GetType() const override { return "manufacture_farm"; }
	virtual const char* GetName() override;
	virtual void SetTargets() override;

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

private:
	static int count;
	int id;
	std::string name;
};
