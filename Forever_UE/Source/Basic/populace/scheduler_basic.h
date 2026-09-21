#pragma once

#include "populace/scheduler_mod.h"

#include <string>


// 默认的Scheduler实现——GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+
// GetName里现拼，和Source/Basic/map/terrain_basic.h/.cpp的OceanTerrain同一个模式。
// 这次迁移不写实际测试逻辑（DailyPlan/ExecNode这套机制已经在Job/Organization验证过），
// DailyPlan留空、不产出任何调度节点，和EmptyJob::DailyPlan同一个"占位，不产生任何调度"
// 做法；Script配置指向一个空的schedule_empty.script，见scheduler_basic.cpp、
// Resource/Story/schedule_empty.script。
class SchedulerBasic : public SchedulerMod {
public:
	SchedulerBasic();

	static const char* GetId() { return "scheduler_basic"; }
	virtual const char* GetType() const override { return "scheduler_basic"; }
	virtual const char* GetName() override;

	// 供RegisterModSchedulers注册时传给SchedulerFactory::RegisterScheduler当权重——
	// Populace::AssignSchedulers()据此建CDF做加权随机分配，见populace.md。
	static float GetPower() { return 1.f; }

	virtual void DailyPlan(const Time& currentTime, PostHandle* post) override {}

private:
	static int count;
	int id;
	std::string name;
};
