#pragma once

#include "class.h"
#include "common/utility.h"

#include <cstdint>
#include <vector>


// 离开城市：一户里所有在场的成年人净资产都为负，整户搬走。详见emigration.md。
class Emigration {
public:
	// 净资产(分)：账户余额 + 存款 + 国债本金 + 持股按当前股价的市值。
	static int64_t NetWorth(const Society& society, const Citizen* citizen);

	// 月度检查所有户，满足条件的整户离开，返回离开的全部市民。protectedCitizen所在的户跳过（玩家当前控制的人）。
	static std::vector<Citizen*> Monthly(Society& society, const std::vector<Room*>& rooms, const Time& now,
		const Citizen* protectedCitizen);

private:
	// 一个市民离开：清算股票/存款/国债，余额归零（负的核销、正的带走），放弃名下房产，打上departed。
	static void Depart(Society& society, const std::vector<Room*>& rooms, Citizen* citizen);
};
