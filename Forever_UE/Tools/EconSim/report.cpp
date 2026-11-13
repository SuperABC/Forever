#include "report.h"

#include "map/map.h"
#include "map/building.h"
#include "map/component.h"
#include "map/room.h"
#include "map/zone.h"
#include "populace/citizen.h"
#include "populace/populace.h"
#include "society/job.h"
#include "society/organization.h"
#include "society/society.h"
#include "society/market.h"
#include "society/treasury.h"
#include "industry/industry.h"

#include <algorithm>
#include <cstdarg>
#include <filesystem>
#include <map>
#include <unordered_set>
#include <vector>


using namespace std;

namespace {
	const char* AccountName(Party::Kind kind) {
		switch (kind) {
		case Party::OUTSIDE: return "外部";
		case Party::GOVERNMENT: return "政府";
		case Party::CITIZEN: return "个人";
		default: return "组织";
		}
	}

	// 分 -> "123456.78元"，带千分位更好读。
	string Yuan(int64_t cents) {
		bool negative = cents < 0;
		int64_t absolute = negative ? -cents : cents;
		string whole = to_string(absolute / 100);
		for (int i = static_cast<int>(whole.size()) - 3; i > 0; i -= 3) whole.insert(static_cast<size_t>(i), ",");
		char fraction[8];
		snprintf(fraction, sizeof(fraction), ".%02d", static_cast<int>(absolute % 100));
		return string(negative ? "-" : "") + whole + fraction + "元";
	}

	string Bar(int count, int maxCount, int width = 40) {
		if (maxCount <= 0) return "";
		int length = static_cast<int>(static_cast<long long>(count) * width / maxCount);
		if (count > 0 && length == 0) length = 1;
		string bar;
		for (int i = 0; i < length; i++) bar += "█";
		return bar;
	}

	struct Bucket {
		string label;
		int64_t lowCents;   // 含
		int64_t highCents;  // 不含
	};

	// 把余额按桶统计，打印类似柱状图的分布。
	vector<int> Histogram(const vector<int64_t>& balances, const vector<Bucket>& buckets) {
		vector<int> counts(buckets.size(), 0);
		for (int64_t balance : balances) {
			for (size_t i = 0; i < buckets.size(); i++) {
				if (balance >= buckets[i].lowCents && balance < buckets[i].highCents) { counts[i]++; break; }
			}
		}
		return counts;
	}

	const int64_t kInf = INT64_MAX;
	const int64_t kNegInf = INT64_MIN;
}

Reporter::Reporter(Map& city, Populace& populace, Society& society, Industry& industry, const string& filePath) :
	city(city), populace(populace), society(society), industry(industry) {
	if (!filePath.empty()) {
		// 报告目录(默认reports/)不存在就先建出来。
		std::error_code ignored;
		std::filesystem::create_directories(std::filesystem::path(filePath).parent_path(), ignored);
		if (fopen_s(&file, filePath.c_str(), "w") != 0) file = nullptr;
	}
}

Reporter::~Reporter() {
	if (file) fclose(file);
}

void Reporter::Out(const char* format, ...) {
	char buffer[4096];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	fputs(buffer, stdout);
	if (file) fputs(buffer, file);
}

Reporter::Snapshot Reporter::Take() const {
	Snapshot snapshot;
	const Ledger& ledger = society.GetLedger();
	snapshot.flows = ledger.GetFlowTotals();
	snapshot.government = ledger.Balance(Party::Government());
	snapshot.outside = ledger.Balance(Party::Outside());
	snapshot.citizens = ledger.CitizensTotal();
	snapshot.organizations = ledger.OrganizationsTotal();
	return snapshot;
}

void Reporter::BeginPeriod() {
	periodBegin = Take();
	industryPeriod = IndustryPeriod();
}

void Reporter::AccumulateIndustryDay() {
	industryPeriod.days++;
	for (const auto& [type, amount] : industry.GetDailyProduction()) industryPeriod.produced[type] += amount;
	for (const GoodsFlow& flow : industry.GetFlows()) {
		if (flow.IsImport()) industryPeriod.imported[flow.product] += flow.quantity;
		else if (flow.IsExport()) industryPeriod.exported[flow.product] += flow.quantity;
		else industryPeriod.internal[flow.product] += flow.quantity;
	}
}

map<string, double> Reporter::DailyCapacity(bool onlyOpen) const {
	map<string, double> capacity;
	for (const auto& [name, manufacture] : industry.GetManufactures()) {
		if (onlyOpen) {
			Party owner = society.OwnerOf(industry.GetManufactureRoom(manufacture));
			if (owner.kind == Party::ORGANIZATION && static_cast<const Organization*>(owner.ref)->IsBankrupt()) continue;
		}
		for (const auto& [type, batches] : manufacture->GetTargets()) {
			const Product* product = industry.FindProduct(type);
			if (product) capacity[type] += batches * product->GetBatchSize();
		}
	}
	return capacity;
}

void Reporter::PrintIndustry(const ServiceTotals& totals) {
	int days = max(1, industryPeriod.days);
	map<string, double> design = DailyCapacity(false), open = DailyCapacity(true);
	Out("  工业(%d天; 括号内为日均)\n", industryPeriod.days);
	Out("    %-16s %12s %12s %12s %12s %12s\n", "货物", "设计日产能", "当前开工产能", "产量", "进口", "出口");
	for (Product* product : industry.GetProducts()) {
		const string& type = product->GetType();
		double produced = industryPeriod.produced.count(type) ? industryPeriod.produced.at(type) : 0.0;
		long long imported = industryPeriod.imported.count(type) ? industryPeriod.imported.at(type) : 0;
		long long exported = industryPeriod.exported.count(type) ? industryPeriod.exported.at(type) : 0;
		Out("    %-16s %12.0f %12.0f %8.0f(%5.0f) %8lld(%5lld) %8lld(%5lld)\n", type.c_str(),
			design.count(type) ? design.at(type) : 0.0, open.count(type) ? open.at(type) : 0.0, produced, produced / days,
			imported, imported / days, exported, exported / days);
	}
	Out("    内销: 城内流转(厂间/厂到店)\n");
	for (Product* product : industry.GetProducts()) {
		const string& type = product->GetType();
		long long internal = industryPeriod.internal.count(type) ? industryPeriod.internal.at(type) : 0;
		Out("      %-16s %8lld(%5lld/天)\n", type.c_str(), internal, internal / days);
	}
	Out("    内销: 商店零售给市民(food) %lld 份(%lld/天)\n", totals.sold, totals.sold / days);
}

void Reporter::PrintCity() {
	int year = populace.GetCurrentYear();
	int minors = 0, adults = 0, elderly = 0, dead = 0, employed = 0;
	for (Citizen* citizen : populace.GetCitizens()) {
		if (!citizen->IsActive()) { dead++; continue; }
		int age = citizen->GetAge(year);
		if (age < 18) minors++;
		else if (age >= 60) elderly++;
		else adults++;
		if (citizen->GetJob()) employed++;
	}

	Out("\n==================== 城市生成结果 ====================\n");
	Out("市民总数 %zu 人: 未成年(<18) %d, 成年(18-59) %d, 老年(>=60) %d%s\n", populace.GetCitizens().size(), minors, adults,
		elderly, dead ? "(含已故)" : "");
	Out("在职 %d 人\n", employed);

	// 园区、建筑、房间：总数与政府拥有数。
	map<string, pair<int, int>> zones, buildings, rooms;
	for (const auto& [name, zone] : city.GetZones()) {
		zones[zone->GetType()].first++;
		if (zone->GetStated()) zones[zone->GetType()].second++;
	}
	for (const auto& [name, building] : city.GetBuildings()) {
		buildings[building->GetType()].first++;
		if (building->GetStated()) buildings[building->GetType()].second++;
		for (Room* room : building->GetRooms()) {
			rooms[room->GetType()].first++;
			if (room->GetStated()) rooms[room->GetType()].second++;
		}
	}
	auto printGroup = [&](const char* title, const map<string, pair<int, int>>& group) {
		int total = 0, stated = 0;
		for (const auto& [type, counts] : group) { total += counts.first; stated += counts.second; }
		Out("%s(共 %d, 政府拥有 %d, %.1f%%):\n", title, total, stated, total ? 100.0 * stated / total : 0.0);
		for (const auto& [type, counts] : group) {
			Out("  %-28s %6d   政府拥有 %6d (%.1f%%)\n", type.c_str(), counts.first, counts.second,
				counts.first ? 100.0 * counts.second / counts.first : 0.0);
		}
	};
	printGroup("园区", zones);
	printGroup("建筑", buildings);
	printGroup("房间", rooms);

	// 组织与岗位。
	map<string, int> organizations;
	map<string, pair<int, int>> jobs;
	for (Organization* organization : society.GetOrganizations()) {
		organizations[organization->GetType()]++;
		for (Job* job : organization->GetJobs()) {
			jobs[job->GetType()].first++;
			if (job->GetOccupant()) jobs[job->GetType()].second++;
		}
	}
	Out("组织(共 %zu):\n", society.GetOrganizations().size());
	for (const auto& [type, count] : organizations) Out("  %-28s %6d\n", type.c_str(), count);
	Out("岗位(总数/已招):\n");
	for (const auto& [type, counts] : jobs) Out("  %-28s %6d / %d\n", type.c_str(), counts.first, counts.second);

	// 户。
	int households = 0, residentRooms = 0, multi = 0;
	for (const auto& [name, building] : city.GetBuildings()) {
		for (Room* room : building->GetRooms()) {
			if (!room->IsResidence() || room->GetHouseholds().empty()) continue;
			residentRooms++;
			households += static_cast<int>(room->GetHouseholds().size());
			if (room->GetHouseholds().size() > 1) multi++;
		}
	}
	{
		map<string, double> capacity = DailyCapacity(false);
		map<string, int> manufactureCount;
		for (const auto& [name, manufacture] : industry.GetManufactures()) manufactureCount[name.substr(name.find('#') + 1)]++;
		Out("工坊(共 %zu 个):\n", industry.GetManufactures().size());
		for (const auto& [type, count] : manufactureCount) Out("  %-28s %6d\n", type.c_str(), count);
		Out("设计日产能(满负荷):\n");
		for (const auto& [type, amount] : capacity) Out("  %-28s %8.0f 份/天\n", type.c_str(), amount);
		Out("商店仓库: %zu 个\n", industry.GetStorages().size());
	}
	Out("住宅: 有人住的房间 %d 间, 共 %d 户(其中多户合住的房间 %d 间)\n", residentRooms, households, multi);
}

void Reporter::PrintFlows(const Snapshot& begin, const Snapshot& end, Party::Kind account) {
	// 某账户类别在该时期各类别的总收入/总支出。
	int64_t incomeTotal = 0, expenseTotal = 0;
	vector<pair<string, int64_t>> incomes, expenses;
	for (size_t k = 0; k < static_cast<size_t>(Flow::COUNT); k++) {
		int64_t income = 0, expense = 0;
		for (size_t other = 0; other < 4; other++) {
			size_t asReceiver = k * 16 + other * 4 + static_cast<size_t>(account);
			size_t asPayer = k * 16 + static_cast<size_t>(account) * 4 + other;
			income += end.flows[asReceiver] - begin.flows[asReceiver];
			expense += end.flows[asPayer] - begin.flows[asPayer];
		}
		if (income > 0) incomes.push_back({ FlowName(static_cast<Flow>(k)), income });
		if (expense > 0) expenses.push_back({ FlowName(static_cast<Flow>(k)), expense });
		incomeTotal += income;
		expenseTotal += expense;
	}
	Out("  收入合计 %s\n", Yuan(incomeTotal).c_str());
	for (const auto& [name, amount] : incomes) Out("    + %-12s %22s\n", name.c_str(), Yuan(amount).c_str());
	Out("  支出合计 %s\n", Yuan(expenseTotal).c_str());
	for (const auto& [name, amount] : expenses) Out("    - %-12s %22s\n", name.c_str(), Yuan(amount).c_str());
	Out("  本期结余(收入-支出) %s\n", Yuan(incomeTotal - expenseTotal).c_str());
}

void Reporter::PrintGovernment(const Snapshot& begin, const Snapshot& end) {
	Out("---- 政府 ----\n");
	Out("  期初余额 %s\n", Yuan(begin.government).c_str());
	PrintFlows(begin, end, Party::GOVERNMENT);
	Out("  期末余额 %s\n", Yuan(end.government).c_str());
}

void Reporter::PrintOutside(const Snapshot& begin, const Snapshot& end) {
	Out("---- 外部(城市外) ----\n");
	Out("  期初余额 %s\n", Yuan(begin.outside).c_str());
	PrintFlows(begin, end, Party::OUTSIDE);
	Out("  期末余额 %s (负数=累计净流入城市的资金)\n", Yuan(end.outside).c_str());
}

void Reporter::PrintCitizens(const Snapshot& begin, const Snapshot& end) {
	Out("---- 个人(全体市民合计) ----\n");
	Out("  期初存款合计 %s\n", Yuan(begin.citizens).c_str());
	PrintFlows(begin, end, Party::CITIZEN);
	Out("  期末存款合计 %s\n", Yuan(end.citizens).c_str());

	// 净资产分布：活期余额+存款+国债本金。
	const Ledger& ledger = society.GetLedger();
	const Treasury& treasury = society.GetTreasury();
	vector<int64_t> balances;
	for (Citizen* citizen : populace.GetCitizens()) {
		if (citizen->IsActive()) balances.push_back(ledger.Balance(Party::Of(citizen)) + treasury.DepositOf(citizen) + treasury.BondsOf(citizen));
	}
	if (balances.empty()) return;
	sort(balances.begin(), balances.end());
	int64_t sum = 0;
	for (int64_t balance : balances) sum += balance;
	auto percentile = [&](double p) { return balances[static_cast<size_t>(p * (balances.size() - 1))]; };
	Out("  人均 %s | P10 %s | 中位 %s | P90 %s | 最少 %s | 最多 %s\n", Yuan(sum / static_cast<int64_t>(balances.size())).c_str(),
		Yuan(percentile(0.1)).c_str(), Yuan(percentile(0.5)).c_str(), Yuan(percentile(0.9)).c_str(),
		Yuan(balances.front()).c_str(), Yuan(balances.back()).c_str());

	const int64_t y = 100;
	vector<Bucket> buckets = {
		{ "负债 <0元", kNegInf, 0 }, { "0 - 1千元", 0, 1000 * y }, { "1千 - 2.5千元", 1000 * y, 2500 * y },
		{ "2.5千 - 5千元", 2500 * y, 5000 * y }, { "5千 - 7.5千元", 5000 * y, 7500 * y },
		{ "7.5千 - 1万元", 7500 * y, 10000 * y }, { "1万 - 2万元", 10000 * y, 20000 * y },
		{ "2万 - 5万元", 20000 * y, 50000 * y }, { "5万元以上", 50000 * y, kInf } };
	vector<int> counts = Histogram(balances, buckets);
	int maxCount = *max_element(counts.begin(), counts.end());
	Out("  净资产分布(人数):\n");
	for (size_t i = 0; i < buckets.size(); i++) {
		Out("    %-14s %6d (%5.1f%%) %s\n", buckets[i].label.c_str(), counts[i], 100.0 * counts[i] / balances.size(),
			Bar(counts[i], maxCount).c_str());
	}

	// 穷/中/富：以开局人均5000元为参照，不足一半(2500元)算穷人，超过两倍(1万元)算富人。
	int poor = 0, rich = 0;
	for (int64_t balance : balances) {
		if (balance < 2500 * y) poor++;
		else if (balance >= 10000 * y) rich++;
	}
	int64_t topShare = 0;
	for (size_t i = balances.size() - balances.size() / 10; i < balances.size(); i++) topShare += balances[i];
	Out("  穷人(<2500元) %d 人 (%.1f%%) | 中等 %zu 人 | 富人(>=1万元) %d 人 (%.1f%%) | 最富10%%的人持有净资产的 %.1f%%\n", poor,
		100.0 * poor / balances.size(), balances.size() - poor - rich, rich, 100.0 * rich / balances.size(),
		sum > 0 ? 100.0 * topShare / sum : 0.0);

	// 穷人构成：按年龄段/有无工作拆开，并对比"只看活期余额"和"余额+存款"两种口径。
	struct Group { const char* name; int count = 0, poorCash = 0, poorWorth = 0; int64_t cash = 0, worth = 0; };
	Group groups[4] = { { "未成年" }, { "成年有工作" }, { "成年无工作" }, { "老年" } };
	int year = populace.GetCurrentYear();
	for (Citizen* citizen : populace.GetCitizens()) {
		if (!citizen->IsActive()) continue;
		int age = citizen->GetAge(year);
		int g = age < society.GetPolicy().adultAge ? 0 : age >= 60 ? 3 : citizen->GetJob() ? 1 : 2;
		int64_t cash = ledger.Balance(Party::Of(citizen));
		int64_t worth = cash + treasury.DepositOf(citizen) + treasury.BondsOf(citizen);
		groups[g].count++;
		groups[g].cash += cash;
		groups[g].worth += worth;
		if (cash < 2500 * y) groups[g].poorCash++;
		if (worth < 2500 * y) groups[g].poorWorth++;
	}
	Out("  穷人构成(穷=活期余额<2500元 / 括号内=净资产(余额+存款+国债)<2500元):\n");
	for (const Group& group : groups) {
		if (group.count == 0) continue;
		Out("    %-8s %6d 人, 穷人 %5d (%4.1f%%) / (%4.1f%%), 人均余额 %s, 人均净资产 %s\n", group.name, group.count, group.poorCash,
			100.0 * group.poorCash / group.count, 100.0 * group.poorWorth / group.count,
			Yuan(group.cash / group.count).c_str(), Yuan(group.worth / group.count).c_str());
	}
}

void Reporter::PrintOrganizations(const Snapshot& begin, const Snapshot& end) {
	Out("---- 组织(全体合计) ----\n");
	Out("  期初资金合计 %s\n", Yuan(begin.organizations).c_str());
	PrintFlows(begin, end, Party::ORGANIZATION);
	Out("  期末资金合计 %s\n", Yuan(end.organizations).c_str());

	const Ledger& ledger = society.GetLedger();
	vector<int64_t> balances;
	int bankrupt = 0;
	for (Organization* organization : society.GetOrganizations()) {
		balances.push_back(ledger.Balance(Party::Of(organization)));
		if (organization->IsBankrupt()) bankrupt++;
	}
	if (balances.empty()) return;
	sort(balances.begin(), balances.end());
	const int64_t y = 100;
	vector<Bucket> buckets = {
		{ "负债 <0元", kNegInf, 0 }, { "0 - 5万元", 0, 50000 * y }, { "5万 - 10万元", 50000 * y, 100000 * y },
		{ "10万 - 15万元", 100000 * y, 150000 * y }, { "15万 - 20万元", 150000 * y, 200000 * y },
		{ "20万 - 30万元", 200000 * y, 300000 * y }, { "30万元以上", 300000 * y, kInf } };
	vector<int> counts = Histogram(balances, buckets);
	int maxCount = *max_element(counts.begin(), counts.end());
	Out("  资金分布(家数, 初始资金每家20万元):\n");
	for (size_t i = 0; i < buckets.size(); i++) {
		Out("    %-14s %6d (%5.1f%%) %s\n", buckets[i].label.c_str(), counts[i], 100.0 * counts[i] / balances.size(),
			Bar(counts[i], maxCount).c_str());
	}
	Out("  倒闭中 %d / %zu 家 | 最少 %s | 中位 %s | 最多 %s\n", bankrupt, balances.size(), Yuan(balances.front()).c_str(),
		Yuan(balances[balances.size() / 2]).c_str(), Yuan(balances.back()).c_str());
}

void Reporter::PrintInitial() {
	Snapshot zero;
	Snapshot now = Take();
	Out("\n==================== 开局状态 ====================\n");
	PrintGovernment(zero, now);
	PrintCitizens(zero, now);
	PrintOrganizations(zero, now);
	PrintOutside(zero, now);
	Out("全系统资金总和 %s (必须为0)\n", Yuan(society.GetLedger().Total()).c_str());
	RecordSummaryRow("开局");
}

void Reporter::PrintPeriod(const string& title) {
	Snapshot now = Take();
	Out("\n==================== %s ====================\n", title.c_str());
	PrintGovernment(periodBegin, now);
	PrintCitizens(periodBegin, now);
	PrintOrganizations(periodBegin, now);
	PrintOutside(periodBegin, now);
	Out("全系统资金总和 %s (必须为0)%s\n", Yuan(society.GetLedger().Total()).c_str(),
		society.GetLedger().Total() == 0 ? "" : "  !!! 不为0");
}

void Reporter::PrintRunSummary(const string& title, const ServiceTotals& totals) {
	Out("---- %s运行概况 ----\n", title.c_str());
	Out("  吃饭: 需求 %lld 份, 卖出 %lld 份, 没买到 %lld 份(%.2f%%), 营业额 %s\n", totals.demand, totals.sold, totals.unmet,
		totals.demand ? 100.0 * totals.unmet / totals.demand : 0.0, Yuan(totals.revenue).c_str());
	for (const auto& [quality, sold] : totals.soldByQuality) Out("    质量%.2f 卖出 %lld 份\n", quality, sold);
	Out("  企业: 本期新倒闭 %d 家, 重启 %d 家\n", totals.newBankruptcies, totals.restarts);
	{
		// 岗位与就业：开放岗位/空缺/成年无工作的人数，以及本期的招聘、裁员流动。
		int positions = 0, filled = 0;
		for (const Organization* organization : society.GetOrganizations()) {
			positions += organization->OpenPositionCount();
			filled += organization->EmployeeCount();
		}
		int unemployed = 0;
		int year = populace.GetCurrentYear();
		for (const Citizen* citizen : populace.GetCitizens()) {
			if (!citizen->IsActive() || !citizen->IsInEconomy() || citizen->GetJob()) continue;
			int age = citizen->GetAge(year);
			if (age >= society.GetPolicy().adultAge && age < society.GetPolicy().retirementAge) unemployed++;
		}
		Out("  就业: 开放岗位 %d 个(在岗 %d, 空缺 %d), 成年无工作 %d 人 | 本期再就业 %d 人, 月结扩招 %d 岗/裁掉 %d 岗(其中在岗 %d 人), 倒闭裁员 %d 人, 重启重开 %d 岗\n",
			positions, filled, positions - filled, unemployed, totals.hired, totals.positionsOpened, totals.positionsClosed,
			totals.dismissed, totals.dismissedByBankruptcy, totals.positionsReopened);
	}
	PrintIndustry(totals);
}

void Reporter::PrintFinance(const string& title, const string& label, const FinanceTotals& totals, int immigrantsActive) {
	const Ledger& ledger = society.GetLedger();
	const Treasury& treasury = society.GetTreasury();
	const Market& market = society.GetMarket();

	// 人口。
	int active = 0, dead = 0, gone = 0, employed = 0, adultsActive = 0;
	for (const Citizen* citizen : populace.GetCitizens()) {
		if (citizen->IsDead()) dead++;
		else if (citizen->IsDeparted()) gone++;
		else {
			active++;
			if (citizen->GetJob()) employed++;
			if (citizen->GetAge(populace.GetCurrentYear()) >= society.GetPolicy().adultAge) adultsActive++;
		}
	}
	int initial = populace.GetInitialPopulation();
	Out("---- %s人口迁入迁出 ----\n", title.c_str());
	Out("  在场 %d 人(开局 %d, 目标1.5倍=%d); 累计死亡 %d, 累计离城 %d; 在岗 %d 人(在场成年人 %d)\n", active, initial,
		static_cast<int>(initial * 1.5), dead, gone, employed, adultsActive);
	Out("  本期迁入 %d 人, 离城 %d 人(%d 户), 其中迁入者 %d 人; 迁入者目前仍在场 %d 人\n", totals.immigrated, totals.departed,
		totals.departedHouseholds, totals.departedImmigrants, immigrantsActive);

	// 股市。
	int listed = 0, holders = 0, withHistory = 0;
	int64_t totalIssued = 0, cap = 0, minPrice = 0, maxPrice = 0, priceSum = 0, heldShares = 0;
	double yieldSum = 0.0;
	unordered_set<const Citizen*> holderSet;
	for (Organization* organization : society.GetOrganizations()) {
		int64_t price = market.Price(ledger, organization);
		if (price <= 0) continue;
		listed++;
		priceSum += price;
		cap += price * organization->GetTotalShares();
		totalIssued += organization->GetTotalShares();
		minPrice = listed == 1 ? price : min(minPrice, price);
		maxPrice = max(maxPrice, price);
		for (const Organization::Shareholder& s : organization->GetShareholders()) {
			heldShares += s.shares;
			holderSet.insert(s.citizen);
		}
		if (market.HasDividendHistory(organization)) {
			withHistory++;
			yieldSum += market.AnnualYield(ledger, organization);
		}
	}
	holders = static_cast<int>(holderSet.size());
	double avgPrice = listed ? static_cast<double>(priceSum) / listed : 0.0;
	double avgYield = withHistory ? yieldSum / withHistory : 0.0;
	Out("---- %s股市 ----\n", title.c_str());
	Out("  上市 %d 支(不含倒闭); 股价 平均 %s / 最低 %s / 最高 %s; 总市值 %s\n", listed, Yuan(static_cast<int64_t>(avgPrice)).c_str(),
		Yuan(minPrice).c_str(), Yuan(maxPrice).c_str(), Yuan(cap).c_str());
	Out("  持股市民 %d 人, 市民持股占总股数 %.1f%%; 平均年化股息率 %.2f%%\n", holders,
		totalIssued ? 100.0 * heldShares / static_cast<double>(totalIssued) : 0.0, avgYield * 100.0);
	Out("  本期成交 %lld 股, 成交额 %s\n", static_cast<long long>(totals.volume), Yuan(totals.turnover).c_str());
	Out("  本期增发 %lld 股(老股东认购 %lld 股), 募资 %s; 全市场总股本 %lld 股\n", static_cast<long long>(totals.sharesIssued),
		static_cast<long long>(totals.sharesSubscribed), Yuan(totals.issueProceeds).c_str(), static_cast<long long>(totalIssued));

	// 存款、国债、印钞。
	int depositors = 0;
	for (const Citizen* citizen : populace.GetCitizens()) {
		if (treasury.DepositOf(citizen) > 0) depositors++;
	}
	Out("---- %s存款/国债/印钞 ----\n", title.c_str());
	Out("  存款余额 %s(%d 人存款); 本期存入 %s, 取出 %s; 利息 存款 %s + 国债 %s, 利息所得税 %s\n", Yuan(treasury.TotalDeposits()).c_str(),
		depositors, Yuan(totals.deposited).c_str(), Yuan(totals.withdrawn).c_str(), Yuan(totals.depositInterest).c_str(),
		Yuan(totals.bondInterest).c_str(), Yuan(totals.interestTax).c_str());
	Out("  国债余额 %s(%d 张); 本期新发 %s, 到期偿还 %s; 本期印钞 %s\n", Yuan(treasury.TotalBonds()).c_str(),
		static_cast<int>(treasury.GetBonds().size()), Yuan(totals.bondIssued).c_str(), Yuan(totals.bondRepaid).c_str(),
		Yuan(totals.printed).c_str());
	Out("  政府账户 %s, 净头寸 %s(账户−存款−国债); 城市外账户 %s; 税率系数 %.3f, 补助系数 %.3f\n",
		Yuan(ledger.Balance(Party::Government())).c_str(), Yuan(treasury.NetPosition(ledger)).c_str(),
		Yuan(ledger.Balance(Party::Outside())).c_str(), society.GetPolicy().taxFactor, society.GetPolicy().allowanceFactor);

	FinanceRow row;
	row.label = label;
	row.active = active;
	row.immigrated = totals.immigrated;
	row.departed = totals.departed;
	row.immigrantsActive = immigrantsActive;
	row.turnover = totals.turnover;
	row.deposits = treasury.TotalDeposits();
	row.bonds = treasury.TotalBonds();
	row.printed = totals.printed;
	row.government = ledger.Balance(Party::Government());
	row.net = treasury.NetPosition(ledger);
	row.marketCap = cap;
	row.avgPrice = avgPrice;
	row.avgYield = avgYield;
	financeRows.push_back(row);
}

void Reporter::PrintFinanceTable() {
	Out("\n==================== 逐月金融/人口汇总 ====================\n");
	Out("%-8s %7s %6s %6s %8s %14s %16s %16s %14s %16s %9s %8s\n", "时点", "在场", "迁入", "离城", "迁入者在", "股票成交额", "存款", "国债", "印钞", "净头寸", "平均股价", "股息率");
	for (const FinanceRow& row : financeRows) {
		Out("%-8s %7d %6d %6d %8d %14s %16s %16s %14s %16s %9s %7.2f%%\n", row.label.c_str(), row.active, row.immigrated, row.departed,
			row.immigrantsActive, Yuan(row.turnover).c_str(), Yuan(row.deposits).c_str(), Yuan(row.bonds).c_str(),
			Yuan(row.printed).c_str(), Yuan(row.net).c_str(), Yuan(static_cast<int64_t>(row.avgPrice)).c_str(), row.avgYield * 100.0);
	}
}

void Reporter::RecordSummaryRow(const string& label) {
	SummaryRow row;
	row.label = label;
	const Ledger& ledger = society.GetLedger();
	const Treasury& treasury = society.GetTreasury();
	// 存款和国债本金在账本上记在政府名下，这里归还给市民：个人合计含存款+国债，政府取扣除负债后的净额。
	row.government = treasury.NetPosition(ledger);
	row.outside = ledger.Balance(Party::Outside());
	row.citizens = ledger.CitizensTotal() + treasury.TotalDeposits() + treasury.TotalBonds();
	row.organizations = ledger.OrganizationsTotal();
	for (Citizen* citizen : populace.GetCitizens()) {
		if (!citizen->IsActive()) continue;
		int64_t balance = ledger.Balance(Party::Of(citizen)) + society.GetTreasury().DepositOf(citizen) + society.GetTreasury().BondsOf(citizen);
		if (balance < 2500 * 100) row.poor++;
		else if (balance >= 10000 * 100) row.rich++;
	}
	for (Organization* organization : society.GetOrganizations()) {
		if (organization->IsBankrupt()) row.bankrupt++;
	}
	summary.push_back(row);
}

void Reporter::PrintSummaryTable() {
	Out("\n==================== 逐月汇总(期末余额) ====================\n");
	Out("%-8s %20s %20s %20s %20s %7s %7s %6s\n", "时点", "政府", "个人合计", "组织合计", "外部", "穷人", "富人", "倒闭");
	for (const SummaryRow& row : summary) {
		Out("%-8s %20s %20s %20s %20s %7d %7d %6d\n", row.label.c_str(), Yuan(row.government).c_str(),
			Yuan(row.citizens).c_str(), Yuan(row.organizations).c_str(), Yuan(row.outside).c_str(), row.poor, row.rich, row.bankrupt);
	}
}
