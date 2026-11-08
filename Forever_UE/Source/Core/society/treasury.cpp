#include "society/treasury.h"

#include "populace/citizen.h"

#include <algorithm>


using namespace std;

int64_t Treasury::DepositOf(const Citizen* citizen) const {
	auto it = deposits.find(citizen);
	return it != deposits.end() ? it->second : 0;
}

int64_t Treasury::TotalDeposits() const {
	int64_t total = 0;
	for (const auto& [citizen, amount] : deposits) total += amount;
	return total;
}

int64_t Treasury::BondsOf(const Citizen* citizen) const {
	int64_t total = 0;
	for (const Bond& bond : bonds) {
		if (bond.holder == citizen) total += bond.principal;
	}
	return total;
}

int64_t Treasury::TotalBonds() const {
	int64_t total = 0;
	for (const Bond& bond : bonds) total += bond.principal;
	return total;
}

int64_t Treasury::NetPosition(const Ledger& ledger) const {
	return ledger.Balance(Party::Government()) - TotalDeposits() - TotalBonds();
}

void Treasury::Deposit(Ledger& ledger, Citizen* citizen, int64_t amount) {
	if (!citizen || amount <= 0) return;
	ledger.Transfer(Party::Of(citizen), Party::Government(), amount, Flow::DEPOSIT);
	deposits[citizen] += amount;
}

int64_t Treasury::Withdraw(Ledger& ledger, Citizen* citizen, int64_t amount) {
	auto it = deposits.find(citizen);
	if (it == deposits.end() || amount <= 0) return 0;
	int64_t taken = min(amount, it->second);
	ledger.Transfer(Party::Government(), Party::Of(citizen), taken, Flow::WITHDRAW);
	it->second -= taken;
	if (it->second <= 0) deposits.erase(it);
	return taken;
}

void Treasury::Subscribe(Ledger& ledger, Citizen* citizen, int64_t amount, int rateBp, int termDays) {
	if (!citizen || amount <= 0) return;
	ledger.Transfer(Party::Of(citizen), Party::Government(), amount, Flow::BOND_ISSUE);
	bonds.push_back({ citizen, amount, rateBp, day + termDays });
}

int64_t Treasury::RedeemBonds(Ledger& ledger, Citizen* citizen) {
	int64_t total = 0;
	for (size_t i = 0; i < bonds.size();) {
		if (bonds[i].holder != citizen) {
			i++;
			continue;
		}
		ledger.Transfer(Party::Government(), Party::Of(citizen), bonds[i].principal, Flow::BOND_REPAY);
		total += bonds[i].principal;
		bonds.erase(bonds.begin() + i);
	}
	return total;
}

int64_t Treasury::RepayMatured(Ledger& ledger) {
	int64_t total = 0;
	for (size_t i = 0; i < bonds.size();) {
		if (bonds[i].maturityDay > day) {
			i++;
			continue;
		}
		ledger.Transfer(Party::Government(), Party::Of(bonds[i].holder), bonds[i].principal, Flow::BOND_REPAY);
		total += bonds[i].principal;
		bonds.erase(bonds.begin() + i);
	}
	return total;
}

Treasury::Interest Treasury::PayInterest(Ledger& ledger, const GovernmentPolicy& policy) {
	Interest result;
	auto pay = [&](Citizen* citizen, int64_t interest, Flow flow, int64_t& sum) {
		if (interest <= 0) return;
		ledger.Transfer(Party::Government(), Party::Of(citizen), interest, flow);
		int64_t tax = policy.Tax(interest, policy.incomeTaxPercent);
		ledger.Transfer(Party::Of(citizen), Party::Government(), tax, Flow::INCOME_TAX);
		sum += interest;
		result.tax += tax;
	};
	for (const auto& [citizen, amount] : deposits) {
		int64_t interest = amount * policy.depositRateBp / 10000 / 12;
		pay(const_cast<Citizen*>(citizen), interest, Flow::DEPOSIT_INTEREST, result.deposit);
	}
	for (const Bond& bond : bonds) {
		int64_t interest = bond.principal * bond.rateBp / 10000 / 12;
		pay(bond.holder, interest, Flow::BOND_INTEREST, result.bond);
	}
	return result;
}
