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
	if (day >= 1 && day <= 5)return { Time("09:00:00.000"), Time("17:00:00.000") };
	else return { Time(), Time() };
}

