#pragma once

#include "class.h"

#include "society/settlement.h"

#include "common/handle.h"
#include "common/utility.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>


class Society;
class Industry;
class Room;
class Citizen;

// 一种服务类型当日的供需。
struct ServiceStat {
	int demand = 0; // 市民需要的份数
	int sold = 0;   // 实际卖出的份数
	int unmet = 0;  // 没买到（没货或买不起）的份数
	int64_t revenue = 0;
	int64_t consumptionTax = 0; // 商店当场缴的消费税
};

// 月度分红结算汇总（分）。
struct MonthlyReport {
	bool settled = false;
	int64_t corporateTax = 0;    // 企业对可分配利润缴的企业税
	int64_t dividends = 0;       // 税后分给股东的红利
	int64_t dividendIncomeTax = 0; // 股东当场缴的所得税
};

struct DailyReport {
	GoodsSettlementReport goods;
	RentReport rent;
	std::map<std::string, ServiceStat> services;
	int64_t wages = 0;
	int64_t wageIncomeTax = 0; // 发工资时当场代扣的所得税
	int wagesPaid = 0;
	int64_t pensions = 0, unemployment = 0, minorAllowance = 0;
	int pensionCount = 0, unemploymentCount = 0, minorCount = 0;
	MonthlyReport month; // 当天跨月时才有内容

	int newBankruptcies = 0;  // 今天新倒闭的企业数
	int restarts = 0;         // 今天股东集资重新营业的企业数
	int bankruptTotal = 0;    // 今天结束时仍在倒闭状态的企业数
	int64_t restartFunds = 0; // 今天股东集资的总额
	int64_t debtWrittenOff = 0; // 重启时核销的负债（由城市外账户承担）
};

// 每日经济驱动链：把Industry当天的货物流水、市民的服务需求、组织的服务供给、工资房租补助、月结税收
// 串成一条线。详见economy.md。
class Economy {
public:
	// 把倒闭企业占用的房间同步给Industry（停业的房间不参与物流、不开工）。每次Industry::Tick之前调用，
	// Daily内部在企业状态变化后也会再同步一次。
	static void SyncIndustry(Society& society, Industry& industry);

	// industry必须已经在今天Tick过（流水是当天的）。rooms是全城房间（算房租和户），citizens是全体市民
	// （算补助），post给组织mod查询商品价格等。currentYear用来算年龄。
	static DailyReport Daily(Society& society, Industry& industry, const std::vector<Room*>& rooms,
		const std::vector<Citizen*>& citizens, const Time& time, PostHandle* post, int currentYear);

private:
	// 月度分红结算：企业税、分红、股东所得税。其余税收都在交易发生时即时缴纳。
	static MonthlyReport Monthly(Society& society);

	// 倒闭与重启：资金为负的企业倒闭；之后每天查询，每个股东位都能找到存款超过"初始资金×占股比例"的
	// 成年市民时，从这些人账户按占股比例集齐初始资金，核销旧负债，重新营业。
	static void UpdateBankruptcies(Society& society, const std::vector<Citizen*>& citizens, int currentYear,
		DailyReport& report);
	static void RunServices(Society& society, Industry& industry, const std::vector<Room*>& rooms,
		PostHandle* post, int currentYear, DailyReport& report);
	static void PayWages(Society& society, DailyReport& report);
	static void PaySubsidies(Society& society, const std::vector<Citizen*>& citizens, int currentYear,
		DailyReport& report);
};
