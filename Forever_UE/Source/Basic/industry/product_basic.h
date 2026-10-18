#pragma once

#include "industry/product_mod.h"

#include <string>


// 工业系统测试案例——三个产品：小麦(无原料)、牛肉(无原料)、汉堡(小麦+牛肉→汉堡)，
// 用来验证Manufacture::ComputeRecipe()的批次配方展开算法和Industry::GlobalAllocate()
// 的全城统一调配，见Source/Core/industry/industry.md。三个类结构相同，构造函数只负责
// id登记，categories/batchSize/ingredients都在各自的SetProperty()里填，见product_mod.h。
//
// 具体数值(batchSize/ingredients的用量)都是占位测试值，跑通之后可以按实际观察到的
// 存量变化速度调整。
//
// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+GetName里现拼。

class WheatProduct : public ProductMod {
public:
	WheatProduct();

	static const char* GetId() { return "product_wheat"; }
	virtual const char* GetType() const override { return "product_wheat"; }
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

class BeefProduct : public ProductMod {
public:
	BeefProduct();

	static const char* GetId() { return "product_beef"; }
	virtual const char* GetType() const override { return "product_beef"; }
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

class BurgerProduct : public ProductMod {
public:
	BurgerProduct();

	static const char* GetId() { return "product_burger"; }
	virtual const char* GetType() const override { return "product_burger"; }
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
