#pragma once

#include "common/mod_state.h"
#include "common/utility.h"
#include "common/handle.h"

#include <string>
#include <unordered_map>
#include <vector>

class Change;

// 设计见scheduler_mod.md。
class SchedulerMod {
public:
	SchedulerMod() = default;
	virtual ~SchedulerMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 子类在这里填scriptModName/milestoneNames(不在构造函数里),Core创建实例后立刻调一次。
	virtual void SetProperty() {}

	// Script配置,同JobMod,见scheduler_mod.md。
	std::string scriptModName; // 空=无自定义ScriptMod;和milestoneNames都空则实例没有Script(GetScript()==nullptr),见MakeScript
	std::vector<std::string> milestoneNames;

	// 生成今天的调度表,写进this->plans(node名->触发时间)。负责下班之后的行为。post同JobMod::DailyPlan。
	virtual void DailyPlan(const Time& currentTime, PostHandle* post) = 0;

	// 执行一个到期节点:new Change*写进this->changes(所有权留在mod实例上),调用前已清空残留。
	virtual void ExecNode(const std::string& node, PostHandle* post) {}

	// 当前绑定的市民姓名,由Scheduler在调用ExecNode前同步,同JobMod::occupantName。
	std::string occupantName;

	std::unordered_map<std::string, Time> plans;
	std::vector<Change*> changes;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
