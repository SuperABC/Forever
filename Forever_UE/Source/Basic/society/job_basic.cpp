#include "job_basic.h"

#include "common/json.h"

using namespace std;

namespace {
	// 通过PostHandle向Core查occupant家/工位的房间地址(post类型"citizen home/workplace address")，
	// 见job_basic.md。查询失败返回空字符串。
	string QueryAddress(PostHandle* post, const string& occupantName, const char* postType) {
		if (!post) return string();
		JsonValue request(DATA_OBJECT);
		request["post"] = postType;
		request["name"] = occupantName;
		post->Post(request);
		const JsonValue& result = post->GetResult();
		if (result["result"].AsString() != "success") return string();
		return result["address"].AsString();
	}
}

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

int WarehouseKeeperJob::count = 0;

WarehouseKeeperJob::WarehouseKeeperJob() : id(count++) {
}

const char* WarehouseKeeperJob::GetName() {
	name = "WarehouseKeeperJob" + to_string(id);
	return name.data();
}

void WarehouseKeeperJob::DailyPlan(const Time& currentTime, PostHandle*) {
	plans.clear();
	plans["leave_home"] = Time(currentTime.GetYear(), currentTime.GetMonth(), currentTime.GetDay(), 9, 0);
	plans["leave_work"] = Time(currentTime.GetYear(), currentTime.GetMonth(), currentTime.GetDay(), 18, 0);
}

void WarehouseKeeperJob::ExecNode(const string& node, PostHandle* post) {
	for (Change* change : changes) delete change;
	changes.clear();

	if (node == "leave_home") {
		string workplaceAddress = QueryAddress(post, occupantName, "citizen workplace address");
		if (workplaceAddress.empty()) return; // 查询失败，这次调度作废，不产出任何Change
		changes.push_back(new NPCNavigateChange(occupantName, workplaceAddress));
	}
	else if (node == "leave_work") {
		string homeAddress = QueryAddress(post, occupantName, "citizen home address");
		if (homeAddress.empty()) return;
		changes.push_back(new NPCNavigateChange(occupantName, homeAddress));
	}
}

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
