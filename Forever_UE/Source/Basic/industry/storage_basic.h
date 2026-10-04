#pragma once

#include "industry/storage_mod.h"

#include <string>


// 阶段4-5工业系统测试案例——三个仓库：小麦仓库/牛肉仓库/汉堡仓库，categories分别对应
// product_basic.h里三个产品的categories，见Source/Core/industry/industry.md。
// capacity是占位测试值，跑通之后按实际观察到的存量变化速度调整。
//
// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+GetName里现拼。

class WheatStorage : public StorageMod {
public:
	WheatStorage();

	static const char* GetId() { return "storage_wheat"; }
	virtual const char* GetType() const override { return "storage_wheat"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};

class BeefStorage : public StorageMod {
public:
	BeefStorage();

	static const char* GetId() { return "storage_beef"; }
	virtual const char* GetType() const override { return "storage_beef"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};

class BurgerStorage : public StorageMod {
public:
	BurgerStorage();

	static const char* GetId() { return "storage_burger"; }
	virtual const char* GetType() const override { return "storage_burger"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};
