#pragma once

#include "map/green_mod.h"


// 公园：在绿化多边形里放一个能容纳的最大活动圆，并在多边形范围内随机种树。
class ParkGreen : public GreenMod {
public:
	FOREVER_MOD_GENERIC_UNIT_TEST("green")
	ParkGreen();

	static const char* GetId() { return "green_park"; }
	virtual const char* GetType() const override { return "green_park"; }
	virtual const char* GetName() override;

	virtual void LayoutGreen() override;

	// 没连路网/连了路网的权重都是1。
	static float GetPowerFree();
	static float GetPowerLinked();

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
