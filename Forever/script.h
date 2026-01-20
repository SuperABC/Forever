#pragma once

#include "milestone.h"
#include "change.h"
#include "story.h"
#include "json.h"
#include "error.h"


class Story;
class Person;

class Script {
public:
	Script();
	Script(const std::shared_ptr<Script> script);
	~Script();

	// 读取剧本
	void ReadScript(std::string path,
		std::unique_ptr<EventFactory>& eventFactory, std::unique_ptr<ChangeFactory>& changeFactory);

	// 匹配事件
	std::pair<std::vector<Dialog>, std::vector<std::shared_ptr<Change>>> MatchEvent(
		std::shared_ptr<Event> event, Story* story);
	std::pair<std::vector<Dialog>, std::vector<std::shared_ptr<Change>>> MatchEvent(
		std::shared_ptr<Event> event, Story* story, std::shared_ptr<Person> person);

private:
	std::vector<MilestoneNode> milestones;
	std::vector<MilestoneNode*> actives;

	// 复合对象读取
	std::vector<std::shared_ptr<Event>> BuildEvent(JsonValue root, std::unique_ptr<EventFactory>& factory);
	std::vector<Dialog> BuildDialogs(JsonValue root);
	std::vector<std::shared_ptr<Change>> BuildChanges(JsonValue root, std::unique_ptr<ChangeFactory>& factory);
	Condition BuildCondition(JsonValue root);

};