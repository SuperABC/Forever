#pragma once

#include "class.h"

#include "society/ledger.h"

#include <cstdint>
#include <unordered_map>
#include <vector>


// 股市：每个组织一支股票，股价由基本面（组织账户余额/总股数）乘一个随买卖压力浮动的系数得出，
// 当天的买卖单在Clear里按同一成交价撮合。详见market.md。
class Market {
	// 存档：SocietySaveCodec直接读写私有字段(society_save.md)。
	friend class SocietySaveCodec;
public:
	// 成交价下限(分/股)，避免账户余额很小时股价归零。
	static constexpr int64_t kMinPrice = 100;

	// 单支股票的行情，随社会段存档。
	struct Quote {
		double priceFactor = 1.0;       // 买卖压力累积出来的价格系数，没有上下限
		double dividendPerShare = -1.0; // 最近一次月度分红折合的每股分红(分)，0=上月没分红，<0=还没有结算过
	};

	struct Order {
		Citizen* citizen = nullptr;
		Organization* organization = nullptr;
		int64_t shares = 0;
	};

	struct ClearReport {
		int64_t volume = 0;   // 成交股数
		int64_t turnover = 0; // 成交额(分)
		int trades = 0;       // 有成交的股票数
	};

	// 当前成交价(分/股)；倒闭的组织不能交易，返回0。
	int64_t Price(const Ledger& ledger, const Organization* organization) const;
	// 月度分红折算的年化股息率(分红×12/股价)，没有分红记录返回0。
	double AnnualYield(const Ledger& ledger, const Organization* organization) const;
	bool HasDividendHistory(const Organization* organization) const;
	void RecordDividend(const Organization* organization, double dividendPerShare);
	// 组织破产重启后行情复位。
	void ResetQuote(const Organization* organization);

	// 挂买单/卖单（当天有效，Clear之后清空）。
	void Bid(Citizen* citizen, Organization* organization, int64_t shares);
	void Ask(Citizen* citizen, Organization* organization, int64_t shares);

	// 按同一成交价撮合每支股票的买卖单：买量超过供给(市民卖单+组织库存股)时按比例缩量，市民卖单优先成交；
	// 买方付给卖方（卖方是组织库存股时付给组织账户），所以账本总和不变。成交后更新价格系数。
	ClearReport Clear(Ledger& ledger, const std::vector<Organization*>& organizations);

	// 把市民持有的某支股票全部卖给组织库存股（离城清算用），按当前价结算，返回结算额(分)。
	int64_t SellAllToTreasury(Ledger& ledger, Organization* organization, Citizen* citizen);

private:
	std::unordered_map<const Organization*, Quote> quotes;
	std::vector<Order> bids;
	std::vector<Order> asks;
};
