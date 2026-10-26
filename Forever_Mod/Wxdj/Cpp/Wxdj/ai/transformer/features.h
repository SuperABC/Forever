#pragma once

// 通用对战模型的输入特征：招式特征(每个招式一个向量)、局面特征(双方当前状态)、出招记录特征(最近K回合)。
// 只用battle/的公开接口，训练环境(env.cpp)和以后游戏里的推理共用这一份，保证两边的特征完全一致。
// 所有数值都按"自己/对手的初始属性"归一化，所以不同阶位、不同属性规模的招式文件可以用同一个模型。

#include "action.h"
#include "cultivator.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace wxfeat {

constexpr int FM = 75;      // 每个招式的特征维数
constexpr int SD = 41;      // 局面特征维数
constexpr int HK = 8;       // 记录最近多少回合
constexpr int HD = 59;      // 每回合记录的特征维数

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
