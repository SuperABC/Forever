#pragma once

#include "populace/scheduler_mod.h"
#include "populace/scheduler_factory.h"

#include "common/utility.h"

#include <string>
#include <unordered_map>
#include <vector>

class Citizen;
class Script;
class ScriptFactory;
class Change;

// Scheduler：每个市民独占持有一个（Citizen自己持有所有权，见citizen.h）。和Job（由
// Organization持有、构造时未必已经绑定occupant、之后靠SetOccupant事后绑定）不一样——
// Scheduler没有类似"组织"这样的自然归属者，从new出来那一刻就唯一绑定一个Citizen，构造
// 函数直接收Citizen*，不需要事后SetOccupant，详见scheduler.md。
class Scheduler {
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
