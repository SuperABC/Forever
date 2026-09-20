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

	/*
	* 构造里程碑
	* @name: 里程碑名称
	* @triggers: 触发事件列表
	* @visible: 是否对玩家可见
	* @drop: 一次性失效条件
	* @description: 里程碑描述文本
	* @goal: 里程碑目标文本
	* @changes: 触发后的变更列表（先于dialogs执行，见Milestone.md"changes在dialogs之前"一节）
	* @dialogs: 关联对话列表（延迟求值，见Dialog.h整体注释）
	* @subsequences: 后续里程碑名称列表
	*/
	Milestone(std::string name, std::vector<Event*> triggers, bool visible, std::string drop, std::string description,
		std::string goal, std::vector<Change*> changes, std::vector<Dialog*> dialogs, std::vector<std::string> subsequences);

	/*
	* 析构里程碑：delete triggers/dialogs/changes本体
	*/
	~Milestone();

	/*
	* 获取触发事件列表
	* @return: 事件指针列表
	*/
	std::vector<Event*> GetTriggers() const;

	/*
	* 获取变更列表（先于GetDialogs执行，见Milestone.md"changes在dialogs之前"一节）
	* @return: 变更指针列表
	*/
	std::vector<Change*> GetChanges() const;

	/*
	* 获取关联对话列表
	* @return: 对话指针列表
	*/
	std::vector<Dialog*> GetDialogs() const;

	/*
	* 判断给定事件是否匹配本里程碑的某个触发事件
	* @e: 待匹配的运行时事件
	* @context: 变量路由上下文
	* @return: 匹配则返回 true，否则返回 false
	*/
	bool MatchTrigger(Event* e, const ScriptContext& context) const;

	/*
	* 获取里程碑名称
	* @return: 里程碑名称字符串
	*/
	std::string GetName() const;

	/*
	* 判断里程碑是否对玩家可见
	* @return: 可见返回 true，否则返回 false
	*/
	bool IsVisible() const;

	/*
	* 获取一次性失效条件表达式
	* @return: 失效条件
	*/
	std::string DropCondition() const;

	/*
	* 根据当前变量值求值失效条件，判断这次匹配成功后是否应让本里程碑失效
	* @context: 变量路由上下文
	* @return: 应失效返回 true，否则返回 false
	*/
	bool DropSelf(const ScriptContext& context) const;

	/*
	* 获取里程碑描述文本
	* @return: 描述字符串
	*/
	std::string GetDescription() const;

	/*
	* 获取里程碑目标文本
	* @return: 目标字符串
	*/
	std::string GetGoal() const;

	/*
	* 获取后续里程碑名称列表
	* @return: 名称字符串列表
	*/
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

	/*
	* 默认构造空里程碑节点
	*/
	MilestoneNode();

	/*
	* 构造持有指定里程碑的节点
	* @milestone: 里程碑指针
	*/
	MilestoneNode(Milestone* milestone);

	// 关联的里程碑内容（引用，不持有所有权——本体挂在Script::milestones里）
	Milestone* content;

	// 前置未满足数量，降为0时从"未解锁"变为"active参与匹配"
	int premise;

	// 后置里程碑节点列表
	std::vector<MilestoneNode*> subsequents;
};
