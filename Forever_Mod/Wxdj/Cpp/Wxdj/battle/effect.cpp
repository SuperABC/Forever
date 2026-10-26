#include "effect.h"


using namespace std;

namespace wxdj {

Effect::Effect(EFFECT_TYPE t, bool k) : type(t), keep(k) {

}

Effect::~Effect() {

}

EFFECT_TYPE Effect::GetType() const {
	return type;
}

bool Effect::NeedKeep() const {
	return keep;
}

void Effect::SetMiss() {
	keep = false;
}

PhysicalEffect::PhysicalEffect() : Effect(EFFECT_PHYSICAL) {
	for (int attribute = ATTRIBUTE_NONE; attribute <= ATTRIBUTE_ALL; ++attribute) {
		ratios[static_cast<ATTRIBUTE_TYPE>(attribute)] = 1.0f;
	}
}

PhysicalEffect::PhysicalEffect(float none, float metal, float wood, float water, float fire, float earth, float all) :
	Effect(EFFECT_PHYSICAL) {
	if (none > 0) ratios[ATTRIBUTE_NONE] = none;
	if (metal > 0) ratios[ATTRIBUTE_METAL] = metal;
	if (wood > 0) ratios[ATTRIBUTE_WOOD] = wood;
	if (water > 0) ratios[ATTRIBUTE_WATER] = water;
	if (fire > 0) ratios[ATTRIBUTE_FIRE] = fire;
	if (earth > 0) ratios[ATTRIBUTE_EARTH] = earth;
	if (all > 0) ratios[ATTRIBUTE_ALL] = all;
}

float PhysicalEffect::GetRatio(ATTRIBUTE_TYPE attribute) const {
	auto it = ratios.find(attribute);
	return (it != ratios.end()) ? it->second : 0.0f;
}

PhysicalEffect::~PhysicalEffect() {

}

PenetrateEffect::PenetrateEffect(float none, float metal, float wood, float water, float fire, float earth, float all) :
	Effect(EFFECT_PENETRATE) {
	if (none > 0) ratios[ATTRIBUTE_NONE] = none;
	if (metal > 0) ratios[ATTRIBUTE_METAL] = metal;
	if (wood > 0) ratios[ATTRIBUTE_WOOD] = wood;
	if (water > 0) ratios[ATTRIBUTE_WATER] = water;
	if (fire > 0) ratios[ATTRIBUTE_FIRE] = fire;
	if (earth > 0) ratios[ATTRIBUTE_EARTH] = earth;
	if (all > 0) ratios[ATTRIBUTE_ALL] = all;
}

PenetrateEffect::~PenetrateEffect() {

}

float PenetrateEffect::GetRatio(ATTRIBUTE_TYPE attribute) const {
	auto it = ratios.find(attribute);
	return (it != ratios.end()) ? it->second : 0.0f;
}

ReduceEffect::ReduceEffect(float none, float metal, float wood, float water, float fire, float earth, float all) :
	Effect(EFFECT_REDUCE) {
	if (none > 0) ratios[ATTRIBUTE_NONE] = none;
	if (metal > 0) ratios[ATTRIBUTE_METAL] = metal;
	if (wood > 0) ratios[ATTRIBUTE_WOOD] = wood;
	if (water > 0) ratios[ATTRIBUTE_WATER] = water;
	if (fire > 0) ratios[ATTRIBUTE_FIRE] = fire;
	if (earth > 0) ratios[ATTRIBUTE_EARTH] = earth;
	if (all > 0) ratios[ATTRIBUTE_ALL] = all;
}

ReduceEffect::~ReduceEffect() {

}

float ReduceEffect::GetRatio(ATTRIBUTE_TYPE attribute) const {
	auto it = ratios.find(attribute);
	return (it != ratios.end()) ? it->second : 0.0f;
}

ReboundEffect::ReboundEffect(float none, float metal, float wood, float water, float fire, float earth, float all) :
	Effect(EFFECT_REBOUND) {
	if (none > 0) ratios[ATTRIBUTE_NONE] = none;
	if (metal > 0) ratios[ATTRIBUTE_METAL] = metal;
	if (wood > 0) ratios[ATTRIBUTE_WOOD] = wood;
	if (water > 0) ratios[ATTRIBUTE_WATER] = water;
	if (fire > 0) ratios[ATTRIBUTE_FIRE] = fire;
	if (earth > 0) ratios[ATTRIBUTE_EARTH] = earth;
	if (all > 0) ratios[ATTRIBUTE_ALL] = all;
}

ReboundEffect::~ReboundEffect() {

}

float ReboundEffect::GetRatio(ATTRIBUTE_TYPE attribute) const {
	auto it = ratios.find(attribute);
	return (it != ratios.end()) ? it->second : 0.0f;
}

AbsorbEffect::AbsorbEffect(float none, float metal, float wood, float water, float fire, float earth, float all) :
	Effect(EFFECT_ABSORB) {
	if (none > 0) ratios[ATTRIBUTE_NONE] = none;
	if (metal > 0) ratios[ATTRIBUTE_METAL] = metal;
	if (wood > 0) ratios[ATTRIBUTE_WOOD] = wood;
	if (water > 0) ratios[ATTRIBUTE_WATER] = water;
	if (fire > 0) ratios[ATTRIBUTE_FIRE] = fire;
	if (earth > 0) ratios[ATTRIBUTE_EARTH] = earth;
	if (all > 0) ratios[ATTRIBUTE_ALL] = all;
}

AbsorbEffect::~AbsorbEffect() {

}

float AbsorbEffect::GetRatio(ATTRIBUTE_TYPE attribute) const {
	auto it = ratios.find(attribute);
	return (it != ratios.end()) ? it->second : 0.0f;
}

LockEffect::LockEffect(float prob) : Effect(EFFECT_LOCK, true), prob(prob) {

}

LockEffect::~LockEffect() {

}

float LockEffect::GetProb() const {
	return prob;
}

DotEffect::DotEffect(int instant, int continuous, int rounds, int interval) :
	Effect(EFFECT_DOT, true), instantDot(instant), continuousDot(continuous),
	dotRounds(rounds), dotInterval(interval), hitNecessary(true), currentRound(0) {

}

DotEffect::~DotEffect() {

}

bool DotEffect::NeedHit() const {
	return hitNecessary;
}

int DotEffect::DotInstant() const {
	return instantDot;
}

int DotEffect::DotContinuous() const {
	return continuousDot;
}

int DotEffect::DotRounds() const {
	return dotRounds;
}

int DotEffect::DotInterval() const {
	return dotInterval;
}

int DotEffect::DotOnce() {
	currentRound++;
	if (currentRound >= dotInterval && dotRounds > 0) {
		dotRounds--;
		currentRound = 0;
		return continuousDot;
	}
	return 0;
}

bool DotEffect::UseUp() const {
	return dotRounds <= 0;
}

RebateEffect::RebateEffect(int instant, int continuous, int rounds, int interval) :
	Effect(EFFECT_REBATE, true), instantRebate(instant), continuousRebate(continuous),
	rebateRounds(rounds), rebateInterval(interval), hitNecessary(false), currentRound(0) {

}

RebateEffect::~RebateEffect() {

}

bool RebateEffect::NeedHit() const {
	return hitNecessary;
}

int RebateEffect::RebateInstant() const {
	return instantRebate;
}

int RebateEffect::RebateContinuous() const {
	return continuousRebate;
}

int RebateEffect::RebateRounds() const {
	return rebateRounds;
}

int RebateEffect::RebateInterval() const {
	return rebateInterval;
}

int RebateEffect::RebateOnce() {
	currentRound++;
	if (currentRound >= rebateInterval && rebateRounds > 0) {
		rebateRounds--;
		currentRound = 0;
		return continuousRebate;
	}
	return 0;
}

bool RebateEffect::UseUp() const {
	return rebateRounds <= 0;
}

RecoverEffect::RecoverEffect(int instant, int continuous, int rounds, int interval) :
	Effect(EFFECT_RECOVER, true), instantRecover(instant), continuousRecover(continuous),
	recoverRounds(rounds), recoverInterval(interval), hitNecessary(false), currentRound(0) {

}

RecoverEffect::~RecoverEffect() {

}

bool RecoverEffect::NeedHit() const {
	return hitNecessary;
}

int RecoverEffect::RecoverInstant() const {
	return instantRecover;
}

int RecoverEffect::RecoverContinuous() const {
	return continuousRecover;
}

int RecoverEffect::RecoverRounds() const {
	return recoverRounds;
}

int RecoverEffect::RecoverInterval() const {
	return recoverInterval;
}

int RecoverEffect::RecoverOnce() {
	currentRound++;
	if (currentRound >= recoverInterval && recoverRounds > 0) {
		recoverRounds--;
		currentRound = 0;
		return continuousRecover;
	}
	return 0;
}

bool RecoverEffect::UseUp() const {
	return recoverRounds <= 0;
}

CureEffect::CureEffect(int instant, int continuous, int rounds, int interval) :
	Effect(EFFECT_CURE, true), instantCure(instant), continuousCure(continuous),
	cureRounds(rounds), cureInterval(interval), hitNecessary(false), currentRound(0) {

}

CureEffect::~CureEffect() {

}

bool CureEffect::NeedHit() const {
	return hitNecessary;
}

int CureEffect::CureInstant() const {
	return instantCure;
}

int CureEffect::CureContinuous() const {
	return continuousCure;
}

int CureEffect::CureRounds() const {
	return cureRounds;
}

int CureEffect::CureInterval() const {
	return cureInterval;
}

int CureEffect::CureOnce() {
	currentRound++;
	if (currentRound >= cureInterval && cureRounds > 0) {
		cureRounds--;
		currentRound = 0;
		return continuousCure;
	}
	return 0;
}

bool CureEffect::UseUp() const {
	return cureRounds <= 0;
}

IncreaseEffect::IncreaseEffect(float ratio) : Effect(EFFECT_INCREASE), ratio(ratio) {

}

IncreaseEffect::~IncreaseEffect() {

}

float IncreaseEffect::GetRatio() const {
	return ratio;
}

ShieldEffect::ShieldEffect(int strength, int rounds) :
	Effect(EFFECT_SHIELD), strength(strength), initial(strength), rounds(rounds) {

}

ShieldEffect::~ShieldEffect() {

}

int ShieldEffect::Strength() const {
	return strength;
}

int ShieldEffect::RemainingRounds() const {
	return rounds;
}

int ShieldEffect::Initial() const {
	return initial;
}

void ShieldEffect::Reduce(int amount) {
	strength -= amount;
	if (strength < 0) strength = 0;
}

void ShieldEffect::PassRound() {
	--rounds;
}

bool ShieldEffect::UseUp() const {
	return rounds <= 0 || strength <= 0;
}

BurnEffect::BurnEffect() : Effect(EFFECT_BURN), burnRounds(3), currentRound(0) {

}

BurnEffect::~BurnEffect() {

}

void BurnEffect::PassRound() {
	currentRound++;
}

int BurnEffect::RemainingRounds() const {
	return burnRounds - currentRound;
}

bool BurnEffect::UseUp() const {
	return currentRound >= burnRounds;
}

RustEffect::RustEffect() : Effect(EFFECT_RUST), rustRounds(3), currentRound(0) {

}

RustEffect::~RustEffect() {

}

void RustEffect::PassRound() {
	currentRound++;
}

int RustEffect::RemainingRounds() const {
	return rustRounds - currentRound;
}

bool RustEffect::UseUp() const {
	return currentRound >= rustRounds;
}

DustEffect::DustEffect() : Effect(EFFECT_DUST), dustRounds(3), currentRound(0) {

}

DustEffect::~DustEffect() {

}

void DustEffect::PassRound() {
	currentRound++;
}

int DustEffect::RemainingRounds() const {
	return dustRounds - currentRound;
}

bool DustEffect::UseUp() const {
	return currentRound >= dustRounds;
}

MudEffect::MudEffect() : Effect(EFFECT_MUD), mudRounds(3), currentRound(0) {

}

MudEffect::~MudEffect() {

}

void MudEffect::PassRound() {
	currentRound++;
}

int MudEffect::RemainingRounds() const {
	return mudRounds - currentRound;
}

bool MudEffect::UseUp() const {
	return currentRound >= mudRounds;
}

}
