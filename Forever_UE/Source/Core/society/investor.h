#pragma once

#include "class.h"
#include "common/utility.h"

#include <cstdint>
#include <vector>


// 市民的理财策略：按风险偏好决定把富余现金存进政府、买股票还是认购国债，缺钱时取款、卖股。
// 纯函数式，不持有状态，详见investor.md。
class Investor {
public:
	// 风险偏好[0,1]，由性格推出：自信、不理性的更激进，用来在存款和股票之间分配。
	static double Risk(const Citizen* citizen);

	struct Summary {
		int deciders = 0;       // 今天轮到决策的市民数
		int64_t deposited = 0;  // 新增存款(分)
		int64_t withdrawn = 0;  // 取出存款(分)
		int bids = 0;           // 挂出的买单数
		int asks = 0;           // 挂出的卖单数
	};

	// 每天：policy.investIntervalDays天一轮，今天轮到的成年市民各做一次决策，存取款当场执行，
	// 股票只挂单，由Market::Clear撮合。dayIndex是Treasury::GetDay。
	static Summary Daily(Society& society, const std::vector<Citizen*>& citizens, const Time& now, int64_t dayIndex);

	// 国债发行日：按风险偏好（越保守越愿意买）让有富余现金的市民认购，直到凑够needed或没人买，
	// 返回实际募集额(分)。
	static int64_t SubscribeBonds(Society& society, const std::vector<Citizen*>& citizens, const Time& now,
		int64_t needed, int rateBp, int termDays);
};
