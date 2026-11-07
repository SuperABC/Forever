#pragma once

#include "class.h"

#include "story/expression.h"

#include <string>
#include <vector>

// 里程碑：Script的最小调度单元。触发/对话/变化三个列表的指针语义见Dialog.h/Change.h整体
// 注释——triggers/dialogs/changes都是Milestone自己new出来、自己在析构时delete的本体
// (OBJECT_HOLDER)，别处引用它们一律用裸指针，不重复delete。
class Milestone {
public:

	// changes先于dialogs执行(见milestone.md)，subsequences是顺序解锁用的后续里程碑名。
	Milestone(std::string name, std::vector<Event*> triggers, bool visible, std::string drop, std::string description,
		std::string goal, std::vector<Change*> changes, std::vector<Dialog*> dialogs, std::vector<std::string> subsequences);

	~Milestone(); // delete triggers/dialogs/changes本体

	std::vector<Event*> GetTriggers() const;

	std::vector<Change*> GetChanges() const;

	std::vector<Dialog*> GetDialogs() const;

	// 任一触发事件的条件门+类型+字段全部匹配即返回true。
	bool MatchTrigger(Event* e, const ScriptContext& context) const;

	std::string GetName() const;

	bool IsVisible() const;

	std::string DropCondition() const;

	// 求值失效条件：匹配成功后为true则本里程碑退出actives。
	bool DropSelf(const ScriptContext& context) const;

	std::string GetDescription() const;

	std::string GetGoal() const;

	std::vector<std::string> GetSubsequences() const;

private:

	// 里程碑名称
	std::string name;

	// 触发事件列表（持有所有权）
	std::vector<Event*> triggers;

	// 是否对玩家可见
	bool visible;

	// 一次性失效条件：匹配成功后求值为true则本里程碑退出actives、不再参与后续匹配
	std::string drop;

	// 里程碑描述文本
	std::string description;

	// 里程碑目标文本
	std::string goal;

	// 关联变更列表（持有所有权，先于dialogs执行，见Milestone.md"changes在dialogs之前"一节）
	std::vector<Change*> changes;

	// 关联对话列表（持有所有权，延迟求值）
	std::vector<Dialog*> dialogs;

	// 后续里程碑名称列表
	std::vector<std::string> subsequences;
};

// 里程碑顺序解锁用的调度节点：只有premise降为0的节点才会被加入Script::actives参与匹配。
class MilestoneNode {
public:

	MilestoneNode();

	MilestoneNode(Milestone* milestone);

	// 关联的里程碑内容（引用，不持有所有权——本体挂在Script::milestones里）
	Milestone* content;

	// 前置未满足数量，降为0时从"未解锁"变为"active参与匹配"
	int premise;

	// 后置里程碑节点列表
	std::vector<MilestoneNode*> subsequents;
};
