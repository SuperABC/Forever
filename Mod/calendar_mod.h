#pragma once

#include "calendar_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModCalendar : public Calendar {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetType() const override { return "mod"; }
    virtual std::string GetName() const override { return "模组日程"; }

    virtual std::pair<Time, Time> WorkingTime(Time date) const {
        return { Time("09:00:00.000"), Time("09:00:00.000") };
    }
};

