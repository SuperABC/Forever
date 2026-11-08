#include "society/market.h"

#include "society/organization.h"
#include "populace/citizen.h"

#include <algorithm>
#include <cmath>
#include <map>


using namespace std;

namespace {
	constexpr double kDailyMove = 0.02;    // 一天买卖压力最多把价格系数推动2%（限速，不是上下限）
	constexpr double kFactorFloor = 1e-3;  // 只为避免系数变成0或负数

	// 把total按各自数量的比例分给各方（total不小于总和时全额），余数依次多给靠前的，总额不变。
	vector<int64_t> Scale(const vector<int64_t>& amounts, int64_t total) {
		int64_t sum = 0;
		for (int64_t amount : amounts) sum += amount;
		vector<int64_t> result(amounts.size(), 0);
		if (sum <= 0 || total <= 0) return result;
		if (total >= sum) return amounts;
		int64_t assigned = 0;
		for (size_t i = 0; i < amounts.size(); i++) {
			result[i] = amounts[i] * total / sum;
			assigned += result[i];
		}
		for (size_t i = 0; assigned < total && i < amounts.size(); i++) {
			if (result[i] < amounts[i]) {
				result[i]++;
				assigned++;
			}
		}
		return result;
	}

	// 同一人的多张单子合并成一份数量。
	void Merge(const vector<const Market::Order*>& orders, vector<Citizen*>& people, vector<int64_t>& quantity) {
		for (const Market::Order* order : orders) {
			auto found = find(people.begin(), people.end(), order->citizen);
			size_t k = found - people.begin();
			if (found == people.end()) {
				people.push_back(order->citizen);
				quantity.push_back(0);
			}
			quantity[k] += order->shares;
		}
	}
}

int64_t Market::Price(const Ledger& ledger, const Organization* organization) const {
	if (!organization || organization->IsBankrupt()) return 0;
	int64_t fundamental = max(kMinPrice, ledger.Balance(Party::Of(organization)) / organization->GetTotalShares());
	auto it = quotes.find(organization);
	double factor = it != quotes.end() ? it->second.priceFactor : 1.0;
	return max(kMinPrice, static_cast<int64_t>(llround(static_cast<double>(fundamental) * factor)));
}

double Market::AnnualYield(const Ledger& ledger, const Organization* organization) const {
	auto it = quotes.find(organization);
	int64_t price = Price(ledger, organization);
	if (it == quotes.end() || price <= 0) return 0.0;
	return max(0.0, it->second.dividendPerShare) * 12.0 / static_cast<double>(price);
}

bool Market::HasDividendHistory(const Organization* organization) const {
	auto it = quotes.find(organization);
	return it != quotes.end() && it->second.dividendPerShare >= 0.0;
}

void Market::RecordDividend(const Organization* organization, double dividendPerShare) {
	quotes[organization].dividendPerShare = dividendPerShare;
}

void Market::ResetQuote(const Organization* organization) {
	quotes[organization] = Quote();
}

void Market::Bid(Citizen* citizen, Organization* organization, int64_t shares) {
	if (citizen && organization && shares > 0) bids.push_back({ citizen, organization, shares });
}

void Market::Ask(Citizen* citizen, Organization* organization, int64_t shares) {
	if (citizen && organization && shares > 0) asks.push_back({ citizen, organization, shares });
}

Market::ClearReport Market::Clear(Ledger& ledger, const vector<Organization*>& organizations) {
	ClearReport report;

	map<const Organization*, vector<const Order*>> bidsBy, asksBy;
	for (const Order& order : bids) bidsBy[order.organization].push_back(&order);
	for (const Order& order : asks) asksBy[order.organization].push_back(&order);

	for (Organization* organization : organizations) {
		int64_t price = Price(ledger, organization);
		if (price <= 0) continue;
		Quote& quote = quotes[organization];

		// 买方：每人最多买到现金买得起的股数。
		vector<Citizen*> buyers;
		vector<int64_t> buyQty;
		if (auto it = bidsBy.find(organization); it != bidsBy.end()) Merge(it->second, buyers, buyQty);
		for (size_t k = 0; k < buyers.size(); k++) {
			int64_t cash = max<int64_t>(0, ledger.Balance(Party::Of(buyers[k])));
			buyQty[k] = min(buyQty[k], cash / price);
		}

		// 卖方：市民卖单不能超过实际持股；组织库存股排在市民之后。
		vector<Citizen*> sellers;
		vector<int64_t> sellQty;
		if (auto it = asksBy.find(organization); it != asksBy.end()) Merge(it->second, sellers, sellQty);
		int64_t citizenSupply = 0;
		for (size_t k = 0; k < sellers.size(); k++) {
			sellQty[k] = min(sellQty[k], organization->SharesOf(sellers[k]));
			citizenSupply += sellQty[k];
		}
		int64_t treasury = organization->TreasuryShares();

		int64_t demand = 0;
		for (int64_t qty : buyQty) demand += qty;
		int64_t traded = min(demand, citizenSupply + treasury);

		// 价格系数看买单和全部供给（市民卖单+库存股）的差，没有上下限也不回落，
		// 价格水平完全由买卖双方的股息率判断（见Investor）推到哪里算哪里。
		// 压力取供需差占总盘子的比例：只要有买单没人接（或卖单没人要），价格就按限速朝那个方向走。
		double pressure = (static_cast<double>(demand) - static_cast<double>(citizenSupply + treasury))
			/ max(1.0, static_cast<double>(demand + citizenSupply + treasury));
		pressure = max(-1.0, min(1.0, pressure));
		quote.priceFactor *= 1.0 + kDailyMove * pressure;
		quote.priceFactor = max(kFactorFloor, quote.priceFactor);

		if (traded <= 0) continue;

		// 买方按比例缩量到traded；卖方先用市民卖单，不够再用库存股。
		vector<int64_t> buyLeft = Scale(buyQty, traded);
		int64_t fromCitizens = min(traded, citizenSupply);
		vector<int64_t> sellLeft = Scale(sellQty, fromCitizens);
		int64_t treasuryLeft = traded - fromCitizens;

		// 两个游标把买单和卖单依次配对成交。
		size_t s = 0;
		for (size_t b = 0; b < buyers.size(); b++) {
			while (buyLeft[b] > 0) {
				while (s < sellers.size() && sellLeft[s] <= 0) s++;
				int64_t chunk = 0;
				Citizen* sellerCitizen = nullptr;
				Party seller = Party::Of(organization);
				if (s < sellers.size()) {
					chunk = min(buyLeft[b], sellLeft[s]);
					sellerCitizen = sellers[s];
					seller = Party::Of(sellerCitizen);
					sellLeft[s] -= chunk;
				}
				else if (treasuryLeft > 0) {
					chunk = min(buyLeft[b], treasuryLeft);
					treasuryLeft -= chunk;
				}
				else {
					break;
				}
				ledger.Transfer(Party::Of(buyers[b]), seller, chunk * price, Flow::STOCK_TRADE);
				organization->AdjustShares(buyers[b], chunk);
				if (sellerCitizen) organization->AdjustShares(sellerCitizen, -chunk);
				buyLeft[b] -= chunk;
				report.volume += chunk;
				report.turnover += chunk * price;
			}
		}
		report.trades++;
	}

	bids.clear();
	asks.clear();
	return report;
}

int64_t Market::SellAllToTreasury(Ledger& ledger, Organization* organization, Citizen* citizen) {
	int64_t shares = organization->SharesOf(citizen);
	if (shares <= 0) return 0;
	int64_t payment = shares * Price(ledger, organization);
	ledger.Transfer(Party::Of(organization), Party::Of(citizen), payment, Flow::STOCK_TRADE);
	organization->AdjustShares(citizen, -shares);
	return payment;
}
