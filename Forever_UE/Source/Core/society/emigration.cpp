#include "society/emigration.h"

#include "society/society.h"
#include "society/organization.h"
#include "map/room.h"
#include "populace/citizen.h"
#include "populace/household.h"

#include <algorithm>


using namespace std;

int64_t Emigration::NetWorth(const Society& society, const Citizen* citizen) {
	const Ledger& ledger = society.GetLedger();
	int64_t worth = ledger.Balance(Party::Of(citizen)) + society.GetTreasury().DepositOf(citizen)
		+ society.GetTreasury().BondsOf(citizen);
	for (const Organization* organization : society.GetOrganizations()) {
		int64_t shares = organization->SharesOf(citizen);
		if (shares > 0) worth += shares * society.GetMarket().Price(ledger, organization);
	}
	return worth;
}

void Emigration::Depart(Society& society, const vector<Room*>& rooms, Citizen* citizen) {
	Ledger& ledger = society.GetLedger();

	// 清算：股票按市价卖给组织库存股，存款取回，国债提前赎回。
	for (Organization* organization : society.GetOrganizations()) {
		society.GetMarket().SellAllToTreasury(ledger, organization, citizen);
	}
	society.GetTreasury().Withdraw(ledger, citizen, society.GetTreasury().DepositOf(citizen));
	society.GetTreasury().RedeemBonds(ledger, citizen);

	// 余额归零：欠款由城市外账户核销，剩下的钱被带走。
	int64_t balance = ledger.Balance(Party::Of(citizen));
	if (balance < 0) ledger.Transfer(Party::Outside(), Party::Of(citizen), -balance, Flow::DEBT_WRITE_OFF);
	else if (balance > 0) ledger.Transfer(Party::Of(citizen), Party::Outside(), balance, Flow::EMIGRATE);

	for (Room* room : rooms) {
		if (room && room->GetOwner() == citizen) room->SetOwner(nullptr);
	}
	citizen->Depart();
}

vector<Citizen*> Emigration::Monthly(Society& society, const vector<Room*>& rooms, const Time& now,
	const Citizen* protectedCitizen) {
	vector<Citizen*> departed;
	for (const Room* room : rooms) {
		if (!room || !room->IsResidential()) continue;
		for (Household* household : room->GetHouseholds()) {
			if (protectedCitizen && household->Contains(protectedCitizen)) continue;
			vector<Citizen*> adults = household->GetAdults(now);
			if (adults.empty()) continue;

			bool allNegative = true;
			for (const Citizen* adult : adults) {
				if (NetWorth(society, adult) >= 0) {
					allNegative = false;
					break;
				}
			}
			if (!allNegative) continue;

			for (Citizen* member : household->GetMembers()) {
				if (!member || !member->IsActive()) continue;
				Depart(society, rooms, member);
				departed.push_back(member);
			}
		}
	}
	return departed;
}
