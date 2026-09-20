#include "scheduler_basic.h"

using namespace std;

int SchedulerBasic::count = 0;

SchedulerBasic::SchedulerBasic() : id(count++) {
}

const char* SchedulerBasic::GetName() {
	name = "SchedulerBasic" + to_string(id);
	return name.data();
}
