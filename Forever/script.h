#pragma once

#include "utility.h"
#include "json.h"
#include "dialog.h"
#include "event.h"
#include "change.h"
#include "milestone.h"
#include "condition.h"
#include "person.h"

#include <vector>
#include <string>
#include <set>
#include <unordered_map>


class Person;

class Script {
public:
	Script();
	~Script();

	// 输出当前剧本信息
	void Print();

	// 读取剧本
	void ReadScript(std::string path);

	// 应用变更
	void ApplyChange(std::shared_ptr<Change> change);

	// 保存/读取存档
	void SaveStory(std::string path);
	void LoadStory(std::string path);

	// 判断条件
	bool JudgeCondition(Condition &condition);

	// 匹配事件
	std::pair<std::vector<Dialog>, std::vector<std::shared_ptr<Change>>> MatchEvent(std::shared_ptr<Event> event);

	// 替换文本变量
	std::string ReplaceContent(std::string content);

private:
	std::vector<MilestoneNode> milestones;
	std::vector<MilestoneNode*> actives;

	std::unordered_map<std::string, std::string> values;

	// 复合对象读取
	std::shared_ptr<Event> BuildEvent(Json::Value root);
	std::vector<Dialog> BuildDialogs(Json::Value root);
	std::vector<std::shared_ptr<Change>> BuildChanges(Json::Value root);
	Condition BuildCondition(Json::Value root);

	// 初始化全局变量
	void InitValues();
};

