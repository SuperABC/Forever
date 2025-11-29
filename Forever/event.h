#pragma once

#include "event_base.h"
#include "condition.h"

#include <string>
#include <vector>


// 子类注册函数
typedef void (*RegisterModEventsFunc)(EventFactory* factory);

class GameStartEvent : public Event {
public:
	GameStartEvent() {}
	virtual ~GameStartEvent() {}

	static std::string GetId() { return "game_start"; }
    virtual std::string GetType() const override { return "game_start"; }
	virtual std::string GetName() const override { return "game_start"; }

	virtual bool operator==(std::shared_ptr<Event> e) {
		if (!e)return false;
		if (GetType() != e->GetType())return false;

		return true;
	}
};

class OptionDialogEvent : public Event {
public:
	OptionDialogEvent() {}
	OptionDialogEvent(std::string target, std::string option)
		: target(target), option(option) {
	}
	virtual ~OptionDialogEvent() {}

	static std::string GetId() { return "option_dialog"; }
    virtual std::string GetType() const override { return "option_dialog"; }
	virtual std::string GetName() const override { return "option_dialog"; }

	virtual bool operator==(std::shared_ptr<Event> e) {
		if(!e)return false;
		if (GetType() != e->GetType())return false;

		return target == std::dynamic_pointer_cast<OptionDialogEvent>(e)->target &&
			option == std::dynamic_pointer_cast<OptionDialogEvent>(e)->option;
	}

	void SetTarget(std::string target) { this->target = target; }
	std::string GetTarget() const { return target; }
	void SetOption(std::string option) { this->option = option; }
	std::string GetOption() const { return option; }

private:
	std::string target;
	std::string option;
};
