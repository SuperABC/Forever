#pragma once

#include "map/component_mod.h"

#include <string>


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
