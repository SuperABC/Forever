#pragma once

#include "milestone.h"
#include "change.h"
#include "story.h"
#include "json.h"
#include "error.h"


class Story;

class Script {
public:
	Script();
	~Script();

	// 读取剧本
	void ReadScript(std::string path,
		std::unique_ptr<EventFactory> &eventFactory, std::unique_ptr<ChangeFactory> &changeFactory);

	// 匹配事件
	std::pair<std::vector<Dialog>, std::vector<std::shared_ptr<Change>>> MatchEvent(
		std::shared_ptr<Event> event, Story *story);

private:
	std::vector<MilestoneNode> milestones;
	std::vector<MilestoneNode*> actives;

	// 复合对象读取
	std::vector<std::shared_ptr<Event>> BuildEvent(Json::Value root, std::unique_ptr<EventFactory> &factory);
	std::vector<Dialog> BuildDialogs(Json::Value root);
	std::vector<std::shared_ptr<Change>> BuildChanges(Json::Value root, std::unique_ptr<ChangeFactory> &factory);
	Condition BuildCondition(Json::Value root);

};