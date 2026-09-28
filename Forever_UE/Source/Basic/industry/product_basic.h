#pragma once

#include "industry/product_mod.h"

#include <string>


// 阶段4-5工业系统测试案例——三个产品：小麦(无原料)、牛肉(无原料)、汉堡(小麦+牛肉→汉堡)，
// 用来验证Manufacture::ComputeRecipe()的批次配方展开算法和Industry::GlobalAllocate()
// 的全城统一调配，见Source/Core/industry/industry.md。三个类结构相同，都是"构造函数里
// 写死categories/batchSize/ingredients，SetProperty()留空"这个模式(见product_mod.h，
// 两段式约定不强制要求真的分两步，具体子类可以在构造函数里一次性赋完)。
//
// 具体数值(batchSize/ingredients的用量)都是占位测试值，跑通之后可以按实际观察到的
// 存量变化速度调整，不是这一阶段要抠准的东西。
//
// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+GetName里现拼。

class WheatProduct : public ProductMod {
public:
	WheatProduct();

	static const char* GetId() { return "product_wheat"; }
	virtual const char* GetType() const override { return "product_wheat"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override {}

private:
	static int count;
	int id;
	std::string name;
};

class BeefProduct : public ProductMod {
public:
	BeefProduct();

	static const char* GetId() { return "product_beef"; }
	virtual const char* GetType() const override { return "product_beef"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override {}

private:
	static int count;
	int id;
	std::string name;
};

class BurgerProduct : public ProductMod {
public:
	BurgerProduct();

	static const char* GetId() { return "product_burger"; }
	virtual const char* GetType() const override { return "product_burger"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override {}

private:
	static int count;
	int id;
	std::string name;
};
