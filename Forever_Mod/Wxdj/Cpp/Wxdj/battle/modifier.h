#pragma once

#include "def.h"

#include <string>
#include <vector>


namespace wxdj {

class Cultivator;

// 特性(外部效果)：装备/秘诀/丹药/场地都用这一套，引擎不区分来源(source只用于日志和移除)。
// 一条特性 = 种类 + 数值 + 属性掩码 + 条件 + 持续回合 + 每回合类的执行时机。给谁加由AddModifier的持有者参数决定。
// 同一种类的数值取加和，持续时间各算各的。具体含义和接入点见battle.md的"特性系统"一节。
enum MOD_KIND {
	// 每回合资源(value；percent=按上限的百分比；正=回复，负=流逝/扣除；timing=回合开始/回合结束)
	MOD_MP_ROUND, MOD_ATK_ROUND, MOD_HP_ROUND,
	// 敏捷
	MOD_SKIP_AGI,       // 跳过敏捷扣除调整(加到agiLoss上)
	MOD_AGI_RECOVER,    // 每回合回复敏捷调整
	MOD_AGI_MAX,        // 敏捷上限增/减(percent=按基础上限的百分比)
	// 上限
	MOD_MP_MAX,         // 真气上限增/减(percent=按基础上限的百分比)；真气>2x上限即死的阈值跟着走
	// 炸弹
	MOD_ACC_RATIO,      // 单次积攒比例调整(加到ACCUMULATE_FRACTION上)
	MOD_BOMB_POWER,     // 炸弹威力增/减(炸弹自己的乘区：1+加和)
	// 招式乘区(value是小数，0.2=+20%，同乘区内加算、乘区之间相乘)
	MOD_BASE_POWER,     // x属性基础威力增减(加算进基础伤害；percent=按该招基础威力的百分比)
	MOD_DMG_DEALT,      // 自己造成x属性伤害增/减(增伤乘区)
	MOD_DMG_TAKEN,      // 受到x属性伤害增加(易伤，只增不减：负数无效)；和减伤效果(海龙金身)同一个乘区：1+易伤加和-减伤比例，在TakeDamage里结算
	MOD_FIELD_MULT,     // 场地x属性伤害倍率增/减(以1为基准加减，灼烧本身是1.5)
	MOD_CRIT_RATE,      // 暴击率增/减
	MOD_CRIT_DMG,       // 暴击伤害增/减
	MOD_DODGE,          // 闪避率乘区(和锈蚀同乘区：1+加和-锈蚀1)
	MOD_HIT,            // 命中率乘区(和扬尘同乘区：1+加和-扬尘0.5)
	MOD_COST,           // x属性消耗调整(百分比，0.2=+20%)
	// 触发(percent=按这次伤害量的百分比；正=回复，负=扣除；作用在持有者自己身上)
	MOD_DEAL_MP, MOD_DEAL_ATK, MOD_DEAL_HP,   // 自己造成x属性伤害时
	MOD_TAKE_MP, MOD_TAKE_ATK, MOD_TAKE_HP,   // 自己受到x属性伤害时
	MOD_REBOUND,        // 受到x属性伤害的反弹比例(加到反弹效果上，夹到[0,1])
	MOD_IMMUNE,         // 免疫x属性(该属性的伤害变0)
	// 立即类(AddModifier时马上执行，不保存，没有持续回合)
	MOD_NOW_MP, MOD_NOW_ATK, MOD_NOW_HP, MOD_NOW_AGI,   // 立即增/减
	MOD_NOW_FIELD,      // 立即添加场地效果：value=EFFECT_BURN/RUST/DUST/MUD
	MOD_CLEAR_FIELD,    // 立即清除全部场地效果
	MOD_UNLOCK,         // 立即解除禁锢
	MOD_NOW_SHIELD,     // 立即添加护盾：value=强度，attr=护盾属性(金木水火土各一位，all=五位齐全，none=无属性)，dur=持续回合(-1=不衰减)
	// 玩家五行属性(Cultivator::SetElements)相关，都是增量，没有时走默认值
	MOD_RESIST,         // 对x属性的抗性增/减(基础：拥有该属性1.5，否则1.0；夹到>=0)；实际伤害=伤害x2/(抗性+1)
	MOD_ELEM_POWER,     // 自己拥有的属性的招式的威力倍率增/减(基础1.5；夹到>=0)，attr限定招式属性
	MOD_KIND_COUNT
};

enum MOD_TIMING { MOD_AT_START, MOD_AT_END };

// 条件：持有者的某项状态占上限的百分比 低于/高于 阈值才生效
enum MOD_STAT { MOD_STAT_NONE, MOD_STAT_HP, MOD_STAT_ATK, MOD_STAT_MP, MOD_STAT_AGI };
struct ModCond {
	MOD_STAT stat = MOD_STAT_NONE;
	bool less = true;       // true=低于，false=高于
	double pct = 0;         // 阈值，0~100
};

// 属性掩码：按ATTRIBUTE_TYPE的值，第0位=无属性，1=金，2=木，3=水，4=火，5=土，6=五行
inline constexpr unsigned MOD_ATTR_ALL = 0x7F;

struct Modifier {
	MOD_KIND kind = MOD_MP_ROUND;
	double value = 0;
	bool percent = false;
	unsigned attrMask = MOD_ATTR_ALL;
	int duration = -1;                 // 剩余回合数，-1=永久；每回合结束-1，到0移除
	MOD_TIMING timing = MOD_AT_START;
	ModCond cond;
	std::string source;
};

class ModifierSet {
public:
	void Add(const Modifier& m);
	void RemoveBySource(const std::string& source);
	void Clear();
	const std::vector<Modifier>& All() const;

	// 条件是否成立(条件看持有者此刻的状态)
	static bool CondHolds(const ModCond& cond, const Cultivator& holder);
	// 某种类、对某属性生效、条件成立的特性的数值加和(没有就是0)
	double Sum(MOD_KIND kind, ATTRIBUTE_TYPE attr, const Cultivator& holder) const;
	bool Any(MOD_KIND kind, ATTRIBUTE_TYPE attr, const Cultivator& holder) const;

	// 回合结束：持续回合-1，到0的移除
	void Tick();

private:
	std::vector<Modifier> mods;
};

// 解析一行特性：  mod <种类> <数值> [%] [attr=none,metal,wood,water,fire,earth,all|elements] [cond=hp<50] [dur=3] [timing=end] [src=名字]
// 种类名见ModKindName；数值后跟%表示percent；cond的stat=hp/atk/mp/agi，比较号< 或 >，阈值是占上限的百分比。解析失败返回false。
bool ParseModifier(const std::string& line, Modifier& out);
const char* ModKindName(MOD_KIND kind);

}
