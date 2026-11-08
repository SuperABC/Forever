#include "society/settlement.h"

#include "society/society.h"
#include "society/organization.h"
#include "industry/industry.h"
#include "map/room.h"
#include "populace/household.h"
#include "populace/citizen.h"

#include "common/utility.h"

#include <algorithm>
#include <cmath>


using namespace std;

namespace {
	// 一笔钱平均分成n份，余数（分）依次多给前几份，保证总额不变。
	vector<int64_t> SplitEvenly(int64_t amount, size_t parts) {
		vector<int64_t> shares(parts, parts ? amount / static_cast<int64_t>(parts) : 0);
		for (size_t i = 0; parts && i < static_cast<size_t>(amount % static_cast<int64_t>(parts)); i++) shares[i]++;
		return shares;
	}

	constexpr float kCitizenFundsMean = 500000.f;   // 5000元
	constexpr float kCitizenFundsStddev = 200000.f; // 2000元
}

void Settlement::InitFunds(Society& society, const vector<Citizen*>& citizens) {
	Ledger& ledger = society.GetLedger();

	for (Citizen* citizen : citizens) {
		if (!citizen) continue;
		float funds = max(0.f, GetRandomNormal(kCitizenFundsMean, kCitizenFundsStddev));
		ledger.Transfer(Party::Outside(), Party::Of(citizen), static_cast<int64_t>(llround(funds)), Flow::INJECTION);
	}

	for (Organization* organization : society.GetOrganizations()) {
		if (!organization || !organization->GetMod()) continue;
		ledger.Transfer(Party::Outside(), Party::Of(organization), organization->GetMod()->initialFunds, Flow::INJECTION);
	}

	// 开局注资总额记作三重负反馈的基准(出口阈值/税率/补助都按它折算)。
	ledger.SetInitialInjection(-ledger.Balance(Party::Outside()));
}

void Settlement::InitFunds(Society& society, Citizen* citizen, int64_t amountCents, bool resetBaseline) {
	if (!citizen) return;
	Ledger& ledger = society.GetLedger();
	if (amountCents < 0) {
		amountCents = static_cast<int64_t>(llround(max(0.f, GetRandomNormal(kCitizenFundsMean, kCitizenFundsStddev))));
	}
	ledger.Transfer(Party::Outside(), Party::Of(citizen), amountCents, Flow::INJECTION);
	if (resetBaseline) ledger.SetInitialInjection(-ledger.Balance(Party::Outside()));
}

GoodsSettlementReport Settlement::SettleGoods(Society& society, const Industry& industry) {
	GoodsSettlementReport report;
	Ledger& ledger = society.GetLedger();
	const GovernmentPolicy& policy = society.GetPolicy();

	for (const GoodsFlow& flow : industry.GetFlows()) {
		const Product* product = industry.FindProduct(flow.product);
		if (!product || flow.quantity <= 0) continue;
		report.goodsFlows++;

		if (flow.IsImport()) {
			Party buyer = society.OwnerOf(industry.GetStorageRoom(flow.to));
			int64_t cost = product->GetImportPrice() * flow.quantity;
			// 进口价已经含关税：税率是对不含税价的25%，对应含税进口价的25/125=20%。本地进口方总共只付
			// 进口价，其中关税那一份付给政府，其余付给城市外。
			int64_t tariff = cost * policy.tariffPercent / (100 + policy.tariffPercent);
			ledger.Transfer(buyer, Party::Outside(), cost - tariff, Flow::GOODS_IMPORT);
			ledger.Transfer(buyer, Party::Government(), tariff, Flow::TARIFF);
			report.importCost += cost;
			report.tariff += tariff;
		}
		else if (flow.IsExport()) {
			Party seller = society.OwnerOf(industry.GetStorageRoom(flow.from));
			int64_t revenue = product->GetExportPrice() * flow.quantity;
			ledger.Transfer(Party::Outside(), seller, revenue, Flow::GOODS_EXPORT);
			report.exportRevenue += revenue;
		}
		else {
			Party seller = society.OwnerOf(industry.GetStorageRoom(flow.from));
			Party buyer = society.OwnerOf(industry.GetStorageRoom(flow.to));
			int64_t payment = product->GetWholesalePrice() * flow.quantity;
			ledger.Transfer(buyer, seller, payment, Flow::GOODS_INTERNAL);
			report.internalPaid += payment;
		}
	}
	return report;
}

RentReport Settlement::PayRents(Society& society, const vector<Room*>& rooms, int currentYear) {
	RentReport report;
	Ledger& ledger = society.GetLedger();
	const GovernmentPolicy& policy = society.GetPolicy();

	for (const Room* room : rooms) {
		if (!room || room->Rent() <= 0) continue;
		int64_t rent = room->Rent();

		Party landlord = Party::Government();
		if (!room->GetStated() && room->GetOwner()) landlord = Party::Of(room->GetOwner());

		if (room->IsResidential()) {
			const vector<Household*>& households = room->GetHouseholds();
			if (households.empty()) continue; // 没人住不收租
			if (households.size() > 1) report.multiHouseholdRooms++;

			vector<int64_t> householdShares = SplitEvenly(rent, households.size());
			for (size_t i = 0; i < households.size(); i++) {
				// 户里的成年人均摊；没有成年人（理论上不会发生）时由全体成员均摊。
				vector<Citizen*> payers = households[i]->GetAdults(currentYear);
				if (payers.empty()) {
					for (Citizen* member : households[i]->GetMembers()) {
						if (member && member->IsActive()) payers.push_back(member);
					}
				}
				if (payers.empty()) continue; // 全家离城或去世的户不付房租
				vector<int64_t> shares = SplitEvenly(householdShares[i], payers.size());
				for (size_t k = 0; k < payers.size(); k++) ledger.Transfer(Party::Of(payers[k]), landlord, shares[k], Flow::RENT);
			}
		}
		else {
			Party payer = society.OwnerOf(room);
			// 倒闭停业的企业不付房租。
			if (payer.kind == Party::ORGANIZATION && static_cast<const Organization*>(payer.ref)->IsBankrupt()) continue;
			ledger.Transfer(payer, landlord, rent, Flow::RENT);
		}
		report.rentPaid += rent;
		report.paidRooms++;

		// 房租税即时缴纳：房东（政府自己收租不需要交税）把房租收入的一部分交给政府。
		if (landlord.kind != Party::GOVERNMENT) {
			int64_t tax = policy.Tax(rent, policy.rentTaxPercent);
			ledger.Transfer(landlord, Party::Government(), tax, Flow::RENT_TAX);
			report.rentTax += tax;
		}
	}
	return report;
}
