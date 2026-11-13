#include "organization_common.h"

#include "common/json.h"

#include <algorithm>
#include <cstdlib>



using namespace std;

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

static int Capacity(const OrganizationMonthlyState& state, const std::string& room) {
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

static int RoomCount(const std::vector<Vacancy>& vacancies, const std::string& type, const std::string& room) {
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
