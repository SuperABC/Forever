#include "calendar.h"


using namespace std;

StandardCalendar::StandardCalendar() {

}

StandardCalendar::~StandardCalendar() {

}

string StandardCalendar::GetId() {
	return "standard";
}

string StandardCalendar::GetType() const {
	return "standard";
}

string StandardCalendar::GetName() const {
	return "标准双休日程";
}

pair<Time, Time> StandardCalendar::WorkingTime(Time date) const {
	int day = date.DayOfWeek();
	Time on("09:00:00.000");
	Time off("17:00:00.000");
	on.SetDate(date.GetYear(), date.GetMonth(), date.GetDay());
	off.SetDate(date.GetYear(), date.GetMonth(), date.GetDay());
	if (day >= 1 && day <= 5)return { on, off };
	else return { Time(), Time() };
}

