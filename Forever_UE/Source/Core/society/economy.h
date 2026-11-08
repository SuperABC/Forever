#pragma once

#include "class.h"

#include "society/settlement.h"
#include "society/investor.h"
#include "society/market.h"
#include "society/treasury.h"

#include "common/handle.h"
#include "common/utility.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>


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
	Treasury::Interest interest;   // 本月结的存款/国债利息及所得税
	int departedHouseholds = 0;    // 本月整户离城的户数
	int departedCitizens = 0;      // 本月离城的市民数
	int64_t sharesIssued = 0;      // 本月增发的新股数（含老股东没认购、进了库存股的部分）
	int64_t sharesSubscribed = 0;  // 其中老股东配股认购的股数
	int64_t issueProceeds = 0;     // 配股募到的资金
	int positionsOpened = 0;       // 本月扩招新开/重开的岗位数
	int positionsClosed = 0;       // 本月裁员关闭的岗位数
	int dismissed = 0;             // 其中被裁掉的在岗人数
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
	int dismissedByBankruptcy = 0; // 今天企业倒闭时裁掉的在岗人数
	int positionsReopened = 0;     // 今天重启的企业重开/新建的岗位数
	int hired = 0;                 // 今天再就业配对成功的人数（Society::MatchUnemployed）
	int64_t debtWrittenOff = 0; // 重启或离城时核销的负债（由城市外账户承担）

	Investor::Summary invest;     // 市民今天的存取款/挂单情况
	Market::ClearReport stock;    // 今天的股票成交
	int64_t bondRepaid = 0;       // 今天偿还的到期国债本金
	int64_t bondIssued = 0;       // 今天发行国债募集到的钱
	int64_t moneyPrinted = 0;     // 今天印的钞（国债补不上的缺口）
	std::vector<Citizen*> departed; // 今天（月结时）离城的市民，UE层据此移除对应Actor
};

// 每日经济驱动链：把Industry当天的货物流水、市民的服务需求、组织的服务供给、工资房租补助、月结税收
// 串成一条线。详见economy.md。
class Economy {
public:
	// 把倒闭企业占用的房间同步给Industry（停业的房间不参与物流、不开工）。每次Industry::Tick之前调用，
	// Daily内部在企业状态变化后也会再同步一次。
	static void SyncIndustry(Society& society, Industry& industry);

	// industry必须已经在今天Tick过（流水是当天的）。rooms是全城房间（算房租和户），citizens是全体市民
	// （算补助），post给组织mod查询商品价格等。currentYear用来算年龄。protectedCitizen是玩家当前控制的市民，
	// 他所在的户不会被判离城。
	static DailyReport Daily(Society& society, Industry& industry, const std::vector<Room*>& rooms,
		const std::vector<Citizen*>& citizens, const Time& time, PostHandle* post, int currentYear,
		const Citizen* protectedCitizen = nullptr);

	// ---- 玩家参与经济(玩家的钱就是被操控市民的Ledger账户) ----

	// 给worker发一笔工资：organization -> worker (Flow::WAGE)，再当场代扣所得税。和每日PayWages同一套扣税。
	static void PayWage(Society& society, Organization* organization, Citizen* worker, int64_t amountCents);

	enum class PurchaseResult { Success, NoMoney, NoSpace, Invalid };
	// 玩家买一件资产(assetType是Asset mod id，价格取它GetProductType()对应Product的零售价)：
	// 余额不够返回NoMoney、背包放不下返回NoSpace，两种情况都不扣钱不给东西；成功才
	// buyer -> seller (SERVICE_PURCHASE)，商店再从营业额里缴消费税(和每日RunServices同一个口径)。
	static PurchaseResult PlayerPurchase(Society& society, Industry& industry, Player& player, Organization* seller,
		Citizen* buyer, const std::string& assetType);

private:
	// 配股增发：账户低于初始资金一定比例（但还没为负）的企业增发新股，老股东按持股比例优先认购，认不完的进库存股。
	static void IssueShares(Society& society, MonthlyReport& report);

	// 月度结算：企业税、分红、股东所得税，存款/国债结息，离城检查。其余税收都在交易发生时即时缴纳。
	static MonthlyReport Monthly(Society& society, const std::vector<Room*>& rooms, int currentYear,
		const Citizen* protectedCitizen, DailyReport& report);

	// 政府账户为负时发国债补流动性，国债补不满的缺口直接印钞。
	static void FundGovernment(Society& society, const std::vector<Citizen*>& citizens, int currentYear,
		DailyReport& report);

	// 倒闭与重启：资金为负的企业倒闭；之后每天查询，mod的每个股东位都能找到存款超过"初始资金×占股比例"的
	// 成年市民时，从这些人账户按占股比例集齐初始资金，核销旧负债，旧股东的股份作废、换成新股东，重新营业。
	static void UpdateBankruptcies(Society& society, const std::vector<Citizen*>& citizens, int currentYear,
		DailyReport& report);
	static void RunServices(Society& society, Industry& industry, const std::vector<Room*>& rooms,
		PostHandle* post, int currentYear, DailyReport& report);
	static void PayWages(Society& society, DailyReport& report);
	static void PaySubsidies(Society& society, const std::vector<Citizen*>& citizens, int currentYear,
		DailyReport& report);
};
