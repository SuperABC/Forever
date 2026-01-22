#pragma once

#include "utility.h"
#include "json.h"
#include "script.h"
#include "dialog.h"
#include "event.h"
#include "change.h"
#include "milestone.h"
#include "condition.h"
#include "populace.h"

#include <vector>
#include <string>
#include <set>
#include <unordered_map>


class Script;
class Populace;

class Story {
public:
	Story();
	~Story();

	// 读取Mods
	void InitEvents();
	void InitChanges();
	std::unique_ptr<EventFactory>& GetEventFactory();
	std::unique_ptr<ChangeFactory>& GetChangeFactory();

	// 读取配置文件
	void ReadConfigs(std::string path) const;

	// 剧情初始化
	void Init(std::unique_ptr<Populace> &populace);

	// 释放空间
	void Destroy();

	// 时钟前进
	void Tick(int day, int hour, int min, int sec);

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
	bool JudgeCondition(Condition& condition);
	bool JudgeCondition(Condition& condition, std::shared_ptr<Person> person);
	bool JudgeCondition(Condition& condition, std::vector<std::function<std::pair<bool, ValueType>(const std::string&)>> getValues);

	// 匹配事件
	std::pair<std::vector<Dialog>, std::vector<std::shared_ptr<Change>>> MatchEvent(std::shared_ptr<Event> event);

	// 变量管理
	void SetValue(const std::string& name, ValueType value);
	std::pair<bool, ValueType> GetValue(const std::string& name);

private:
	// Mod管理
	std::vector<HMODULE> modHandles;
	std::unique_ptr<EventFactory> eventFactory;
	std::unique_ptr<ChangeFactory> changeFactory;

	std::unique_ptr<Script> script;
	std::unordered_map<std::string, ValueType> variables;

	// 初始化全局变量
	void InitVariables(std::unique_ptr<Populace>& populace);
};

