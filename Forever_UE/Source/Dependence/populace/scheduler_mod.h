#pragma once

#include "common/utility.h"
#include "common/handle.h"

#include <string>
#include <unordered_map>
#include <vector>

class Change;

// SchedulerMod：每个市民挂一个的行为调度器定义。上班的行为已经完全由Citizen::GetJob()
// 负责（Populace::Tick驱动Job::DailyPlan/ExecNode），Scheduler这次负责的是下班之后的
// 行为——阶段3骨架只验证了Mod发现/加载机制本身（只有GetType/GetName两个纯虚接口），这次
// 补齐真正的业务接口，结构完全照抄JobMod（Source/Dependence/society/job_mod.h），DailyPlan/
// ExecNode这套调度机制同样和milestone/JSON完全独立，见job.md"DailyPlan/ExecNode"一节。
class SchedulerMod {
public:
	SchedulerMod() = default;
	virtual ~SchedulerMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// Script配置——和JobMod/OrganizationMod同一个"mod自己声明意图，Core只读"安全模式：
	// 两个普通成员字段，不通过返回值传递，具体子类应该在自己的构造函数里直接赋值。
	// scriptModName默认"empty"，milestoneNames默认为空，每项是不含路径/扩展名的bare
	// 文件名，Core通过Config::GetScriptPath()解析实际路径，见job_mod.h"Script配置"一节。
	std::string scriptModName = "empty";
	std::vector<std::string> milestoneNames;

	// 生成今天的调度表，直接写进this->plans(node名->今天触发的具体时间)。这次负责的是
	// citizen下班之后的行为（上班逻辑已经在JobMod::DailyPlan/ExecNode里）；这次迁移只
	// 搭接口形状，不写实际测试逻辑（该机制已经在Job/Organization验证过），具体子类的
	// DailyPlan留空即可。
	// @post：向Core发起查询的句柄，同JobMod::DailyPlan，不需要这个能力可以忽略。
	virtual void DailyPlan(const Time& currentTime, PostHandle* post) = 0;

	// 执行一个到期节点：直接new Change*写进this->changes(所有权留在这个mod实例身上，
	// 不转移给调用方)。调用前调用方已经保证清空了上一次的残留（见Scheduler::ExecNode）。
	// 不需要对某个节点做任何事的类型不用重写。
	virtual void ExecNode(const std::string& node, PostHandle* post) {}

	// 当前绑定的市民姓名——由Scheduler在调用ExecNode前同步进来，供ExecNode里构造需要
	// 知道"是谁"的Change使用，和JobMod::occupantName同一个字段用途。
	std::string occupantName;

	std::unordered_map<std::string, Time> plans;
	std::vector<Change*> changes;
};
