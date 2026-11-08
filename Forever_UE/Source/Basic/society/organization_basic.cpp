#include "organization_basic.h"

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

namespace {
	constexpr int64_t kInitialShares = 10000;      // 同Organization::kTotalShares
	constexpr int64_t kMaxShareMultiple = 4;       // 总股本最多涨到初始发行量的4倍
	constexpr int64_t kDistressBalancePercent = 30; // 账户低于初始资金的30%算吃紧
	constexpr int64_t kExpandProfitPercent = 5;    // 当月利润 > 全组织一个月人力成本的5%才扩招
	constexpr int64_t kHiringBudgetPercent = 50;   // 扩招最多花当月利润的50%养新增人手
	constexpr int kStepPercent = 10;               // 每次裁员调整计划人数的比例
		constexpr int kExpandStepPercent = 20;         // 每次扩招调整计划人数的比例
	constexpr int kDaysPerMonth = 30;

	// 缺钱增发：账户为正但低于初始资金的30%，增发现有总股数的20%，总股本不超过初始的4倍。
	int64_t DistressIssue(const OrganizationMonthlyState& state) {
		if (state.balance < 0 || state.balance >= state.initialFunds * kDistressBalancePercent / 100) return 0;
		if (state.totalShares >= kInitialShares * kMaxShareMultiple) return 0;
		return state.totalShares / 5;
	}

	int Capacity(const OrganizationMonthlyState& state, const std::string& room) {
		for (int i = 0; i < state.roomCount; i++) {
			if (room == state.rooms[i].room) return state.rooms[i].capacity;
		}
		return 0;
	}

	int PlannedCount(const std::vector<Vacancy>& vacancies, const std::string& type) {
		int n = 0;
		for (const Vacancy& v : vacancies) if (v.type == type) n++;
		return n;
	}

	int RoomCount(const std::vector<Vacancy>& vacancies, const std::string& type, const std::string& room) {
		int n = 0;
		for (const Vacancy& v : vacancies) if (v.type == type && v.room == room) n++;
		return n;
	}

	// 把type类型的岗位计划整体调整steps条：steps>0 扩招，轮流往计划人数最少的房间加，每个房间不超过
	// 容量×maxPercent%；steps<0 裁员，轮流从计划人数最多的房间减，每个房间不低于容量×minPercent%（向上取整）。
	// 只在计划里已有该类型岗位的房间里调整。
	void AdjustPlan(std::vector<Vacancy>& vacancies, const OrganizationMonthlyState& state, const std::string& type,
		int steps, int minPercent, int maxPercent) {
		std::vector<std::string> rooms;
		for (const Vacancy& v : vacancies) {
			if (v.type != type) continue;
			if (std::find(rooms.begin(), rooms.end(), v.room) == rooms.end()) rooms.push_back(v.room);
		}
		for (int step = 0; step < std::abs(steps); step++) {
			const std::string* pick = nullptr;
			int pickCount = 0;
			for (const std::string& room : rooms) {
				int count = RoomCount(vacancies, type, room);
				int capacity = Capacity(state, room);
				if (steps > 0) {
					if (count >= capacity * maxPercent / 100) continue;
					if (!pick || count < pickCount) { pick = &room; pickCount = count; }
				}
				else {
					if (count <= (capacity * minPercent + 99) / 100) continue;
					if (!pick || count > pickCount) { pick = &room; pickCount = count; }
				}
			}
			if (!pick) return;
			if (steps > 0) {
				vacancies.push_back({ *pick, type });
			}
			else {
				for (size_t i = vacancies.size(); i-- > 0;) {
					if (vacancies[i].type == type && vacancies[i].room == *pick) {
						vacancies.erase(vacancies.begin() + i);
						break;
					}
				}
			}
		}
	}

	bool Distress(const OrganizationMonthlyState& state) {
		// 只看账户存量：单月亏损不算吃紧，余额跌破初始资金30%才裁员。
		return state.balance < state.initialFunds * kDistressBalancePercent / 100;
	}

	// 主力岗位的扩缩人数：吃紧(余额低于初始资金的30%)就裁计划的10%；岗位已满时，当月利润高过一个月人力成本的5%、
	// 或账户高出初始资金的部分超过一个月人力成本，就扩计划的20%。新增人手一个月的工资不超过预算：
	// 当月利润的50%，或账户高出初始资金部分的1/12（取大者）。
	int HeadcountDelta(const OrganizationMonthlyState& state, int planned, int64_t wage) {
		if (Distress(state)) return -std::max(1, planned * kStepPercent / 100);
		int step = std::max(1, planned * kExpandStepPercent / 100);
		int64_t monthlyCost = wage * kDaysPerMonth * std::max(1, state.positions);
		int64_t surplus = std::max<int64_t>(0, state.balance - state.initialFunds);
		bool profitable = state.profit > monthlyCost * kExpandProfitPercent / 100;
		bool rich = surplus > monthlyCost;
		if ((!profitable && !rich) || state.employees < state.positions) return 0;
		int64_t budget = std::max(std::max<int64_t>(0, state.profit) * kHiringBudgetPercent / 100, surplus / 12);
		int64_t affordable = budget / std::max<int64_t>(1, wage * kDaysPerMonth);
		return static_cast<int>(std::min<int64_t>(step, affordable));
	}
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

int FactoryOrganization::count = 0;

FactoryOrganization::FactoryOrganization() : id(count++) {
	initialFunds = 20000000;                // 20万元
	shareholders = vector<float>(10, 0.1f); // 10个股东各占10%
	dividendPayoutRatio = 0.5f;             // 税后利润分一半，其余留在企业账户
	wage = 10000;                           // 每个员工每日100元
}

const char* FactoryOrganization::GetName() {
	name = "FactoryOrganization" + to_string(id);
	return name.data();
}

const unordered_map<string, pair<int, int>>& FactoryOrganization::GetRequirements() {
	static const unordered_map<string, pair<int, int>> requirements = { { "component_factory", { 1, 1 } } };
	return requirements;
}

void FactoryOrganization::DesignJobsForRoom(const string& componentType, const string&,
	const string& roomType, const string& roomName, int workspaceCapacity, bool reset) {
	if (reset) vacancies.clear(); // 必须由mod自己清空，见organization_mod.h
	// 工坊房间按容纳量满编定员。
	if (componentType != "component_factory") return;
	if (roomType == "room_farm" || roomType == "room_ranch" || roomType == "room_food_factory") {
		for (int i = 0; i < workspaceCapacity; i++) vacancies.push_back({ roomName, "job_factory_worker" });
	}
}

void FactoryOrganization::MonthlySettle(const OrganizationMonthlyState& state) {
	issueShares = DistressIssue(state);
	// 工人跟着利润扩缩；超编（房间容量的2倍以内）会提高工坊产能，所以岗位已满且利润高时才扩招。
	int workers = PlannedCount(vacancies, "job_factory_worker");
	int delta = HeadcountDelta(state, workers, wage);
	if (delta != 0) AdjustPlan(vacancies, state, "job_factory_worker", delta, 50, 200);
}
