#pragma once

#include "calendar_base.h"

#include <string>
#include <vector>


// 子类注册函数
typedef void (*RegisterModCalendarsFunc)(CalendarFactory* factory);

// 标准双休日程
class StandardCalendar : public Calendar {
public:
	StandardCalendar();
	virtual ~StandardCalendar();

	static std::string GetId();
	virtual std::string GetType() const override;
	virtual std::string GetName() const override;

	virtual Time SigninTime(Time date) const override;
	virtual Time SignoutTime(Time date) const override;
};

// 全勤无休日程
class FullCalendar : public Calendar {
public:
	FullCalendar();
	virtual ~FullCalendar();

	static std::string GetId();
	virtual std::string GetType() const override;
	virtual std::string GetName() const override;

	virtual Time SigninTime(Time date) const override;
	virtual Time SignoutTime(Time date) const override;

private:
	Time on, off;
};
