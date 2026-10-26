#pragma once

#include "def.h"
#include "action.h"
#include "cultivator.h"
#include "modifier.h"

#include <vector>
#include <memory>
#include <random>
#include <string>


namespace wxdj {

enum LOG_TYPE {
	LOG_MP, // 回复真气
	LOG_ATK, // 回复武力值
	LOG_HP, // 回复健康值
	LOG_CONSUME, // 消耗真气
	LOG_SKIP, // 跳过回合
	LOG_MOVE, // 出招
	LOG_DAMAGE, // 招式命中
	LOG_DODGE, // 闪避成功
	LOG_FAIL, // 出招失败
	LOG_EFFECT, // 特殊效果
};

// 战斗日志
class Log {
public:
	// 状态变化日志
	Log(LOG_TYPE type, int player, int amount);

	// 跳过回合日志
	Log(int player);

	// 出招日志
	Log(int subject, int object, int amount, std::string name, int point);

	// 造成伤害日志
	Log(int subject, int object, ATTRIBUTE_TYPE attribute, int pointAtk, int pointHp);

	// 闪避成功成功日志
	Log(int subject, int object);

	// 带伤害即时效果日志
	Log(int subject, int object, ATTRIBUTE_TYPE attribute, int pointAtk, int pointHp, EFFECT_TYPE effect);

	// 无伤害即时效果日志
	Log(int subject, int object, int amount, EFFECT_TYPE effect);

	LOG_TYPE type;
	int subject, object;
	ATTRIBUTE_TYPE attribute;
	int pointAtk, pointHp;
	int amount;
	std::string name;
	EFFECT_TYPE effect = EFFECT_NONE;
};

// 战斗控制器
class Controller {
public:
	Controller();
	Controller(std::vector<std::shared_ptr<Cultivator>> cultivators);
	~Controller();

	// 添加参战修炼者
	void AddCultivator(std::shared_ptr<Cultivator> cultivator);

	// 获取修炼者
	std::shared_ptr<Cultivator> GetCultivator(int idx) const;

	// 重置回合状态
	void ResetRound();

	// 回合开始阶段
	void StartTurn();

	// 回合行动阶段
	void ActionTurn(std::vector<std::pair<std::shared_ptr<Action>, int>> actions);

	// 回合结算阶段
	void CheckTurn(std::vector<std::pair<std::shared_ptr<Action>, int>> actions);

	//回合结束阶段
	void EndTurn();

	// 固定随机种子(测试/回放用；默认每局随机)
	void Seed(unsigned int seed);

	// 特性(外部效果)：给持有者加一条特性；立即类(now_*、清除场地、解除禁锢)加的时候马上执行、不保存
	void AddModifier(int holder, const Modifier& modifier);
	void RemoveModifiers(int holder, const std::string& source);
	// 基础暴击率和暴击伤害(PvE对局创建时设置；默认暴击率0，所以PvP的随机数序列和以前完全一样)
	void SetCritBase(double rate, double damage);

	// 为指定修仙者添加一个持续效果
	void AddEffect(int cultivator, std::shared_ptr<Effect> effect);

	// 添加场地效果
	void AddEnvironment(std::shared_ptr<Effect> effect);
	bool AddReactionEnvironment(int reaction); // 按反应类型加场地效果，没有反应返回false
	void BuildDualEnvironment(const std::shared_ptr<Action>& act); // 双招未命中时，用它的两个基础招式的属性自己构造场地效果

	// 执行指定阶段的效果
	void ExecEffect(STAGE_TYPE stage);

	// 获取自上次调用以来的所有新日志
	std::vector<Log> GetNews();

	// 这个修炼者这一回合是否被禁锢(回合开始时由禁锢效果设置)：被禁锢时单招/双招/释放炸弹都会被跳过
	bool IsLocked(int cultivator) const;

	// 某个修炼者身上的持续效果、场地效果(只读，AI的局面特征要用)
	const std::vector<std::shared_ptr<Effect>>& GetEffectsOf(int cultivator) const;
	const std::vector<std::shared_ptr<Effect>>& GetEnvironmentEffects() const;

	// 获取当前场地效果：(效果类型, 还剩几回合)
	std::vector<std::pair<EFFECT_TYPE, int>> GetEnvironment() const;

	// 回复真气量
	void RecoverMP(int cultivator, int amount, bool log = true);

	// 消耗真气量
	void ConsumeMP(int cultivator, int amount, bool log = true);

	// 出招
	void MakeMove(int subject, int object, int amount, std::string name, int point, bool log = true);

	// 跳过回合
	void SkipMove(int cultivator, bool log = true);

	// 回复武力值
	void RecoverATK(int cultivator, int amount, bool log = true);

	// 回复生命值
	void RecoverHP(int cultivator, int amount, bool log = true);

	// 命中成功
	void HitSuccess(int subject, int object, std::shared_ptr<Action> offend, std::shared_ptr<Action> defend, bool log = true);
	
	// 命中失败
	void HitFail(int subject, bool log = true);

	// 造成纯武力值伤害
	// defendAttribute是受击一方这一招的属性(没有出招时取无属性)，物理效果按它取"转换成物理伤害的比例"
	void TakeDamage(int subject, int object,
		std::vector<std::shared_ptr<Effect>> offend,
		std::vector<std::shared_ptr<Effect>> defend,
		ATTRIBUTE_TYPE attribute, int amount, bool log = true,
		ATTRIBUTE_TYPE defendAttribute = ATTRIBUTE_NONE);

	// 造成伤害武力值/物理伤害
	void TakeDamage(int subject, int object,
		std::vector<std::shared_ptr<Effect>> offend,
		std::vector<std::shared_ptr<Effect>> defend,
		ATTRIBUTE_TYPE attribute, int atk, int hp, bool log = true);

	// 闪避成功
	void DodgeSuccess(int subject, int object, bool log = true);

	// 跳过一次的敏捷损失(见controller.cpp)
	void PaySkip(int cultivator);

private:
	std::vector<std::shared_ptr<Cultivator>> cultivators; // 参战修炼者
	std::vector<std::vector<std::shared_ptr<Effect>>> effects; // 每个修炼者身上的效果
	std::vector<std::shared_ptr<Effect>> environment; // 场地效果
	std::vector<int> status; // 状态标志位
	std::vector<int> regenMP; // 这回合开始时自然回复的真气量，空炸弹要把它撤销
	std::vector<float> skipAgi; // 这回合跳过的人：>0=跳过的闪避用这个敏捷(付得起是翻倍后的，付不起是原来的)，0=没有跳过
	int mark = 0; // 日志输出游标
	std::vector<Log> logs; // 所有日志
	std::mt19937 rng; // 随机数生成器
	double critBaseRate = 0; // 基础暴击率
	double critBaseDamage = 0.5; // 基础暴击伤害(倍率加成)

	// 算出这回合出的招(单招/双招)的有效威力：连招增伤 + 特性乘区，并更新连招链。
	// chainBonus返回连招让威力多出的百分比(0=没有)，critPercent返回暴击伤害的百分比(0=没暴击)
	void ApplyPower(int idx, int target, const std::shared_ptr<Action>& act, int& chainBonus, int& critPercent);

	// 特性：每回合类、触发类、命中/闪避乘区
	void ExecModifiers(STAGE_TYPE stage);
	void ApplyTraitDelta(int holder, MOD_KIND resourceKind, int delta, const std::string& source);
	void ApplyDamageTriggers(int subject, int object, ATTRIBUTE_TYPE attribute, int damage);
	double Vulnerability(int object, ATTRIBUTE_TYPE attribute) const;   // 易伤加和(只增不减)：和减伤同一个受到伤害乘区，在TakeDamage里结算
	double DodgeProb(int attacker, int defender, ATTRIBUTE_TYPE attribute, float defenderAGI);
	bool RollHit(int attacker, ATTRIBUTE_TYPE attribute);

	// 护盾：先过护盾再扣武力/健康，返回穿透护盾的伤害量
	int ApplyShield(int object, int amount, ATTRIBUTE_TYPE attribute);
	// 属性抗性乘数：2/(抗性+1)；抗性=100%(默认)时恰为1，调用方可跳过
	double ResistFactor(int object, ATTRIBUTE_TYPE attribute) const;

	// 检查属性克制关系
	bool CheckCounter(ATTRIBUTE_TYPE a1, ATTRIBUTE_TYPE a2);

	// 计算闪避概率
	double DodgeChance(float attackerAGI, float defenderAGI);

	// 检查两种属性碰撞产生的元素反应类型
	int CheckReaction(ATTRIBUTE_TYPE a1, ATTRIBUTE_TYPE a2);

	// 检查场地中是否存在某种反应效果
	bool FindReaction(EFFECT_TYPE reaction);

	// 根据出招属性消除相克的场地效果
	void PutoutReaction(ATTRIBUTE_TYPE attribute);
};

}
