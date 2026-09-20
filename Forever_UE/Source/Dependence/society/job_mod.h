#pragma once

#include "common/utility.h"

#include <string>
#include <unordered_map>
#include <vector>

class Change;

// JobMod：一份具体工作类型的行为定义(比如店员)。这次迁移不要Calendar——"哪几天上班/几点
// 上下班"直接写进具体JobMod子类的DailyPlan里，不再有独立的Calendar概念。
//
// DailyPlan/ExecNode这次调度机制完全是C++直接构造Change*，和milestone/JSON没有任何关系
// （不做"按节点名找milestone触发"这种合并——milestone只能通过它自己原本的Event广播+
// Script::MatchEvent触发器匹配流程被触发，两者运行时完全独立，见job.md）。plans/changes
// 是这个mod实例自己的成员字段：DailyPlan/ExecNode是具体子类重写的虚方法，直接写自己的
// 成员字段（分配/写入/析构全程都在这个mod实例所在的同一侧，Core侧只读，不delete这里面
// 的Change*），和BuildingMod::floors/singles/rows已经在用的跨DLL安全模式一致，见
// Source/Core/society/job.md"跨DLL数据传递约束"一节。
class JobMod {
public:
	JobMod() = default;
	virtual ~JobMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 这个job类型要加载的milestone json文件名列表(不含路径/扩展名)——和下面的
	// DailyPlan/ExecNode完全无关，纯粹是给这个Job独占的Script提供milestone内容，将来
	// 由外部广播一个Event、走Script::MatchEvent的正常触发器匹配流程触发。不需要这个
	// 能力的job类型不用重写，用默认空实现。
	virtual std::vector<std::string> GetMilestoneFiles() const { return {}; }

	// 生成今天的调度表，直接写进this->plans(node名->今天触发的具体时间)。
	virtual void DailyPlan(const Time& currentTime) = 0;

	// 执行一个到期节点：直接new Change*写进this->changes(所有权留在这个mod实例身上，
	// 不转移给调用方)。调用前调用方已经保证清空了上一次的残留（见Job::ExecNode）。不需要
	// 对某个节点做任何事的job类型不用重写。
	virtual void ExecNode(const std::string& node) {}

	// 当前持有这份工作的市民姓名——由Job在调用ExecNode前同步进来，供ExecNode里构造
	// NPCNavigateChange这类需要知道"是谁"的Change使用，不代表这个mod持有Citizen（
	// Dependence层看不到Citizen这个Core类型，只能传字符串）。
	std::string occupantName;

	std::unordered_map<std::string, Time> plans;
	std::vector<Change*> changes;
};
