#include "job_factory_worker.h"

#include "common/json.h"

using namespace std;

int FactoryWorkerJob::count = 0;

FactoryWorkerJob::FactoryWorkerJob() : id(count++) {
}

const char* FactoryWorkerJob::GetName() {
	name = "FactoryWorkerJob" + to_string(id);
	return name.data();
}

void FactoryWorkerJob::SetProperty() {
	scriptModName = "script_empty";
	milestoneNames = { "job_factory_worker" };
}

void FactoryWorkerJob::DailyPlan(const Time&, PostHandle*) {
	// 上下班由job_factory_worker.script的计时器驱动，这里不排任何C++节点(plans保持为空)。
}
