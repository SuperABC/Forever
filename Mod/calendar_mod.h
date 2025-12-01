#pragma once

#include "calendar_base.h"

#include <memory>
#include <string>


// ģ��������
class ModCalendar : public Calendar {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetType() const override { return "mod"; }
    virtual std::string GetName() const override { return "ģ���ճ�"; }

    virtual std::pair<Time, Time> WorkingTime(Time date) const override {
        return { Time("09:00:00.000"), Time("09:00:00.000") };
    }
};

