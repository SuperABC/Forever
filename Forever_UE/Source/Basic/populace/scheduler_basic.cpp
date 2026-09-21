#include "scheduler_basic.h"

using namespace std;

int SchedulerBasic::count = 0;

SchedulerBasic::SchedulerBasic() : id(count++) {
	scriptModName = "empty";
	milestoneNames = { "schedule_empty" };
}

const char* SchedulerBasic::GetName() {
	name = "SchedulerBasic" + to_string(id);
	return name.data();
}
