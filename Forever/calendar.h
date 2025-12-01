#pragma once

#include "calendar_base.h"

#include <string>
#include <vector>


// ����ע�ắ��
typedef void (*RegisterModCalendarsFunc)(CalendarFactory* factory);

class TestCalendar : public Calendar {
public:
	TestCalendar() {}
	virtual ~TestCalendar() {}

	static std::string GetId() { return "test"; }
	virtual std::string GetType() const override { return "test"; }
	virtual std::string GetName() const override { return "�����ճ�"; }

	virtual std::pair<Time, Time> WorkingTime(Time date) const override {
		int day = date.DayOfWeek();
		if (day >= 1 && day <= 5)return { Time("09:00:00.000"), Time("09:00:00.000")};
		else return { Time(), Time() };
	}
};
