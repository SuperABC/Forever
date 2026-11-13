#include "job_shop_saler.h"

#include "common/json.h"

using namespace std;

int ShopSalerJob::count = 0;

ShopSalerJob::ShopSalerJob() : id(count++) {
}

const char* ShopSalerJob::GetName() {
	name = "ShopSalerJob" + to_string(id);
	return name.data();
}

void ShopSalerJob::SetProperty() {
	scriptModName = "script_empty";
	milestoneNames = { "job_shop_saler" };
}

void ShopSalerJob::DailyPlan(const Time&, PostHandle*) {
	// 上下班由job_shop_saler.script的计时器驱动，这里不排任何C++节点(plans保持为空)。
}
