#pragma once

#include "event_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModEvent : public Event {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetType() const override { return "mod"; }
    virtual std::string GetName() const override { return "mod"; }

	virtual bool operator==(std::shared_ptr<Event> e) override {
		if (!e)return false;

		return GetType() == e->GetType();
	}
};

