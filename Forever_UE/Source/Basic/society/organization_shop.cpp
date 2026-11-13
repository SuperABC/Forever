#include "organization_shop.h"

#include "organization_common.h"

#include "common/json.h"

#include <algorithm>
#include <cstdlib>

using namespace std;

int ShopOrganization::count = 0;

ShopOrganization::ShopOrganization() : id(count++) {
	scriptModName = "script_empty";
	milestoneNames = { "organization_shop" };

	initialFunds = 20000000;         // 20万元
	shareholders = vector<float>(10, 0.1f); // 10个股东各占10%
	dividendPayoutRatio = 0.5f;             // 税后利润分一半，其余留在企业账户
	wage = 10000;                    // 每个员工每日100元
}

const char* ShopOrganization::GetName() {
	name = "ShopOrganization" + to_string(id);
	return name.data();
}

const unordered_map<string, pair<int, int>>& ShopOrganization::GetRequirements() {
	static const unordered_map<string, pair<int, int>> requirements = { { "component_shop", { 1, 2 } } };
	return requirements;
}

void ShopOrganization::DesignJobsForRoom(const std::string& componentType, const std::string&,
	const std::string& roomType, const std::string& roomName, int workspaceCapacity, bool reset) {
	if (reset) vacancies.clear(); // 必须由mod自己清空，见organization_mod.h
	// 按房间容纳量满编定员。
	if (componentType != "component_shop") return;
	if (roomType == "room_shop") {
		for (int i = 0; i < workspaceCapacity; i++) vacancies.push_back({ roomName, "job_shop_saler" });
	}
	else if (roomType == "room_warehouse") {
		for (int i = 0; i < workspaceCapacity; i++) vacancies.push_back({ roomName, "job_warehouse_keeper" });
	}
}

void ShopOrganization::MonthlySettle(const OrganizationMonthlyState& state) {
	issueShares = DistressIssue(state);
	// 售货员跟着利润扩缩（每个房间满编到2倍之间，裁到一半为止）。
	int salers = PlannedCount(vacancies, "job_shop_saler");
	int delta = HeadcountDelta(state, salers, wage);
	if (delta != 0) AdjustPlan(vacancies, state, "job_shop_saler", delta, 50, 200);
	// 仓库管理员单独处理：平时保持满编，吃紧的月份缩到一半。
	if (Distress(state)) AdjustPlan(vacancies, state, "job_warehouse_keeper", -1000, 50, 100);
	else AdjustPlan(vacancies, state, "job_warehouse_keeper", 1000, 50, 100);
}

void ShopOrganization::CollectServices(Container* script, const vector<string>& stockTypes, PostHandle* post) {
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

		// 服务类型为空的货物(小麦/牛肉)暂不开放给市民直接购买。
		string serviceType = info["service type"].AsString();
		if (serviceType.empty()) continue;

		ServiceOffer offer;
		offer.name = type; // 服务名只给自己看：直接用货物类型，ApplyServiceUsage时据此扣库存
		offer.type = serviceType;
		offer.unitPrice = static_cast<int64_t>(info["retail price"].AsDouble()); // 商店单价固定为零售价，不参与市场调控
		offer.quality = static_cast<float>(info["service quality"].AsDouble());
		offer.capacity = ToInt(value); // 能卖的份数就是库存
		services.push_back(offer);
	}
}

void ShopOrganization::ApplyServiceUsage(const string& name, int count, Container* script) {
	if (!script || count <= 0) return;

	auto [exist, value] = script->GetValue("stock." + name);
	if (!exist) return;
	script->SetValue("stock." + name, ValueType(max(0, ToInt(value) - count)));
}
