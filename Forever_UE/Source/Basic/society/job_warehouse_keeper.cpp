#include "job_warehouse_keeper.h"

#include "common/json.h"

using namespace std;

namespace {
	// 通过PostHandle向Core查occupant家/工位的房间地址(post类型"citizen home/workplace address")，
	// 见job_warehouse_keeper.md。查询失败返回空字符串。
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
