#include "organization_restaurant.h"

#include "organization_common.h"

#include "common/json.h"

#include <algorithm>
#include <cstdlib>

using namespace std;

int RestaurantOrganization::count = 0;

RestaurantOrganization::RestaurantOrganization() : id(count++) {
	scriptModName = "script_empty";
	milestoneNames = { "organization_restaurant" };

	initialFunds = 20000000;         // 20万元
	shareholders = vector<float>(10, 0.1f); // 10个股东各占10%
	dividendPayoutRatio = 0.5f;             // 税后利润分一半，其余留在企业账户
	wage = 10000;                    // 每个员工每日100元
}

const char* RestaurantOrganization::GetName() {
	name = "RestaurantOrganization" + to_string(id);
	return name.data();
}

const unordered_map<string, pair<int, int>>& RestaurantOrganization::GetRequirements() {
	static const unordered_map<string, pair<int, int>> requirements = { { "component_restaurant", { 1, 1 } } };
	return requirements;
}

void RestaurantOrganization::DesignJobsForRoom(const std::string& componentType, const std::string&,
	const std::string& roomType, const std::string& roomName, int workspaceCapacity, bool reset) {
	if (reset) vacancies.clear(); // 必须由mod自己清空，见organization_mod.h
	// 按房间容纳量满编定员。
	if (componentType != "component_restaurant" || roomType != "room_restaurant") return;
	for (int i = 0; i < workspaceCapacity; i++) vacancies.push_back({ roomName, "job_restaurant_waiter" });
}

void RestaurantOrganization::MonthlySettle(const OrganizationMonthlyState& state) {
	issueShares = DistressIssue(state);
	// 服务员跟着利润扩缩（每个房间满编到2倍之间，裁到一半为止）。
	int waiters = PlannedCount(vacancies, "job_restaurant_waiter");
	int delta = HeadcountDelta(state, waiters, wage);
	if (delta != 0) AdjustPlan(vacancies, state, "job_restaurant_waiter", delta, 50, 200);
}

void RestaurantOrganization::CollectServices(Container* script, const vector<string>& stockTypes, PostHandle* post) {
	services.clear();
	if (!script || !post) return;

	for (const string& type : stockTypes) {
		auto [exist, value] = script->GetValue("stock." + type);
		if (!exist || ToInt(value) <= 0) continue;

		JsonValue request;
		request["post"] = "product info";
		request["type"] = type;
		post->Post(request);
		const JsonValue& info = post->GetResult();
		if (info["result"].AsString() != "success") continue;

		// 只开放food服务(汉堡)；小麦/牛肉等服务类型为空的货物不卖。
		string serviceType = info["service type"].AsString();
		if (serviceType != "food") continue;

		ServiceOffer offer;
		offer.name = type; // 服务名只给自己看：直接用货物类型，ApplyServiceUsage时据此扣库存
		offer.type = serviceType;
		offer.unitPrice = static_cast<int64_t>(info["retail price"].AsDouble()) * 2; // 餐厅单价固定为零售价2倍
		offer.quality = 0.5f; // 餐厅服务质量固定0.5，不看商品自身的质量
		offer.capacity = ToInt(value); // 能卖的份数就是库存
		services.push_back(offer);
	}
}

void RestaurantOrganization::ApplyServiceUsage(const string& name, int count, Container* script) {
	if (!script || count <= 0) return;

	auto [exist, value] = script->GetValue("stock." + name);
	if (!exist) return;
	script->SetValue("stock." + name, ValueType(max(0, ToInt(value) - count)));
}
