#pragma once

#include "class.h"

#include "populace/scheduler_mod.h"
#include "populace/scheduler_factory.h"

#include "common/utility.h"

#include <string>
#include <unordered_map>
#include <vector>


// Scheduler：每个市民独占持有一个，构造时就绑定Citizen，详见scheduler.md。
class Scheduler {
	// 存档：PopulaceSaveCodec直接读写私有字段(populace_save.md)。
	friend class PopulaceSaveCodec;
public:
	Scheduler() = delete;
	Scheduler(SchedulerFactory* factory, ScriptFactory* scriptFactory, const std::string& id, Citizen* citizen);
	~Scheduler(); // factory->DestroyScheduler(mod)；delete script

	std::string GetType() const;
	Script* GetScript() const;

	void DailyPlan(const Time& currentTime, PostHandle* post);
	const std::unordered_map<std::string, Time>& GetPlans() const;

	std::vector<Change*> ExecNode(const std::string& node, PostHandle* post);

private:
	SchedulerMod* mod;
	SchedulerFactory* factory;
	Script* script;
	Citizen* citizen; // 不持有所有权，只用于ExecNode前同步occupantName
};
