#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <cmath>
#include <random>
#include <algorithm>
#include <memory>


namespace wxdj {

// ---- 平衡系数(数值依据见battle.md"数值平衡"一节，改动前先看那里) ----
// 双招：真气消耗 = DUAL_COST_FACTOR * (两招消耗之和)，威力 = DUAL_POWER_FACTOR * (两招威力之和)。
inline constexpr double DUAL_COST_FACTOR = 1.0;
inline constexpr double DUAL_POWER_FACTOR = 0.6;
// 积攒真气炸弹：每次消耗当前真气的ACCUMULATE_FRACTION，消耗量按倍率存进炸弹。倍率是修炼者的个人属性，
// 由所在阶位决定，按表给定(3-1阶=第1式=1.0，3-2阶=1.2，3-3阶=1.5，3-4阶=1.9，3-5阶=2.4，3-6阶=3.0，4-1阶=3.7，4-2阶=4.5，4-3阶=5.6，4-4阶=7.0，4-5阶=8.5，4-6阶=第18~20式=10)，
// 同一阶位的各式共用这个最大式，见RealmMaxForm/BombMultiplierOfForm。下面是默认值(4-6阶位)。
inline constexpr float ACCUMULATE_FRACTION = 0.2f;
inline constexpr float BOMB_ACCUMULATE_MULTIPLIER = 10.f;
// 释放真气炸弹(能量B=累积量)：对手没出招(跳过/积攒/空炸弹/被禁锢)时受到 B*BOMB_HIT_FACTOR_PASSIVE，
// 对手出了招受到 (B-对手出招威力)*BOMB_HIT_FACTOR_ACTIVE；自己反噬 B*BOMB_BACKFIRE_FACTOR。
// 双方互扔炸弹：双方各受到 (B1+B2)*BOMB_BACKFIRE_FACTOR(两个炸弹之和乘反噬系数，一起生效)。
inline constexpr float BOMB_HIT_FACTOR_PASSIVE = 0.4f;
inline constexpr float BOMB_HIT_FACTOR_ACTIVE = 0.8f;   // 出招自带抵消和减伤/反弹等效果，所以不能把炸弹调小
inline constexpr float BOMB_BACKFIRE_FACTOR = 0.2f;
// 锈蚀场地下，炸弹对双方的系数都是这个(取代上面的命中系数和反噬系数)。
inline constexpr float BOMB_RUST_FACTOR = 0.5f;

// 跳过回合(含被禁锢而无法出招)时敏捷的倍数，只用于躲避对方未被正面应对的普通招式：双方敏捷相同时，
// 跳过的一方有 4/(1+4)=80% 的概率躲开；被禁锢的一方如果选择积攒真气炸弹(没有跳过)，仍然不能躲避。
inline constexpr int SKIP_AGI_FACTOR = 2;

// 连续跳过要多扣敏捷：第1次扣agiLoss，之后每连续多跳一次多扣SKIP_STREAK_EXTRA_LOSS(默认1、1.5、2、2.5...)；
// 只要这回合出了招/积攒/真放了炸弹，连续次数清零；空炸弹不算跳过也不清零。目的是压住"一直跳过"，
// 真气不够时应该改出便宜的低式招式。
inline constexpr float SKIP_STREAK_EXTRA_LOSS = 0.5f;

// 阶位对应的最大式：阶位按xiaohua.txt里的阶位列(3-1~3-6各一式，4-1=7~8式，4-2=9~10式，4-3=11~12式，
// 4-4=13~14式，4-5=15~17式，4-6=18~20式)。传入第几式，返回它所在阶位的最大式。
inline int RealmMaxForm(int form) {
	static const int maxForm[20] = { 1, 2, 3, 4, 5, 6, 8, 8, 10, 10, 12, 12, 14, 14, 17, 17, 17, 20, 20, 20 };
	if (form < 1) form = 1;
	if (form > 20) form = 20;
	return maxForm[form - 1];
}

// 炸弹积攒倍率：所在阶位的最大式/2。
inline float BombMultiplierOfForm(int form) {
	// 按阶位给定：3-1 1.0，3-2 1.2，3-3 1.5，3-4 1.9，3-5 2.4，3-6 3.0，4-1 3.7，4-2 4.5，4-3 5.6，4-4 7.0，4-5 8.5，4-6 10.0
	switch (RealmMaxForm(form)) {
	case 1: return 1.0f; case 2: return 1.2f; case 3: return 1.5f; case 4: return 1.9f; case 5: return 2.4f; case 6: return 3.0f;
	case 8: return 3.7f; case 10: return 4.5f; case 12: return 5.6f; case 14: return 7.0f; case 17: return 8.5f;
	default: return 10.0f;
	}
}

// 五行属性枚举
enum ATTRIBUTE_TYPE {
	ATTRIBUTE_NONE, // 无属性
	ATTRIBUTE_METAL, // 金
	ATTRIBUTE_WOOD, // 木
	ATTRIBUTE_WATER, // 水
	ATTRIBUTE_FIRE, // 火
	ATTRIBUTE_EARTH, // 土
	ATTRIBUTE_ALL // 五行
};

// 玩家五行属性(金木水火土各一个开关，可任意组合)：位掩码，第n位对应ATTRIBUTE_TYPE的值，0=无属性。
// 与Modifier的attrMask同一套编码；无属性/没有这个属性的玩家，所有五行相关规则都不触发。
inline constexpr unsigned ELEMENT_ALL_MASK = (1u << ATTRIBUTE_METAL) | (1u << ATTRIBUTE_WOOD) | (1u << ATTRIBUTE_WATER) | (1u << ATTRIBUTE_FIRE) | (1u << ATTRIBUTE_EARTH);
inline constexpr unsigned ElementBit(ATTRIBUTE_TYPE attr) { return attr == ATTRIBUTE_ALL ? ELEMENT_ALL_MASK : attr == ATTRIBUTE_NONE ? 0u : (1u << attr); }
// mask是否"覆盖"这个属性：无属性永不覆盖；金木水火土要有对应位；五行要五位齐全
inline constexpr bool ElementCovers(unsigned mask, ATTRIBUTE_TYPE attr) {
	return attr != ATTRIBUTE_NONE && (mask & ElementBit(attr)) == ElementBit(attr);
}
inline constexpr double ELEM_POWER_BASE = 1.5;     // 自己拥有的属性的招式，威力倍率
inline constexpr double ELEM_RESIST_OWNED = 1.5;   // 自己拥有的属性的初始抗性(100%=1.0)
inline constexpr double ELEM_SHIELD_KEEP = 0.5;    // 护盾对同属性伤害：扣除的盾量乘数

// 阶位枚举
enum Realm {
	YELLOW_INIT, // 黄阶初期
	YELLOW_INIT_PEAK, // 黄阶初期巅峰
	YELLOW_MID, // 黄阶中期
	YELLOW_MID_PEAK, // 黄阶中期巅峰
	YELLOW_LATE, // 黄阶后期
	YELLOW_LATE_PEAK, // 黄阶后期巅峰
	MYSTIC_INIT, // 玄阶初期
	MYSTIC_INIT_PEAK, // 玄阶初期巅峰
	MYSTIC_MID, // 玄阶中期
	MYSTIC_MID_PEAK, // 玄阶中期巅峰
	MYSTIC_LATE, // 玄阶后期
	MYSTIC_LATE_PEAK, // 玄阶后期巅峰
	EARTH_INIT, // 地阶初期
	EARTH_INIT_PEAK, // 地阶初期巅峰
	EARTH_MID, // 地阶中期
	EARTH_MID_PEAK, // 地阶中期巅峰
	EARTH_LATE, // 地接后期
	EARTH_LATE_PEAK, // 地阶后期巅峰
	HEAVEN_INIT, // 天阶初期
	HEAVEN_INIT_PEAK, // 天阶初期巅峰
	HEAVEN_MID, // 天阶中期
	HEAVEN_MID_PEAK, // 天阶中期巅峰
	HEAVEN_LATE, // 天阶后期
	HEAVEN_LATE_PEAK // 天阶后期巅峰
};

// 属性克制关系
inline const std::map<std::pair<ATTRIBUTE_TYPE, ATTRIBUTE_TYPE>, bool> ATTRIBUTE_COUNTERS = {
	{{ATTRIBUTE_WATER, ATTRIBUTE_FIRE}, true}, // 水克火
	{{ATTRIBUTE_FIRE, ATTRIBUTE_WOOD}, true}, // 火克木
	{{ATTRIBUTE_WOOD, ATTRIBUTE_WATER}, true}, // 木克水
	{{ATTRIBUTE_EARTH, ATTRIBUTE_METAL}, true}, // 土克金
	{{ATTRIBUTE_METAL, ATTRIBUTE_WOOD}, true}, // 金克木
	{{ATTRIBUTE_WOOD, ATTRIBUTE_EARTH}, true}, // 木克土
	{{ATTRIBUTE_ALL, ATTRIBUTE_METAL}, true}, // 五行克金
	{{ATTRIBUTE_ALL, ATTRIBUTE_WOOD}, true}, // 五行克木
	{{ATTRIBUTE_ALL, ATTRIBUTE_WATER}, true}, // 五行克水
	{{ATTRIBUTE_ALL, ATTRIBUTE_FIRE}, true}, // 五行克火
	{{ATTRIBUTE_ALL, ATTRIBUTE_EARTH}, true}, // 五行克土
	{{ATTRIBUTE_METAL, ATTRIBUTE_NONE}, true}, // 金克无属性
	{{ATTRIBUTE_WOOD, ATTRIBUTE_NONE}, true}, // 木克无属性
	{{ATTRIBUTE_WATER, ATTRIBUTE_NONE}, true}, // 水克无属性
	{{ATTRIBUTE_FIRE, ATTRIBUTE_NONE}, true}, // 火克无属性
	{{ATTRIBUTE_EARTH, ATTRIBUTE_NONE}, true}, // 土克无属性
};

// 双属性组合主属性映射
inline const std::map<std::pair<ATTRIBUTE_TYPE, ATTRIBUTE_TYPE>, ATTRIBUTE_TYPE> DUAL_ATTRIBUTE_PRIMARY = {
	{{ATTRIBUTE_WOOD, ATTRIBUTE_FIRE}, ATTRIBUTE_FIRE},
	{{ATTRIBUTE_FIRE, ATTRIBUTE_WOOD}, ATTRIBUTE_FIRE},
	{{ATTRIBUTE_FIRE, ATTRIBUTE_EARTH}, ATTRIBUTE_EARTH},
	{{ATTRIBUTE_EARTH, ATTRIBUTE_FIRE}, ATTRIBUTE_EARTH},
	{{ATTRIBUTE_EARTH, ATTRIBUTE_METAL}, ATTRIBUTE_EARTH},
	{{ATTRIBUTE_METAL, ATTRIBUTE_EARTH}, ATTRIBUTE_EARTH},
	{{ATTRIBUTE_METAL, ATTRIBUTE_WATER}, ATTRIBUTE_WATER},
	{{ATTRIBUTE_WATER, ATTRIBUTE_METAL}, ATTRIBUTE_WATER},
	{{ATTRIBUTE_WATER, ATTRIBUTE_WOOD}, ATTRIBUTE_WOOD},
	{{ATTRIBUTE_WOOD, ATTRIBUTE_WATER}, ATTRIBUTE_WOOD},
	{{ATTRIBUTE_FIRE, ATTRIBUTE_METAL}, ATTRIBUTE_METAL},
	{{ATTRIBUTE_METAL, ATTRIBUTE_FIRE}, ATTRIBUTE_METAL},
	{{ATTRIBUTE_METAL, ATTRIBUTE_WOOD}, ATTRIBUTE_METAL},
	{{ATTRIBUTE_WOOD, ATTRIBUTE_METAL}, ATTRIBUTE_METAL},
	{{ATTRIBUTE_WOOD, ATTRIBUTE_EARTH}, ATTRIBUTE_WOOD},
	{{ATTRIBUTE_EARTH, ATTRIBUTE_WOOD}, ATTRIBUTE_WOOD},
	{{ATTRIBUTE_EARTH, ATTRIBUTE_WATER}, ATTRIBUTE_EARTH},
	{{ATTRIBUTE_WATER, ATTRIBUTE_EARTH}, ATTRIBUTE_EARTH},
	{{ATTRIBUTE_WATER, ATTRIBUTE_FIRE}, ATTRIBUTE_WATER},
	{{ATTRIBUTE_FIRE, ATTRIBUTE_WATER}, ATTRIBUTE_WATER},
	{{ATTRIBUTE_ALL, ATTRIBUTE_METAL}, ATTRIBUTE_METAL},
	{{ATTRIBUTE_METAL, ATTRIBUTE_ALL}, ATTRIBUTE_METAL},
	{{ATTRIBUTE_ALL, ATTRIBUTE_WOOD}, ATTRIBUTE_WOOD},
	{{ATTRIBUTE_WOOD, ATTRIBUTE_ALL}, ATTRIBUTE_WOOD},
	{{ATTRIBUTE_ALL, ATTRIBUTE_WATER}, ATTRIBUTE_WATER},
	{{ATTRIBUTE_WATER, ATTRIBUTE_ALL}, ATTRIBUTE_WATER},
	{{ATTRIBUTE_ALL, ATTRIBUTE_FIRE}, ATTRIBUTE_FIRE},
	{{ATTRIBUTE_FIRE, ATTRIBUTE_ALL}, ATTRIBUTE_FIRE},
	{{ATTRIBUTE_ALL, ATTRIBUTE_EARTH}, ATTRIBUTE_EARTH},
	{{ATTRIBUTE_EARTH, ATTRIBUTE_ALL}, ATTRIBUTE_EARTH},
	{{ATTRIBUTE_METAL, ATTRIBUTE_METAL}, ATTRIBUTE_METAL},
	{{ATTRIBUTE_WOOD, ATTRIBUTE_WOOD}, ATTRIBUTE_WOOD},
	{{ATTRIBUTE_WATER, ATTRIBUTE_WATER}, ATTRIBUTE_WATER},
	{{ATTRIBUTE_FIRE, ATTRIBUTE_FIRE}, ATTRIBUTE_FIRE},
	{{ATTRIBUTE_EARTH, ATTRIBUTE_EARTH}, ATTRIBUTE_EARTH},
	{{ATTRIBUTE_ALL, ATTRIBUTE_ALL}, ATTRIBUTE_ALL},
	{{ATTRIBUTE_NONE, ATTRIBUTE_METAL}, ATTRIBUTE_METAL},
	{{ATTRIBUTE_METAL, ATTRIBUTE_NONE}, ATTRIBUTE_METAL},
	{{ATTRIBUTE_NONE, ATTRIBUTE_WOOD}, ATTRIBUTE_WOOD},
	{{ATTRIBUTE_WOOD, ATTRIBUTE_NONE}, ATTRIBUTE_WOOD},
	{{ATTRIBUTE_NONE, ATTRIBUTE_WATER}, ATTRIBUTE_WATER},
	{{ATTRIBUTE_WATER, ATTRIBUTE_NONE}, ATTRIBUTE_WATER},
	{{ATTRIBUTE_NONE, ATTRIBUTE_FIRE}, ATTRIBUTE_FIRE},
	{{ATTRIBUTE_FIRE, ATTRIBUTE_NONE}, ATTRIBUTE_FIRE},
	{{ATTRIBUTE_NONE, ATTRIBUTE_EARTH}, ATTRIBUTE_EARTH},
	{{ATTRIBUTE_EARTH, ATTRIBUTE_NONE}, ATTRIBUTE_EARTH},
	{{ATTRIBUTE_METAL, ATTRIBUTE_METAL}, ATTRIBUTE_METAL},
	{{ATTRIBUTE_WOOD, ATTRIBUTE_WOOD}, ATTRIBUTE_WOOD},
	{{ATTRIBUTE_WATER, ATTRIBUTE_WATER}, ATTRIBUTE_WATER},
	{{ATTRIBUTE_FIRE, ATTRIBUTE_FIRE}, ATTRIBUTE_FIRE},
	{{ATTRIBUTE_EARTH, ATTRIBUTE_EARTH}, ATTRIBUTE_EARTH},
	{{ATTRIBUTE_NONE, ATTRIBUTE_NONE}, ATTRIBUTE_NONE},
	{{ATTRIBUTE_ALL, ATTRIBUTE_NONE}, ATTRIBUTE_ALL},
	{{ATTRIBUTE_NONE, ATTRIBUTE_ALL}, ATTRIBUTE_ALL}
};

// 阶位要求的最小 ATK 值
inline const std::map<Realm, int> REALM_ATK_REQUIREMENTS = {
	// 黄阶
	{Realm::YELLOW_INIT, 20},
	{Realm::YELLOW_INIT_PEAK, 30},
	{Realm::YELLOW_MID, 40},
	{Realm::YELLOW_MID_PEAK, 50},
	{Realm::YELLOW_LATE, 60},
	{Realm::YELLOW_LATE_PEAK, 80},

	// 玄阶
	{Realm::MYSTIC_INIT, 100},
	{Realm::MYSTIC_INIT_PEAK, 120},
	{Realm::MYSTIC_MID, 140},
	{Realm::MYSTIC_MID_PEAK, 160},
	{Realm::MYSTIC_LATE, 180},
	{Realm::MYSTIC_LATE_PEAK, 200},

	// 地阶
	{Realm::EARTH_INIT, 250},
	{Realm::EARTH_INIT_PEAK, 300},
	{Realm::EARTH_MID, 350},
	{Realm::EARTH_MID_PEAK, 400},
	{Realm::EARTH_LATE, 450},
	{Realm::EARTH_LATE_PEAK, 500},

	// 天阶
	{Realm::HEAVEN_INIT, 600},
	{Realm::HEAVEN_INIT_PEAK, 700},
	{Realm::HEAVEN_MID, 800},
	{Realm::HEAVEN_MID_PEAK, 900},
	{Realm::HEAVEN_LATE, 1000},
	{Realm::HEAVEN_LATE_PEAK, 1200}
};

enum STAGE_TYPE {
	STAGE_START, // 回合开始阶段
	STAGE_ACTION, // 回合行动阶段
	STAGE_CHECK, // 回合结算阶段
	STAGE_END // 回合结束阶段
};

enum ACTION_TYPE {
	ACTION_SKIP, // 跳过回合
	ACTION_ACCUMULATE, // 积攒真气
	ACTION_BOMB, // 真气炸弹
	ACTION_SINGLE, // 单发招式
	ACTION_DUAL, // 双发招式
};

enum EFFECT_TYPE {
	EFFECT_NONE, // 无
	EFFECT_PHYSICAL, // 物理伤害
	EFFECT_PENETRATE, // 穿透
	EFFECT_REDUCE, // 减伤
	EFFECT_REBOUND, // 反弹
	EFFECT_ABSORB, // 吸收真气
	EFFECT_LOCK, // 禁锢
	EFFECT_DOT, // 持续伤害
	EFFECT_REBATE, // 回复真气
	EFFECT_RECOVER, // 回复武力值
	EFFECT_CURE, // 回复健康值
	EFFECT_BURN, // 金火灼烧
	EFFECT_RUST, // 金水锈蚀
	EFFECT_DUST, // 土火扬尘
	EFFECT_MUD, // 土水泥沼
	EFFECT_INCREASE, // 连续升式增伤(招式效果，不进双招的合并效果)
	EFFECT_SHIELD, // 护盾(同时保护武力和健康)
	EFFECT_CRIT, // 暴击(只用于日志)
	EFFECT_TRAIT, // 特性触发/生效(只用于日志：name="资源|来源"，资源是mp/atk/hp/agi，amount带符号)
};

// 招式名是"家族名+第X式"：拆出家族名和式号(第一式=1 ... 第二十式=20)；名字里没有"第"返回式号0(不参与连招)。
inline std::string FamilyOfName(const std::string& name) {
	size_t p = name.find("\xE7\xAC\xAC"); // "第"的UTF-8
	return p == std::string::npos ? name : name.substr(0, p);
}
inline int FormOfName(const std::string& name) {
	size_t p = name.find("\xE7\xAC\xAC");
	if (p == std::string::npos) return 0;
	std::string s = name.substr(p + 3);
	size_t q = s.find("\xE5\xBC\x8F"); // "式"
	if (q != std::string::npos) s = s.substr(0, q);
	static const char* digits[] = { "\xE4\xB8\x80", "\xE4\xBA\x8C", "\xE4\xB8\x89", "\xE5\x9B\x9B", "\xE4\xBA\x94",
		"\xE5\x85\xAD", "\xE4\xB8\x83", "\xE5\x85\xAB", "\xE4\xB9\x9D" };   // 一~九
	int total = 0, cur = 0;
	for (size_t i = 0; i + 2 < s.size() + 0 && i + 3 <= s.size(); i += 3) {
		std::string ch = s.substr(i, 3);
		if (ch == "\xE5\x8D\x81") { total += (cur == 0 ? 1 : cur) * 10; cur = 0; }   // 十
		else {
			for (int d = 0; d < 9; ++d) if (ch == digits[d]) { cur = d + 1; break; }
		}
	}
	return total + cur;
}

}
