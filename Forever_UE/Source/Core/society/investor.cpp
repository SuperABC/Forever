#include "society/investor.h"

#include "society/society.h"
#include "society/organization.h"
#include "populace/citizen.h"

#include <algorithm>
#include <cmath>


using namespace std;

namespace {
	constexpr int kStockCandidates = 8;  // 每次决策随机看几支股票
	constexpr int kStockPicks = 3;       // 从中挑几支买
	constexpr double kSellFraction = 0.25;

	bool CanInvest(const Citizen* citizen, int currentYear, int adultAge) {
		return citizen && citizen->IsActive() && citizen->IsInEconomy() && citizen->GetAge(currentYear) >= adultAge;
	}

	struct Holding {
		Organization* organization = nullptr;
		int64_t shares = 0;
		double yield = 0.0;
		int64_t price = 0;
	};

	vector<Holding> HoldingsOf(const Society& society, const Citizen* citizen) {
		vector<Holding> holdings;
		for (Organization* organization : society.GetOrganizations()) {
			int64_t shares = organization->SharesOf(citizen);
			if (shares <= 0) continue;
			holdings.push_back({ organization, shares, society.GetMarket().AnnualYield(society.GetLedger(), organization),
				society.GetMarket().Price(society.GetLedger(), organization) });
		}
		return holdings;
	}
}

double Investor::Risk(const Citizen* citizen) {
	const Personality& personality = citizen->GetPersonality();
	double risk = 0.5 * (personality.confidence + (1.0 - personality.reasoning));
	return max(0.0, min(1.0, risk));
}

Investor::Summary Investor::Daily(Society& society, const vector<Citizen*>& citizens, int currentYear, int64_t dayIndex) {
	Summary summary;
	Ledger& ledger = society.GetLedger();
	Treasury& treasury = society.GetTreasury();
	Market& market = society.GetMarket();
	const GovernmentPolicy& policy = society.GetPolicy();
	const vector<Organization*>& organizations = society.GetOrganizations();
	int interval = max(1, policy.investIntervalDays);
	// 股票要和存款比：股息率达到存款年利率才值得持有，价格就这样被买卖双方推到股息率≈存款利率附近。
	double hurdle = static_cast<double>(policy.depositRateBp) / 10000.0;

	for (size_t index = 0; index < citizens.size(); index++) {
		Citizen* citizen = citizens[index];
		if (static_cast<int64_t>(index) % interval != dayIndex % interval) continue;
		if (!CanInvest(citizen, currentYear, policy.adultAge)) continue;
		summary.deciders++;

		int64_t cash = ledger.Balance(Party::Of(citizen));
		if (cash < policy.reserve) {
			// 现金不够缓冲：先取存款，还不够就挂卖单，先卖股息率低的。
			int64_t need = policy.reserve - cash;
			int64_t got = treasury.Withdraw(ledger, citizen, need);
			summary.withdrawn += got;
			need -= got;
			if (need > 0) {
				vector<Holding> holdings = HoldingsOf(society, citizen);
				sort(holdings.begin(), holdings.end(), [](const Holding& a, const Holding& b) { return a.yield < b.yield; });
				for (const Holding& holding : holdings) {
					if (need <= 0) break;
					if (holding.price <= 0) continue;
					int64_t sell = min(holding.shares, (need + holding.price - 1) / holding.price);
					market.Ask(citizen, holding.organization, sell);
					need -= sell * holding.price;
					summary.asks++;
				}
			}
			continue;
		}

		int64_t surplus = cash - policy.reserve;
		double risk = Risk(citizen);

		// 保守的多存款，激进的多买股，各自最多动用富余现金的一半，另一半留着认购国债。
		int64_t depositAmount = static_cast<int64_t>(static_cast<double>(surplus) * (1.0 - risk) * 0.5);
		if (depositAmount > 0) {
			treasury.Deposit(ledger, citizen, depositAmount);
			summary.deposited += depositAmount;
		}

		int64_t stockBudget = static_cast<int64_t>(static_cast<double>(surplus) * risk * 0.9);
		if (stockBudget > 0 && !organizations.empty()) {
			struct Candidate {
				Organization* organization;
				int64_t price;
				double score;
			};
			vector<Candidate> candidates;
			for (int k = 0; k < kStockCandidates; k++) {
				Organization* organization = organizations[GetRandom(static_cast<int>(organizations.size()))];
				int64_t price = market.Price(ledger, organization);
				if (price <= 0) continue;
				// 已经有分红记录的，股息率不到存款利率就不买（存款更划算）；还没结算过的（开局第一个月）可以试着买。
				double yield = market.AnnualYield(ledger, organization);
				if (market.HasDividendHistory(organization) && yield < hurdle) continue;
				double noise = static_cast<double>(GetRandom(100)) / 100.0 * 0.02;
				candidates.push_back({ organization, price, yield + noise });
			}
			sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
			int picks = static_cast<int>(min<size_t>(candidates.size(), kStockPicks));
			for (int k = 0; k < picks; k++) {
				int64_t shares = stockBudget / picks / candidates[k].price;
				if (shares <= 0) continue;
				market.Bid(citizen, candidates[k].organization, shares);
				summary.bids++;
			}
		}

		// 股息率不到存款利率的持仓减一部分。
		for (const Holding& holding : HoldingsOf(society, citizen)) {
			if (holding.price <= 0 || !market.HasDividendHistory(holding.organization) || holding.yield >= hurdle) continue;
			int64_t sell = max<int64_t>(1, static_cast<int64_t>(static_cast<double>(holding.shares) * kSellFraction));
			market.Ask(citizen, holding.organization, min(sell, holding.shares));
			summary.asks++;
		}
	}
	return summary;
}

int64_t Investor::SubscribeBonds(Society& society, const vector<Citizen*>& citizens, int currentYear,
	int64_t needed, int rateBp, int termDays) {
	Ledger& ledger = society.GetLedger();
	const GovernmentPolicy& policy = society.GetPolicy();
	int64_t raised = 0;
	if (needed <= 0 || citizens.empty()) return 0;

	// 从随机位置起轮一圈，免得下标靠前的市民总是先被认购。
	size_t start = static_cast<size_t>(GetRandom(static_cast<int>(citizens.size())));
	for (size_t offset = 0; offset < citizens.size() && raised < needed; offset++) {
		Citizen* citizen = citizens[(start + offset) % citizens.size()];
		if (!CanInvest(citizen, currentYear, policy.adultAge)) continue;
		int64_t surplus = ledger.Balance(Party::Of(citizen)) - policy.reserve;
		if (surplus <= 0) continue;
		int64_t willing = static_cast<int64_t>(static_cast<double>(surplus) * (1.0 - Risk(citizen)) * 0.5);
		int64_t amount = min(willing, needed - raised);
		if (amount <= 0) continue;
		society.GetTreasury().Subscribe(ledger, citizen, amount, rateBp, termDays);
		raised += amount;
	}
	return raised;
}
