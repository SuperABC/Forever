#pragma once

#include "action.h"
#include "cultivator.h"

#include <array>
#include <memory>
#include <random>
#include <string>
#include <vector>


namespace wxdj {

// 自博弈训练出的对战策略(表格式)，驱动敌方出招。
// 第一层按局面(324格)选招类：跳过/积攒/释放炸弹/单招/双招；
// 第二层按上下文(32格：自己真气档位x对手上一招属性)选具体招式家族。
// 策略文件由训练工具产出，格式见battle.md的"对战AI"一节。
class PolicyAI {
public:
	PolicyAI();

	// 加载策略文件，成功返回true。策略里的家族数必须等于家族个数。
	bool Load(const std::string& file, int familyCount);

	// 每个家族取哪一式出招由调用方给出：familyActions[f]是家族f用于出招的招式下标
	std::shared_ptr<Action> Choose(Cultivator& me, const Cultivator& opponent,
		const std::vector<int>& familyActions);

	// 记录对手这回合出的招，下一回合的局面判断要用
	void Observe(const Action& opponentAction);

	// 新的一场战斗开始时清掉对手历史
	void Reset();

private:
	static const int CLASS_COUNT = 5;
	static const int STATE_COUNT = 4 * 3 * 3 * 3 * 3;
	static const int CONTEXT_COUNT = 4 * 8;

	int StateOf(const Cultivator& me, const Cultivator& opponent) const;
	int ContextOf(const Cultivator& me) const;
	int PickFrom(const double* weights, int count);

	int familyCount = 0;
	std::vector<std::array<double, CLASS_COUNT>> classProb; // STATE_COUNT x CLASS_COUNT
	std::vector<std::vector<double>> singleProb;            // CONTEXT_COUNT x 家族数
	std::vector<std::vector<double>> dualProb;              // CONTEXT_COUNT x 家族数

	int opponentAccumulate = 0; // 对手连续积攒的次数
	int opponentAttribute = 7;  // 对手上一招的属性，7=非攻击招
	std::mt19937 rng;
};

}
