#pragma once

#include "society/ledger.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>


class Map;
class Populace;
class Society;
class Industry;
class Economy;

// 经济报表：城市生成结果，以及政府/个人/组织/外部四类账户在某个时期（开局或每个月）的收入、支出、
// 结余和余额分布。同时写到标准输出和报表文件。详见report.md。
class Reporter {
public:
	Reporter(Map& city, Populace& populace, Society& society, Industry& industry, const std::string& filePath);
	~Reporter();

	// 城市生成结果：市民构成、各类园区/建筑/房间数及政府拥有数、组织与岗位。
	void PrintCity();

	// 开局状态：把"开局到现在"的全部流水（注资）当作第0期。
	void PrintInitial();

	// 累计一天的工业数据（产量、进口、出口、城内流转）；主循环每天在Industry::Tick之后调用。
	void AccumulateIndustryDay();

	// 开始记一个新时期（记下期初快照）。
	void BeginPeriod();

	// 输出当前时期的报表（相对BeginPeriod()的变化），title是这一期的名字。
	void PrintPeriod(const std::string& title);

	// 运行概况：每个月的买饭供需，由主循环累加后传入。
	struct ServiceTotals {
		long long demand = 0, sold = 0, unmet = 0;
		std::map<float, long long> soldByQuality;
		int64_t revenue = 0;
		int newBankruptcies = 0, restarts = 0;
		int hired = 0;                 // 再就业配对成功人数
		int dismissedByBankruptcy = 0; // 企业倒闭裁掉的在岗人数
		int positionsReopened = 0;     // 重启企业重开/新建的岗位数
		int positionsOpened = 0, positionsClosed = 0, dismissed = 0; // 月结扩招/裁员
	};
	void PrintRunSummary(const std::string& title, const ServiceTotals& totals);

	// 一个月内的金融/人口流动累计，由主循环累加后传入PrintFinance。
	struct FinanceTotals {
		int immigrated = 0;           // 迁入人数
		int departed = 0;             // 离城人数
		int departedHouseholds = 0;   // 离城户数
		int departedImmigrants = 0;   // 离城的人里，本次模拟中迁入的人数
		int64_t turnover = 0;         // 股票成交额
		int64_t volume = 0;           // 股票成交股数
		int64_t sharesIssued = 0, sharesSubscribed = 0, issueProceeds = 0; // 增发新股数/老股东认购数/募资
		int64_t deposited = 0, withdrawn = 0;       // 存入/取出
		int64_t depositInterest = 0, bondInterest = 0, interestTax = 0;
		int64_t bondIssued = 0, bondRepaid = 0;     // 国债新发/到期偿还
		int64_t printed = 0;          // 印钞
	};
	// 输出人口迁入迁出、股市、存款、国债、印钞的月度概况，并记入逐月金融汇总表。
	// immigrantsActive是本次模拟迁入、目前仍在场的人数。
	void PrintFinance(const std::string& title, const std::string& label, const FinanceTotals& totals, int immigrantsActive);

	// 跨月汇总表用：记下本期末的几个关键余额。
	void RecordSummaryRow(const std::string& label);
	void PrintSummaryTable();
	void PrintFinanceTable();

private:
	struct Snapshot {
		std::array<int64_t, static_cast<size_t>(Flow::COUNT) * 16> flows{};
		int64_t government = 0, outside = 0, citizens = 0, organizations = 0;
	};
	struct SummaryRow {
		std::string label;
		int64_t government = 0, outside = 0, citizens = 0, organizations = 0;
		int poor = 0, rich = 0, bankrupt = 0;
	};

	Snapshot Take() const;
	void Out(const char* format, ...);
	void PrintFlows(const Snapshot& begin, const Snapshot& end, Party::Kind account);
	void PrintGovernment(const Snapshot& begin, const Snapshot& end);
	void PrintCitizens(const Snapshot& begin, const Snapshot& end);
	void PrintOrganizations(const Snapshot& begin, const Snapshot& end);
	void PrintOutside(const Snapshot& begin, const Snapshot& end);
	void PrintIndustry(const ServiceTotals& totals);
	// 设计日产能：全部工坊满负荷每天产多少（onlyOpen=true只算没有停业的）。
	std::map<std::string, double> DailyCapacity(bool onlyOpen) const;

	Map& city;
	Populace& populace;
	Society& society;
	Industry& industry;
	FILE* file = nullptr;
	Snapshot periodBegin;

	struct IndustryPeriod {
		int days = 0;
		std::map<std::string, double> produced;
		std::map<std::string, long long> imported, exported, internal;
	} industryPeriod;
	std::vector<SummaryRow> summary;

	struct FinanceRow {
		std::string label;
		int active = 0, immigrated = 0, departed = 0, immigrantsActive = 0;
		int64_t turnover = 0, deposits = 0, bonds = 0, printed = 0, government = 0, net = 0, marketCap = 0;
		double avgPrice = 0.0, avgYield = 0.0;
	};
	std::vector<FinanceRow> financeRows;
};
