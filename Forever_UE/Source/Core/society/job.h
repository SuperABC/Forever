#pragma once

#include "society/job_mod.h"
#include "society/job_factory.h"

#include "common/utility.h"

#include <string>
#include <unordered_map>
#include <vector>

class Room;
class Citizen;
class Script;
class ScriptFactory;
class Change;

// Job：一份具体工作岗位的实体，独占持有一个JobMod实例+一份自己的Script（self.前缀路由
// 目标，构造时按mod->GetMilestoneFiles()加载milestone——这份milestone和DailyPlan/ExecNode
// 调度机制完全无关，只服务于这个Script将来独立接收Event广播、走Script::MatchEvent的正常
// 触发器匹配流程，见job.md）。**Job由Organization持有所有权**，Citizen只存一个不持有
// 所有权的Job*指针（Citizen::GetJob()/SetJob()）。
class Job {
public:
	Job(JobFactory* factory, ScriptFactory* scriptFactory, const std::string& id, Room* position);
	~Job(); // factory->DestroyJob(mod)；delete script

	std::string GetType() const;
	Room* GetPosition() const; // 工位room，不持有
	Script* GetScript() const;

	Citizen* GetOccupant() const; // 当前持有这份工作的市民，不持有
	void SetOccupant(Citizen* citizen);

	// 生成今天的调度表：转调mod->DailyPlan(currentTime)，结果就是mod->plans，这里只读。
	void DailyPlan(const Time& currentTime);
	const std::unordered_map<std::string, Time>& GetPlans() const;

	// 执行一个到期节点：先把occupant姓名同步给mod（供mod::ExecNode构造Change时使用），
	// 转调mod->ExecNode(node)（mod自己负责清空上一次残留的changes），只读
	// mod->GetChanges()把指针值复制进这里新建的vector返回——不delete、不接管这些
	// Change*的所有权，它们留给mod自己在下一次ExecNode或最终析构时清理，见job.md
	// "跨DLL数据传递约束"一节。这次的调度机制不经过milestone/JSON。
	std::vector<Change*> ExecNode(const std::string& node);

private:
	JobMod* mod;
	JobFactory* factory;
	Script* script;
	Room* position;
	Citizen* occupant = nullptr;
};
