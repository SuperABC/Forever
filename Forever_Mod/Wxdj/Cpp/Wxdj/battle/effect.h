#pragma once

#include "def.h"

#include <memory>
#include <vector>


namespace wxdj {

// 效果基类
class Effect {
public:
	Effect(EFFECT_TYPE t, bool k = false);
	virtual ~Effect();

	// 获取效果类型
	EFFECT_TYPE GetType() const;

	// 是否持续
	bool NeedKeep() const;

	// 未命中并取消持续
	void SetMiss();

private:
	EFFECT_TYPE type;
	bool keep;
};

// 物理伤害效果，命中扣除生命值而非武力值
class PhysicalEffect : public Effect {
public:
	// 全部属性都100%转换成物理伤害(真气炸弹用)
	PhysicalEffect();
	// 按对方属性分别指定"有多少比例的伤害转换成物理伤害"，剩余部分仍是招式(武力)伤害
	PhysicalEffect(float none, float metal, float wood, float water, float fire, float earth, float all);
	virtual ~PhysicalEffect();

	// 对方这一招属性为attribute时，转换成物理伤害的比例
	float GetRatio(ATTRIBUTE_TYPE attribute) const;

private:
	std::unordered_map<ATTRIBUTE_TYPE, float> ratios;
};

// 穿透效果，按不同属性对应比例造成额外伤害
class PenetrateEffect : public Effect {
public:
	PenetrateEffect(float none, float metal, float wood, float water, float fire, float earth, float all);
	virtual ~PenetrateEffect();

	float GetRatio(ATTRIBUTE_TYPE attribute) const;

private:
	std::unordered_map<ATTRIBUTE_TYPE, float> ratios;
};

// 减伤效果，按不同属性对应比例减少受到的伤害
class ReduceEffect : public Effect {
public:
	ReduceEffect(float none, float metal, float wood, float water, float fire, float earth, float all);
	virtual ~ReduceEffect();

	float GetRatio(ATTRIBUTE_TYPE attribute) const;

private:
	std::unordered_map<ATTRIBUTE_TYPE, float> ratios;
};

// 反弹效果，按不同属性对应比例反弹造成的伤害给攻击者
class ReboundEffect : public Effect {
public:
	ReboundEffect(float none, float metal, float wood, float water, float fire, float earth, float all);
	virtual ~ReboundEffect();

	float GetRatio(ATTRIBUTE_TYPE attribute) const;

private:
	std::unordered_map<ATTRIBUTE_TYPE, float> ratios;
};

// 吸收真气效果，按不同属性对应比例将造成的伤害转化为回复真气
class AbsorbEffect : public Effect {
public:
	AbsorbEffect(float none, float metal, float wood, float water, float fire, float earth, float all);
	virtual ~AbsorbEffect();

	float GetRatio(ATTRIBUTE_TYPE attribute) const;

private:
	std::unordered_map<ATTRIBUTE_TYPE, float> ratios;
};

// 禁锢效果，概率使目标下一回合无法行动
class LockEffect : public Effect {
public:
	LockEffect(float prob);
	virtual ~LockEffect();

	float GetProb() const;

private:
	float prob;
};

// 持续伤害效果，命中触发后每隔固定回合造成一次伤害
class DotEffect : public Effect {
public:
	DotEffect(int instant, int continuous = 0, int rounds = 0, int interval = 1);
	virtual ~DotEffect();

	bool NeedHit() const;
	int DotInstant() const;
	int DotContinuous() const;
	int DotRounds() const;
	int DotInterval() const;
	int DotOnce();
	bool UseUp() const;

private:
	int instantDot; // 即时伤害
	int continuousDot; // 每段持续伤害
	int dotRounds; // 剩余触发次数
	int dotInterval; // 固定触发间隔的回合数
	bool hitNecessary; // 是否需要命中才触发
	int currentRound; // 从上一次触发开始已累计的回合数
};

// 回复真气效果，无论是否命中每隔固定回合回复一次真气
class RebateEffect : public Effect {
public:
	RebateEffect(int instant, int continuous = 0, int rounds = 0, int interval = 1);
	virtual ~RebateEffect();

	bool NeedHit() const;
	int RebateInstant() const;
	int RebateContinuous() const;
	int RebateRounds() const;
	int RebateInterval() const;
	int RebateOnce();
	bool UseUp() const;

private:
	int instantRebate; // 即时回复
	int continuousRebate; // 每段持续回复
	int rebateRounds; // 剩余触发次数
	int rebateInterval; // 固定触发间隔的回合数
	bool hitNecessary; // 是否需要命中才触发
	int currentRound; // 从上一次触发开始已累计的回合数
};

// 回复武力值效果，命中触发后每隔固定回合回复一次武力值
class RecoverEffect : public Effect {
public:
	RecoverEffect(int instant, int continuous = 0, int rounds = 0, int interval = 1);
	virtual ~RecoverEffect();

	bool NeedHit() const;
	int RecoverInstant() const;
	int RecoverContinuous() const;
	int RecoverRounds() const;
	int RecoverInterval() const;
	int RecoverOnce();
	bool UseUp() const;

private:
	int instantRecover; // 即时回复
	int continuousRecover; // 每段持续回复
	int recoverRounds; // 剩余触发次数
	int recoverInterval; // 固定触发间隔的回合数
	bool hitNecessary; // 是否需要命中才触发
	int currentRound; // 从上一次触发开始已累计的回合数
};

// 回复健康值效果，命中触发后每隔固定回合回复一次健康值
class CureEffect : public Effect {
public:
	CureEffect(int instant, int continuous = 0, int rounds = 0, int interval = 1);
	virtual ~CureEffect();

	bool NeedHit() const;
	int CureInstant() const;
	int CureContinuous() const;
	int CureRounds() const;
	int CureInterval() const;
	int CureOnce();
	bool UseUp() const;

private:
	int instantCure; // 即时回复
	int continuousCure; // 每段持续回复
	int cureRounds; // 剩余触发次数
	int cureInterval; // 固定触发间隔的回合数
	bool hitNecessary; // 是否需要命中才触发
	int currentRound; // 从上一次触发开始已累计的回合数
};

// 连续升式增伤：本招连着上一回合的"同家族前一式"出，每连续一步威力多加ratio(第n步 x(1+ratio*n))
class IncreaseEffect : public Effect {
public:
	IncreaseEffect(float ratio);
	virtual ~IncreaseEffect();

	float GetRatio() const;

private:
	float ratio;
};

// 护盾：出招时给自己加一块，同时保护武力和健康；多块盾同时存在、各自独立计时，
// 受到伤害时所有盾一起减去同一个伤害值，穿透量=伤害-最大的那块盾的强度
class ShieldEffect : public Effect {
public:
	ShieldEffect(int strength, int rounds);
	virtual ~ShieldEffect();

	int Strength() const;
	int RemainingRounds() const;
	int Initial() const; // 出招时的强度(显示/招式提示用)
	void Reduce(int amount); // 一起减去伤害值
	void PassRound();
	bool UseUp() const;

private:
	int strength; // 当前剩余强度
	int initial;
	int rounds; // 还剩几回合(含当前回合)
};

// 金火灼烧场地效果
class BurnEffect : public Effect {
public:
	BurnEffect();
	virtual ~BurnEffect();

	void PassRound();
	bool UseUp() const;
	// 还剩几回合
	int RemainingRounds() const;

private:
	int burnRounds; // 总共持续回合数
	int currentRound; // 当前已生效回合数
};

// 金水锈蚀场地效果
class RustEffect : public Effect {
public:
	RustEffect();
	virtual ~RustEffect();

	void PassRound();
	bool UseUp() const;
	// 还剩几回合
	int RemainingRounds() const;

private:
	int rustRounds; // 总共持续回合数
	int currentRound; // 当前已生效回合数
};

// 土火扬尘场地效果
class DustEffect : public Effect {
public:
	DustEffect();
	virtual ~DustEffect();

	void PassRound();
	bool UseUp() const;
	// 还剩几回合
	int RemainingRounds() const;

private:
	int dustRounds; // 总共持续回合数
	int currentRound; // 当前已生效回合数
};

// 土水泥沼场地效果
class MudEffect : public Effect {
public:
	MudEffect();
	virtual ~MudEffect();

	void PassRound();
	bool UseUp() const;
	// 还剩几回合
	int RemainingRounds() const;

private:
	int mudRounds; // 总共持续回合数
	int currentRound; // 当前已生效回合数
};

}
