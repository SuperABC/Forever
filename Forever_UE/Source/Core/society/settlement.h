#pragma once

#include "class.h"

#include "society/ledger.h"

#include <cstdint>
#include <vector>


// 一次货款结算的汇总，用于报表。
struct GoodsSettlementReport {
	int64_t internalPaid = 0;   // 城内流转货款（买方付给卖方）
	int64_t importCost = 0;     // 进口货款（买方实付的含税进口价总额，其中关税部分付给政府）
	int64_t tariff = 0;         // 进口关税（含在进口价里，买方付给政府的那一份）
	int64_t exportRevenue = 0;  // 出口收入（城市外付给卖方）
	int goodsFlows = 0;
};

// 一次房租结算的汇总。
struct RentReport {
	int64_t rentPaid = 0;       // 租客/组织付给房东的房租
	int64_t rentTax = 0;        // 房东交给政府的房租税（即时缴纳）
	int paidRooms = 0;
	int multiHouseholdRooms = 0; // 有多户合住（房租均分）的住宅房间数
};

// 把"钱"和Industry的货物流水对上：Industry只记录货物流向，付款收款在这里完成。详见settlement.md。
class Settlement {
public:
	// 新开局注资：城市外账户给每个市民N(5000元,2000元)（不小于0），给每个组织mod指定的初始资金。
	static void InitFunds(Society& society, const std::vector<Citizen*>& citizens);

	// 给开局之后新生成的市民(spawn_npc)单独开钱包注资：amountCents<0按开局同一个正态分布随机，
	// 否则用指定金额(分)。resetBaseline=true(默认)同样计入开局注资基准，避免三重负反馈的基准偏小；
	// 迁入人口传false，基准保持开局值。是否接入经济系统无关。
	static void InitFunds(Society& society, Citizen* citizen, int64_t amountCents = -1, bool resetBaseline = true);

	// 按本日货物流水结算货款：城内流转按批发价、进口按进口价（外加关税）、出口按出口价。
	static GoodsSettlementReport SettleGoods(Society& society, const Industry& industry);

	// 日结房租：每个有房租的房间由使用者付给房东（私有=owner，公有或无主=政府），房东再按房租税率把房租收入的一部分当场交给政府。住宅由里面的户付（多户合住均分，每户再由成年成员均摊，没人住不收），
	// 工坊/商店等由占用该房间的组织付。
	static RentReport PayRents(Society& society, const std::vector<Room*>& rooms, int currentYear);
};
