#include "organization_manufacture.h"

#include "organization_common.h"

#include "common/json.h"

#include <algorithm>
#include <cstdlib>

using namespace std;

int ManufactureOrganization::count = 0;

ManufactureOrganization::ManufactureOrganization() : id(count++) {
	initialFunds = 20000000;                // 20万元
	shareholders = vector<float>(10, 0.1f); // 10个股东各占10%
	dividendPayoutRatio = 0.5f;             // 税后利润分一半，其余留在企业账户
	wage = 10000;                           // 每个员工每日100元
}

const char* ManufactureOrganization::GetName() {
	name = "ManufactureOrganization" + to_string(id);
	return name.data();
}

const unordered_map<string, pair<int, int>>& ManufactureOrganization::GetRequirements() {
	static const unordered_map<string, pair<int, int>> requirements = { { "component_manufacture", { 1, 1 } } };
	return requirements;
}

void ManufactureOrganization::DesignJobsForRoom(const string& componentType, const string&,
	const string& roomType, const string& roomName, int workspaceCapacity, bool reset) {
	if (reset) vacancies.clear(); // 必须由mod自己清空，见organization_mod.h
	// 工坊房间按容纳量满编定员。
	if (componentType != "component_manufacture") return;
	if (roomType == "room_farm" || roomType == "room_ranch" || roomType == "room_food_factory") {
		for (int i = 0; i < workspaceCapacity; i++) vacancies.push_back({ roomName, "job_factory_worker" });
	}
}

void ManufactureOrganization::MonthlySettle(const OrganizationMonthlyState& state) {
	issueShares = DistressIssue(state);
	// 工人跟着利润扩缩；超编（房间容量的2倍以内）会提高工坊产能，所以岗位已满且利润高时才扩招。
	int workers = PlannedCount(vacancies, "job_factory_worker");
	int delta = HeadcountDelta(state, workers, wage);
	if (delta != 0) AdjustPlan(vacancies, state, "job_factory_worker", delta, 50, 200);
}
