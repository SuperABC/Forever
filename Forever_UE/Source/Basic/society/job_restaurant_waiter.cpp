#include "job_restaurant_waiter.h"

#include "common/json.h"

using namespace std;

int RestaurantWaiterJob::count = 0;

RestaurantWaiterJob::RestaurantWaiterJob() : id(count++) {
}

const char* RestaurantWaiterJob::GetName() {
	name = "RestaurantWaiterJob" + to_string(id);
	return name.data();
}

void RestaurantWaiterJob::SetProperty() {
	scriptModName = "script_empty";
	milestoneNames = { "job_restaurant_waiter" };
}

void RestaurantWaiterJob::DailyPlan(const Time&, PostHandle*) {
	// 上下班由job_restaurant_waiter.script的计时器驱动，这里不排任何C++节点(plans保持为空)。
}
