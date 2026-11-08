#pragma once

#include "class.h"

#include "society/ledger.h"
#include "society/policy.h"

#include <cstdint>
#include <unordered_map>
#include <vector>


// 政府的负债簿：市民存款和国债。钱本身已经转进Ledger的政府账户，这里只记政府欠谁多少，
// 所以政府净头寸 = 政府账户 − 存款 − 国债本金。详见treasury.md。
class Treasury {
	// 存档：SocietySaveCodec直接读写私有字段(society_save.md)。
	friend class SocietySaveCodec;
public:
	struct Bond {
		Citizen* holder = nullptr;
		int64_t principal = 0;
		int rateBp = 0;          // 年利率，基点
		int64_t maturityDay = 0; // 到期的天序号，见GetDay
	};

	// 天序号：每次Economy::Daily加1，国债到期按它算。状态，随社会段存档。
	int64_t GetDay() const { return day; }
	void AdvanceDay() { day++; }

	int64_t DepositOf(const Citizen* citizen) const;
	int64_t TotalDeposits() const;
	int64_t BondsOf(const Citizen* citizen) const;
	int64_t TotalBonds() const;
	const std::vector<Bond>& GetBonds() const { return bonds; }

	// 政府净头寸(分)：负反馈和发债判断都用它。借来的钱和存款同时抬高账户与负债，所以不会改善它。
	int64_t NetPosition(const Ledger& ledger) const;

	// 存入：市民 -> 政府 (DEPOSIT)；amount不超过市民当前余额由调用方保证。
	void Deposit(Ledger& ledger, Citizen* citizen, int64_t amount);
	// 取出至多amount：政府 -> 市民 (WITHDRAW)，政府账户可以被取成负数。返回实际取出额（不超过存款）。
	int64_t Withdraw(Ledger& ledger, Citizen* citizen, int64_t amount);
	// 认购一笔国债：市民 -> 政府 (BOND_ISSUE)，记一张期限termDays天的债券。
	void Subscribe(Ledger& ledger, Citizen* citizen, int64_t amount, int rateBp, int termDays);
	// 提前赎回这个市民的全部国债（政府按本金付，BOND_REPAY），返回赎回本金。
	int64_t RedeemBonds(Ledger& ledger, Citizen* citizen);
	// 到期的国债本金还给持有人 (BOND_REPAY)，返回当天偿还总额。
	int64_t RepayMatured(Ledger& ledger);

	struct Interest {
		int64_t deposit = 0; // 存款利息总额
		int64_t bond = 0;    // 国债利息总额
		int64_t tax = 0;     // 利息所得税总额
	};
	// 月度结息：存款按policy.depositRateBp、债券按各自利率，年利率/12；利息当场按incomeTaxPercent缴所得税。
	Interest PayInterest(Ledger& ledger, const GovernmentPolicy& policy);

private:
	int64_t day = 0;
	std::unordered_map<const Citizen*, int64_t> deposits;
	std::vector<Bond> bonds;
};
