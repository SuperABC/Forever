#pragma once

// 通用对战模型的输入特征：招式特征(每个招式一个向量)、局面特征(双方当前状态)、出招记录特征(最近K回合)。
// 只用battle/的公开接口，训练环境(env.cpp)和以后游戏里的推理共用这一份，保证两边的特征完全一致。
// 所有数值都按"自己/对手的初始属性"归一化，所以不同阶位、不同属性规模的招式文件可以用同一个模型。

#include "action.h"
#include "controller.h"
#include "cultivator.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace wxfeat {

constexpr int FM = 83;      // 每个招式的特征维数(75~80是increase/shield/式号，81~82是static，追加在原来的75维后面)
constexpr int SD = 49;      // 局面特征维数(最后8维是双方的护盾/连招，追加在原来的41维后面)
constexpr int HK = 8;       // 记录最近多少回合
constexpr int HD = 59;      // 每回合记录的特征维数

// PvE追加的特征(玩家五行属性/抗性/暴击/特性)：PvP模型的维数FM/SD不变，PvE模型在它们后面再追加FX/SX维，全部以"没有这些东西=0"为基线，
// 所以PvP的对局(没有五行/暴击/特性)里这些维度恒为0，PvP权重补零列后输出不变。只在PvE模式的训练环境和PvE模型的推理里计算。
constexpr int FX = 5;       // 每个招式追加的维数
constexpr int SX = 34;      // 局面追加的维数(双方各17)
constexpr int FM_PVE = FM + FX;
constexpr int SD_PVE = SD + SX;

inline float Clamp(float v) { return std::max(-10.f, std::min(10.f, v)); }

// 一个招式的特征。self=出这招的人，opp=对手(用来衡量这招对对手有多大)。
inline void MoveFeatures(wxdj::SingleAction& a, const wxdj::Cultivator& self, const wxdj::Cultivator& opp, float* o) {
	using namespace wxdj;
	std::memset(o, 0, sizeof(float) * FM);
	float mp = static_cast<float>(a.GetPoint()), power = static_cast<float>(a.GetPower());
	float sHP = static_cast<float>(std::max(1, self.GetMaxHP())), sATK = static_cast<float>(std::max(1, self.GetMaxATK()));
	float sMP = static_cast<float>(std::max(1, self.GetMaxMP()));
	float oHP = static_cast<float>(std::max(1, opp.GetMaxHP())), oATK = static_cast<float>(std::max(1, opp.GetMaxATK()));
	int i = 0;
	o[i++] = Clamp(mp / sMP);
	o[i++] = std::log1p(mp) / 10.f;
	o[i++] = Clamp(power / sATK);
	o[i++] = std::log1p(power) / 10.f;
	o[i++] = Clamp(power / oHP);
	o[i++] = Clamp(power / oATK);
	int attr = static_cast<int>(a.GetAttribute());
	if (attr >= 0 && attr < 7) o[i + attr] = 1.f;
	i += 7;

	auto ratios = [&](EFFECT_TYPE type, int flag, auto&& get) {
		if (auto e = a.GetEffect(type)) {
			for (int k = 0; k < 7; ++k) o[i + k] = get(e, static_cast<ATTRIBUTE_TYPE>(k));
			o[65 + flag] = 1.f;
		}
		i += 7;
	};
	ratios(EFFECT_PENETRATE, 1, [](std::shared_ptr<Effect> e, ATTRIBUTE_TYPE t) { return static_cast<PenetrateEffect*>(e.get())->GetRatio(t); });
	ratios(EFFECT_PHYSICAL, 0, [](std::shared_ptr<Effect> e, ATTRIBUTE_TYPE t) { return static_cast<PhysicalEffect*>(e.get())->GetRatio(t); });
	ratios(EFFECT_REDUCE, 2, [](std::shared_ptr<Effect> e, ATTRIBUTE_TYPE t) { return static_cast<ReduceEffect*>(e.get())->GetRatio(t); });
	ratios(EFFECT_REBOUND, 3, [](std::shared_ptr<Effect> e, ATTRIBUTE_TYPE t) { return static_cast<ReboundEffect*>(e.get())->GetRatio(t); });
	ratios(EFFECT_ABSORB, 4, [](std::shared_ptr<Effect> e, ATTRIBUTE_TYPE t) { return static_cast<AbsorbEffect*>(e.get())->GetRatio(t); });
	if (auto e = a.GetEffect(EFFECT_LOCK)) {
		o[i] = static_cast<LockEffect*>(e.get())->GetProb();
		o[65 + 5] = 1.f;
	}
	i += 1;
	if (auto e = a.GetEffect(EFFECT_DOT)) {
		auto* d = static_cast<DotEffect*>(e.get());
		o[i] = Clamp(d->DotInstant() / oATK); o[i + 1] = Clamp(d->DotContinuous() / oATK);
		o[i + 2] = d->DotRounds() / 10.f; o[i + 3] = d->DotInterval() / 5.f;
		o[65 + 6] = 1.f;
	}
	i += 4;
	if (auto e = a.GetEffect(EFFECT_REBATE)) {
		auto* d = static_cast<RebateEffect*>(e.get());
		o[i] = Clamp(d->RebateInstant() / sMP); o[i + 1] = Clamp(d->RebateContinuous() / sMP);
		o[i + 2] = d->RebateRounds() / 10.f; o[i + 3] = d->RebateInterval() / 5.f;
		o[65 + 7] = 1.f;
	}
	i += 4;
	if (auto e = a.GetEffect(EFFECT_RECOVER)) {
		auto* d = static_cast<RecoverEffect*>(e.get());
		o[i] = Clamp(d->RecoverInstant() / sATK); o[i + 1] = Clamp(d->RecoverContinuous() / sATK);
		o[i + 2] = d->RecoverRounds() / 10.f; o[i + 3] = d->RecoverInterval() / 5.f;
		o[65 + 8] = 1.f;
	}
	i += 4;
	if (auto e = a.GetEffect(EFFECT_CURE)) {
		auto* d = static_cast<CureEffect*>(e.get());
		o[i] = Clamp(d->CureInstant() / sHP); o[i + 1] = Clamp(d->CureContinuous() / sHP);
		o[i + 2] = d->CureRounds() / 10.f; o[i + 3] = d->CureInterval() / 5.f;
		o[65 + 9] = 1.f;
	}
	// 追加的维度(旧模型对它们的权重补零，所以旧模型行为不变，重训之后才会用到)
	if (auto e = a.GetEffect(EFFECT_INCREASE)) {
		o[75] = 1.f;
		o[76] = Clamp(static_cast<IncreaseEffect*>(e.get())->GetRatio());
	}
	if (auto e = a.GetEffect(EFFECT_SHIELD)) {
		auto* s = static_cast<ShieldEffect*>(e.get());
		o[77] = 1.f;
		o[78] = Clamp(s->Ratio());   // 盾量占出招者武力上限的比例
		o[79] = s->RemainingRounds() / 5.f;
	}
	o[80] = FormOfName(a.GetName()) / 20.f;   // 式号，让模型能认出"接着上一式往上连"
	if (auto e = a.GetEffect(EFFECT_STATIC)) {   // 对静止敌人的增伤(克制积攒炸弹)
		o[81] = 1.f;
		o[82] = Clamp(static_cast<StaticEffect*>(e.get())->GetRatio());
	}
}

// PvE招式追加特征，o指向招式特征的第FM维(共FX维)：自己是否拥有这个招式的属性、同属性威力倍率、自己对这个属性的增伤加和、
// 对手对这个属性的伤害乘数(2/(抗性+1)-1)、对手对这个属性的易伤加和。
inline void MoveExtrasPvE(wxdj::SingleAction& a, const wxdj::Cultivator& self, const wxdj::Cultivator& opp, float* o) {
	using namespace wxdj;
	std::memset(o, 0, sizeof(float) * FX);
	ATTRIBUTE_TYPE attr = a.GetAttribute();
	if (self.HasElement(attr)) {
		o[0] = 1.f;
		o[1] = Clamp(static_cast<float>(self.ElemPowerMul(attr) / 3.0));
	}
	if (!self.Mods().All().empty()) o[2] = Clamp(static_cast<float>(self.Mods().Sum(MOD_DMG_DEALT, attr, self) / 2.0));
	double x = opp.Resist(attr);
	if (x != 1.0) o[3] = Clamp(static_cast<float>(2.0 / (x + 1.0) - 1.0));
	if (!opp.Mods().All().empty()) o[4] = Clamp(static_cast<float>(std::max(0.0, opp.Mods().Sum(MOD_DMG_TAKEN, attr, opp)) / 2.0));
}

// PvE局面追加特征，o指向局面特征的第SD维(共SX维)：先自己再对手，各17维——五行属性5位、对五个元素的抗性5维((抗性-1)/2)、
// 当前最厚那块盾的五行属性5位、暴击率、暴击伤害(没有暴击率时为0)。
inline void StateExtrasPvE(const wxdj::Controller& ctl, int seat, float* o) {
	using namespace wxdj;
	double baseRate = 0, baseDmg = 0;
	ctl.GetCritBase(baseRate, baseDmg);
	for (int k = 0; k < 2; ++k) {
		int idx = k == 0 ? seat : 1 - seat;
		auto c = ctl.GetCultivator(idx);
		float* p = o + k * 17;
		std::memset(p, 0, sizeof(float) * 17);
		for (int a = ATTRIBUTE_METAL; a <= ATTRIBUTE_EARTH; ++a) {
			p[a - 1] = (c->GetElements() & ElementBit(static_cast<ATTRIBUTE_TYPE>(a))) ? 1.f : 0.f;
			p[5 + a - 1] = Clamp(static_cast<float>((c->Resist(static_cast<ATTRIBUTE_TYPE>(a)) - 1.0) / 2.0));
		}
		int best = -1;
		for (const auto& e : ctl.GetEffectsOf(idx)) {
			if (e->GetType() != EFFECT_SHIELD) continue;
			auto* s = static_cast<ShieldEffect*>(e.get());
			if (best < 0 || s->Strength() > best) {
				best = s->Strength();
				for (int a = ATTRIBUTE_METAL; a <= ATTRIBUTE_EARTH; ++a)
					p[10 + a - 1] = (s->Mask() & ElementBit(static_cast<ATTRIBUTE_TYPE>(a))) ? 1.f : 0.f;
			}
		}
		double rate = baseRate, dmg = baseDmg;
		if (!c->Mods().All().empty()) {
			rate += c->Mods().Sum(MOD_CRIT_RATE, ATTRIBUTE_NONE, *c);
			dmg += c->Mods().Sum(MOD_CRIT_DMG, ATTRIBUTE_NONE, *c);
		}
		if (rate > 0) { p[15] = Clamp(static_cast<float>(rate)); p[16] = Clamp(static_cast<float>(std::max(0.0, dmg))); }
	}
}

// 局面特征最后8维：先自己再对手，各4维——最大护盾强度(相对武力上限)、护盾块数、连招链长、连招里最高的式号。
// o指向局面特征的第SD-8维。只用Controller/Cultivator的公开接口，训练环境和游戏里推理共用。
inline void StateExtras(const wxdj::Controller& ctl, int seat, float* o) {
	using namespace wxdj;
	for (int k = 0; k < 2; ++k) {
		int idx = k == 0 ? seat : 1 - seat;
		auto c = ctl.GetCultivator(idx);
		float matk = static_cast<float>(std::max(1, c->GetMaxATK()));
		float maxShield = 0.f;
		int count = 0;
		for (const auto& e : ctl.GetEffectsOf(idx)) {
			if (e->GetType() != EFFECT_SHIELD) continue;
			maxShield = std::max(maxShield, static_cast<float>(static_cast<ShieldEffect*>(e.get())->Strength()));
			++count;
		}
		int len = 0, form = 0;
		for (const auto& ce : c->GetChain()) { len = std::max(len, ce.len); form = std::max(form, ce.form); }
		o[k * 4 + 0] = Clamp(maxShield / matk);
		o[k * 4 + 1] = count / 3.f;
		o[k * 4 + 2] = len / 10.f;
		o[k * 4 + 3] = form / 20.f;
	}
}

// "第X式"：招式名里"第"之后的部分当作式的编号，没有"第"返回空串(=不受同式限制)。
inline std::string FormKey(const std::string& name) {
	size_t p = name.find("\xE7\xAC\xAC"); // "第"的UTF-8
	return p == std::string::npos ? std::string() : name.substr(p);
}
// 家族名：招式名去掉"第X式"。
inline std::string FamilyKey(const std::string& name) {
	size_t p = name.find("\xE7\xAC\xAC");
	return p == std::string::npos ? name : name.substr(0, p);
}

}
