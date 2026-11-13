#pragma once

#include "society/job_mod.h"

#include "story/change.h"


class RestaurantWaiterJob : public JobMod {
public:
	RestaurantWaiterJob();

	static const char* GetId() { return "job_restaurant_waiter"; }
	virtual const char* GetType() const override { return "job_restaurant_waiter"; }
	virtual void DailyPlan(const Time& currentTime, PostHandle* post) override;
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
