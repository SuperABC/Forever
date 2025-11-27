#pragma once

#include "utility.h"
#include "condition.h"

#include <string>
#include <vector>


enum EVENT_TYPE {
	EVENT_NOTHING_HAPPEN, // 无

	EVENT_GAME_START, // 游戏开始
	EVENT_OPTION_DIALOG, // 使用选项对话
};

class Event {
public:
	Event(EVENT_TYPE type) : type(type) {}
	virtual ~Event() {}

	// 获取类型
	EVENT_TYPE GetType();

	// 判断匹配
	virtual bool operator==(std::shared_ptr<Event> e) {
		if(!e)return false;
		return type == e->type;
	}

    // 设置/获取控制条件
    void SetCondition(Condition condition);
    Condition& GetCondition();

protected:
	EVENT_TYPE type = EVENT_NOTHING_HAPPEN;

    Condition condition;
};

class GameStartEvent : public Event {
public:
	GameStartEvent() :
		Event(EVENT_GAME_START) {}
	virtual ~GameStartEvent() {}

	virtual bool operator==(std::shared_ptr<Event> e) {
		if (!e)return false;
		if (!Event::operator==(e))return false;

		return true;
	}
};

class OptionDialogEvent : public Event {
public:
	OptionDialogEvent(std::string target, std::string option) :
		Event(EVENT_OPTION_DIALOG), target(target), option(option) {}
	virtual ~OptionDialogEvent() {}

	virtual bool operator==(std::shared_ptr<Event> e) {
		if(!e)return false;
		if (!Event::operator==(e))return false;

		return target == std::dynamic_pointer_cast<OptionDialogEvent>(e)->target &&
			option == std::dynamic_pointer_cast<OptionDialogEvent>(e)->option;
	}

private:
	std::string target;
	std::string option;
};
