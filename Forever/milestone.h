#pragma once

#include "utility.h"
#include "dialog.h"
#include "change.h"
#include "event.h"

#include <string>


class Milestone {
public:
    Milestone(std::string name, std::shared_ptr<Event> trigger, bool visible, Condition keep,
        std::string description, std::string goal, std::vector<Dialog> dialogs, std::vector<std::shared_ptr<Change>> changes);
    ~Milestone();

    // 复合对象类型
    std::shared_ptr<Event> GetTrigger();
    std::vector<Dialog> GetDialogs();
    std::vector<std::shared_ptr<Change>> GetChanges();

    // 匹配事件
    bool MatchTrigger(std::shared_ptr<Event> e);

    // 获取参数
    std::string GetName();
    bool IsVisible();
    bool DropSelf(std::function<ValueType(const std::string&)> getValue);
    std::string GetDescription();
    std::string GetGoal();

private:
    std::string name;
    std::shared_ptr<Event> trigger;
    bool visible;
    Condition drop;
    std::string description;
    std::string goal;
    std::vector<Dialog> dialogs;
    std::vector<std::shared_ptr<Change>> changes;
};

class MilestoneNode {
public:
	MilestoneNode(Milestone milestone);

	Milestone content;

    // 前置数量
	int premise = 0;

    // 后置里程碑
	std::vector<MilestoneNode *> subsequents;
};
