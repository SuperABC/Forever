#include "job_basic.h"

#include "common/json.h"

using namespace std;

namespace {
	// 向Core查occupant家/工位的具体房间地址(Room::GetAddress()格式)——
	// NPCNavigateChange::destination不能再写"home"/"workplace"这种描述性文本，必须是
	// 能被Map::LocateRoom(address)直接解析回Room*的具体地址字符串，这个job所在的
	// Dependence层看不到Citizen/Room这些Core类型，只能通过PostHandle按姓名查询拿到
	// 字符串结果，见Core/common/implement.cpp的"citizen home/workplace address"两个
	// post类型。查询失败(citizen没有家/没有job等理论上不会发生的情况)返回空字符串。
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
	scriptModName = "empty";
	milestoneNames = { "job_shop_saler" };
}

const char* ShopSalerJob::GetName() {
	name = "ShopSalerJob" + to_string(id);
	return name.data();
}

void ShopSalerJob::DailyPlan(const Time& currentTime, PostHandle*) {
	plans.clear();
	plans["leave_home"] = Time(currentTime.GetYear(), currentTime.GetMonth(), currentTime.GetDay(), 9, 0);
	plans["leave_work"] = Time(currentTime.GetYear(), currentTime.GetMonth(), currentTime.GetDay(), 12, 0);
}

void ShopSalerJob::ExecNode(const string& node, PostHandle* post) {
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
