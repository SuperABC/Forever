#include "action.h"


using namespace std;

Effect::Effect(EFFECT_TYPE t, bool k) : type(t), keep(k) {

}

Effect::~Effect() {

}

EFFECT_TYPE Effect::GetType() {
    return type;
}

bool Effect::NeedKeep() {
    return keep;
}

void Effect::SetMiss() {
    keep = false;
}

PhysicalEffect::PhysicalEffect() :
    Effect(EFFECT_PHYSICAL) {

}

PhysicalEffect::~PhysicalEffect() {

}

PenetrateEffect::PenetrateEffect(float none, float metal, float wood, float water, float fire, float earth, float all) :
    Effect(EFFECT_PENETRATE) {
    if (none > 0)ratios.emplace_back(make_pair(ATTRIBUTE_NONE, none));
    if (metal > 0)ratios.emplace_back(make_pair(ATTRIBUTE_METAL, metal));
    if (wood > 0)ratios.emplace_back(make_pair(ATTRIBUTE_WOOD, wood));
    if (water > 0)ratios.emplace_back(make_pair(ATTRIBUTE_WATER, water));
    if (fire > 0)ratios.emplace_back(make_pair(ATTRIBUTE_FIRE, fire));
    if (earth > 0)ratios.emplace_back(make_pair(ATTRIBUTE_EARTH, earth));
    if (all > 0)ratios.emplace_back(make_pair(ATTRIBUTE_ALL, all));
}

PenetrateEffect::~PenetrateEffect() {

}

ReduceEffect::ReduceEffect(float none, float metal, float wood, float water, float fire, float earth, float all) :
    Effect(EFFECT_REDUCE) {
    if (none > 0)ratios.emplace_back(make_pair(ATTRIBUTE_NONE, none));
    if (metal > 0)ratios.emplace_back(make_pair(ATTRIBUTE_METAL, metal));
    if (wood > 0)ratios.emplace_back(make_pair(ATTRIBUTE_WOOD, wood));
    if (water > 0)ratios.emplace_back(make_pair(ATTRIBUTE_WATER, water));
    if (fire > 0)ratios.emplace_back(make_pair(ATTRIBUTE_FIRE, fire));
    if (earth > 0)ratios.emplace_back(make_pair(ATTRIBUTE_EARTH, earth));
    if (all > 0)ratios.emplace_back(make_pair(ATTRIBUTE_ALL, all));
}

ReduceEffect::~ReduceEffect() {

}

ReboundEffect::ReboundEffect(float none, float metal, float wood, float water, float fire, float earth, float all) :
    Effect(EFFECT_REBOUND) {
    if (none > 0)ratios.emplace_back(make_pair(ATTRIBUTE_NONE, none));
    if (metal > 0)ratios.emplace_back(make_pair(ATTRIBUTE_METAL, metal));
    if (wood > 0)ratios.emplace_back(make_pair(ATTRIBUTE_WOOD, wood));
    if (water > 0)ratios.emplace_back(make_pair(ATTRIBUTE_WATER, water));
    if (fire > 0)ratios.emplace_back(make_pair(ATTRIBUTE_FIRE, fire));
    if (earth > 0)ratios.emplace_back(make_pair(ATTRIBUTE_EARTH, earth));
    if (all > 0)ratios.emplace_back(make_pair(ATTRIBUTE_ALL, all));
}

ReboundEffect::~ReboundEffect() {

}

AbsorbEffect::AbsorbEffect(float none, float metal, float wood, float water, float fire, float earth, float all) :
    Effect(EFFECT_ABSORB) {
    if (none > 0)ratios.emplace_back(make_pair(ATTRIBUTE_NONE, none));
    if (metal > 0)ratios.emplace_back(make_pair(ATTRIBUTE_METAL, metal));
    if (wood > 0)ratios.emplace_back(make_pair(ATTRIBUTE_WOOD, wood));
    if (water > 0)ratios.emplace_back(make_pair(ATTRIBUTE_WATER, water));
    if (fire > 0)ratios.emplace_back(make_pair(ATTRIBUTE_FIRE, fire));
    if (earth > 0)ratios.emplace_back(make_pair(ATTRIBUTE_EARTH, earth));
    if (all > 0)ratios.emplace_back(make_pair(ATTRIBUTE_ALL, all));
}

AbsorbEffect::~AbsorbEffect() {

}

LockEffect::LockEffect(float prob) :
    Effect(EFFECT_LOCK, true), prob(prob) {

}

LockEffect::~LockEffect() {

}

DotEffect::DotEffect(int instant, int continuous, int rounds, int interval) :
    instantDot(instant), continuousDot(continuous), dotRounds(rounds), dotInterval(interval),
    Effect(EFFECT_DOT, true) {
    currentRound = 0;
}

DotEffect::~DotEffect() {

}

bool DotEffect::NeedHit() {
    return hitNecessary;
}

int DotEffect::DotInstant() {
    return instantDot;
}

int DotEffect::DotContinuous() {
    return continuousDot;
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

bool DotEffect::UseUp() {
    return dotRounds <= 0;
}

RebateEffect::RebateEffect(int instant, int continuous, int rounds, int interval) :
    instantRebate(instant), continuousRebate(continuous), rebateRounds(rounds), rebateInterval(interval),
    Effect(EFFECT_REBATE, true) {
    currentRound = 0;
}

RebateEffect::~RebateEffect() {

}

bool RebateEffect::NeedHit() {
    return hitNecessary;
}

int RebateEffect::RebateInstant() {
    return instantRebate;
}

int RebateEffect::RebateContinuous() {
    return continuousRebate;
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

bool RebateEffect::UseUp() {
    return rebateRounds <= 0;
}

RecoverEffect::RecoverEffect(int instant, int continuous, int rounds, int interval) :
    instantRecover(instant), continuousRecover(continuous), recoverRounds(rounds), recoverInterval(interval),
    Effect(EFFECT_RECOVER, true) {
    currentRound = 0;
}

RecoverEffect::~RecoverEffect() {

}

bool RecoverEffect::NeedHit() {
    return hitNecessary;
}

int RecoverEffect::RecoverInstant() {
    return instantRecover;
}

int RecoverEffect::RecoverContinuous() {
    return continuousRecover;
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

bool RecoverEffect::UseUp() {
    return recoverRounds <= 0;
}

CureEffect::CureEffect(int instant, int continuous, int rounds, int interval) :
    instantCure(instant), continuousCure(continuous), cureRounds(rounds), cureInterval(interval),
    Effect(EFFECT_CURE, true) {
    currentRound = 0;
}

CureEffect::~CureEffect() {

}

bool CureEffect::NeedHit() {
    return hitNecessary;
}

int CureEffect::CureInstant() {
    return instantCure;
}

int CureEffect::CureContinuous() {
    return continuousCure;
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

bool CureEffect::UseUp() {
    return cureRounds <= 0;
}

BurnEffect::BurnEffect() : Effect(EFFECT_BURN) {
    burnRounds = 3;
    currentRound = 0;
}

BurnEffect::~BurnEffect() {

}

void BurnEffect::PassRound() {
    currentRound++;
}

bool BurnEffect::UseUp() {
    return currentRound >= burnRounds;
}

RustEffect::RustEffect() : Effect(EFFECT_RUST) {
    burnRounds = 3;
    currentRound = 0;
}

RustEffect::~RustEffect() {

}

void RustEffect::PassRound() {
    currentRound++;
}

bool RustEffect::UseUp() {
    return currentRound >= burnRounds;
}

DustEffect::DustEffect() : Effect(EFFECT_DUST) {
    burnRounds = 3;
    currentRound = 0;
}

DustEffect::~DustEffect() {

}

void DustEffect::PassRound() {
    currentRound++;
}

bool DustEffect::UseUp() {
    return currentRound >= burnRounds;
}

MudEffect::MudEffect() : Effect(EFFECT_MUD) {
    burnRounds = 3;
    currentRound = 0;
}

MudEffect::~MudEffect() {

}

void MudEffect::PassRound() {
    currentRound++;
}

bool MudEffect::UseUp() {
    return currentRound >= burnRounds;
}

Action::Action(ACTION_TYPE type, ATTRIBUTE_TYPE attribute, int point, int power, vector<shared_ptr<Effect>> effects)
    : type(type), attribute(attribute), point(point), power(power), effects(effects) {
}

Action::~Action() {

}

ACTION_TYPE Action::GetType() const {
    return type;
}
ATTRIBUTE_TYPE Action::GetAttribute() const {
    return attribute;
}
int Action::GetPoint() const {
    return point;
}
int Action::GetPower() const {
    return power;
}

bool Action::Validate(int power) const {
    return this->power >= power;
}

shared_ptr<Effect> Action::GetEffect(EFFECT_TYPE type) {
    for (auto effect : effects) {
        if (effect->GetType() == type)return effect;
    }

    return nullptr;
}

vector<shared_ptr<Effect>>& Action::GetEffects() {
    return effects;
}

AccumulateAction::AccumulateAction() : Action(ACTION_ACCUMULATE, ATTRIBUTE_NONE, 0, 0, {}) {

}

BombAction::BombAction() : Action(ACTION_BOMB, ATTRIBUTE_NONE, 0, 0, {}) {
    effects.push_back(make_shared<PhysicalEffect>());
}

void BombAction::SetPower(int power) {
    this->power = power;
}

SkipAction::SkipAction() : Action(ACTION_SKIP, ATTRIBUTE_NONE, 0, 0, {}) {

}

SingleAction::SingleAction(const string& name, ATTRIBUTE_TYPE attribute, int point, int power, Realm realm,
    vector<shared_ptr<Effect>> effects)
    : name(name), realm(realm), Action(ACTION_SINGLE, attribute, point, power, effects) {
}

SingleAction::SingleAction(const SingleAction& action) :
    Action(action.type, action.attribute, action.point, action.power, {}) {
    name = action.name;
    realm = action.realm;

    for (auto effect : action.effects) {
        switch (effect->GetType())
        {
        case EFFECT_PHYSICAL:
            effects.push_back(make_shared<PhysicalEffect>(*dynamic_cast<PhysicalEffect*>(effect.get())));
            break;
        case EFFECT_PENETRATE:
            effects.push_back(make_shared<PenetrateEffect>(*dynamic_cast<PenetrateEffect*>(effect.get())));
            break;
        case EFFECT_REDUCE:
            effects.push_back(make_shared<ReduceEffect>(*dynamic_cast<ReduceEffect*>(effect.get())));
            break;
        case EFFECT_REBOUND:
            effects.push_back(make_shared<ReboundEffect>(*dynamic_cast<ReboundEffect*>(effect.get())));
            break;
        case EFFECT_ABSORB:
            effects.push_back(make_shared<AbsorbEffect>(*dynamic_cast<AbsorbEffect*>(effect.get())));
            break;
        case EFFECT_LOCK:
            effects.push_back(make_shared<LockEffect>(*dynamic_cast<LockEffect*>(effect.get())));
            break;
        case EFFECT_DOT:
            effects.push_back(make_shared<DotEffect>(*dynamic_cast<DotEffect*>(effect.get())));
            break;
        case EFFECT_REBATE:
            effects.push_back(make_shared<RebateEffect>(*dynamic_cast<RebateEffect*>(effect.get())));
            break;
        case EFFECT_RECOVER:
            effects.push_back(make_shared<RecoverEffect>(*dynamic_cast<RecoverEffect*>(effect.get())));
            break;
        case EFFECT_CURE:
            effects.push_back(make_shared<CureEffect>(*dynamic_cast<CureEffect*>(effect.get())));
            break;
        default:
            break;
        }
    }
}

string SingleAction::GetName() const {
    return name;
}
Realm SingleAction::GetRealm() const {
    return realm;
}
vector<shared_ptr<Effect>>& SingleAction::GetEffects() {
    return effects;
}

DualAction::DualAction(shared_ptr<SingleAction> action1, shared_ptr<SingleAction> action2)
    : action1(action1), action2(action2),
    Action(ACTION_DUAL, DUAL_ATTRIBUTE_PRIMARY.at({ action1->GetAttribute(), action2->GetAttribute() }),
        static_cast<int>(0.75 * (action1->GetPoint() + action2->GetPoint())),
        static_cast<int>(1.2 * (action1->GetPower() + action2->GetPower())),
        {}
    ) {
    effects.insert(effects.end(), action1->effects.begin(), action1->effects.end());
    effects.insert(effects.end(), action2->effects.begin(), action2->effects.end());
}

DualAction::~DualAction() {

}

shared_ptr<SingleAction> DualAction::GetAction1() {
    return action1;
}
shared_ptr<SingleAction> DualAction::GetAction2() {
    return action2;
}