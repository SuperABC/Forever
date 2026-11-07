#pragma once

#include "populace/scheduler_mod.h"

#include <string>


class SchedulerBasic : public SchedulerMod {
public:
	SchedulerBasic();

	static const char* GetId() { return "scheduler_basic"; }
	virtual const char* GetType() const override { return "scheduler_basic"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

	// 注册时作为权重传给SchedulerFactory::RegisterScheduler，见scheduler_basic.md。
	static float GetPower() { return 1.f; }

	virtual void DailyPlan(const Time& currentTime, PostHandle* post) override {}

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
