#pragma once

#include "class.h"

#include <vector>


// 人口迁入：每天按“离1.5倍开局人口还差多少”从城外迁入成年新市民。详见immigration.md。
class Immigration {
public:
	// x=开局市民总数，y=当前在场(IsActive)的市民总数，当天迁入 (1.5x−y)×r 人，r每天取[0,0.5%]的随机数，
	// 小数部分按概率进位。每人：随机姓名/性别/20~45岁，按开局同一分布领积蓄（不动开局注资基准），
	// 住进一间没人住的住宅房间，并求职。没有空房时当天迁不进来。返回迁入的市民，UE层据此生成Actor。
	static std::vector<Citizen*> Daily(Society& society, Populace& populace, Map& map, int currentYear);
};
