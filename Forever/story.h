#pragma once

#include "utility.h"
#include "json.h"
#include "script.h"
#include "dialog.h"
#include "event.h"
#include "change.h"
#include "milestone.h"
#include "condition.h"

#include <vector>
#include <string>
#include <set>
#include <unordered_map>


class Script;

class Story {
public:
	Story();
	~Story();

	// 读取Mods
	void InitEvents();
	void InitChanges();
	std::unique_ptr<EventFactory>& GetEventFactory();
	std::unique_ptr<ChangeFactory>& GetChangeFactory();

	// 剧情初始化
	void Init();

	// 输出当前剧本信息
	void Print();

	// 读取剧本
	void ReadStory(std::string path);

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
	std::string ReplaceContent(const std::string& content);

	// 变量管理
	void SetValue(const std::string& name, ValueType value);
	ValueType GetValue(const std::string& name);

private:
	// Mod管理
	std::vector<HMODULE> modHandles;
	std::unique_ptr<EventFactory> eventFactory;
	std::unique_ptr<ChangeFactory> changeFactory;

	std::unordered_map<std::string, ValueType> variables;

	std::unique_ptr<Script> script;

	// 初始化全局变量
	void InitVariables();
};

