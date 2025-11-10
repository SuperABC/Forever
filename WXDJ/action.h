#pragma once

#include "def.h"


class Effect {
private:
    EFFECT_TYPE type;
    bool keep;

public:
    Effect(EFFECT_TYPE t, bool k = false);
    virtual ~Effect();

    EFFECT_TYPE GetType();
    bool NeedKeep();
    void SetMiss();
};

class PhysicalEffect : public Effect {
private:

public:
    PhysicalEffect();
    virtual ~PhysicalEffect();
};

class PenetrateEffect : public Effect {
private:
    std::vector<std::pair<ATTRIBUTE_TYPE, float>> ratios;

public:
    PenetrateEffect(float none, float metal, float wood, float water, float fire, float earth, float all);
    virtual ~PenetrateEffect();

    float GetRatio(ATTRIBUTE_TYPE attribute) {
        for (auto ratio : ratios) {
            if (ratio.first == attribute)return ratio.second;
        }

        return 0.0f;
    }
};

class ReduceEffect : public Effect {
private:
    std::vector<std::pair<ATTRIBUTE_TYPE, float>> ratios;

public:
    ReduceEffect(float none, float metal, float wood, float water, float fire, float earth, float all);
    virtual ~ReduceEffect();

    float GetRatio(ATTRIBUTE_TYPE attribute) {
        for (auto ratio : ratios) {
            if (ratio.first == attribute)return ratio.second;
        }

        return 0.0f;
    }
};

class ReboundEffect : public Effect {
private:
    std::vector<std::pair<ATTRIBUTE_TYPE, float>> ratios;

public:
    ReboundEffect(float none, float metal, float wood, float water, float fire, float earth, float all);
    virtual ~ReboundEffect();

    float GetRatio(ATTRIBUTE_TYPE attribute) {
        for (auto ratio : ratios) {
            if (ratio.first == attribute)return ratio.second;
        }

        return 0.0f;
    }
};

class AbsorbEffect : public Effect {
private:
    std::vector<std::pair<ATTRIBUTE_TYPE, float>> ratios;

public:
    AbsorbEffect(float none, float metal, float wood, float water, float fire, float earth, float all);
    virtual ~AbsorbEffect();

    float GetRatio(ATTRIBUTE_TYPE attribute) {
        for (auto ratio : ratios) {
            if (ratio.first == attribute)return ratio.second;
        }

        return 0.0f;
    }
};

class LockEffect : public Effect {
private:
    float prob;

public:
    LockEffect(float prob);
    virtual ~LockEffect();

    float GetProb() {
        return prob;
    }
};

class DotEffect : public Effect {
private:
    int instantDot;
    int continuousDot;
    int dotRounds;
    int dotInterval;
    bool hitNecessary = false;

    int currentRound;

public:
    DotEffect(int instant, int continuous = 0, int rounds = 0, int interval = 1);
    virtual ~DotEffect();

    bool NeedHit();
    int DotInstant();
    int DotContinuous();
    int DotOnce();
    bool UseUp();
};

class RebateEffect : public Effect {
private:
    int instantRebate;
    int continuousRebate;
    int rebateRounds;
    int rebateInterval;
    bool hitNecessary = false;

    int currentRound;

public:
    RebateEffect(int instant, int continuous = 0, int rounds = 0, int interval = 1);
    virtual ~RebateEffect();

    bool NeedHit();
    int RebateInstant();
    int RebateContinuous();
    int RebateOnce();
    bool UseUp();
};

class RecoverEffect : public Effect {
private:
    int instantRecover;
    int continuousRecover;
    int recoverRounds;
    int recoverInterval;
    bool hitNecessary = false;

    int currentRound;

public:
    RecoverEffect(int instant, int continuous = 0, int rounds = 0, int interval = 1);
    virtual ~RecoverEffect();

    bool NeedHit();
    int RecoverInstant();
    int RecoverContinuous();
    int RecoverOnce();
    bool UseUp();
};

class CureEffect : public Effect {
private:
    int instantCure;
    int continuousCure;
    int cureRounds;
    int cureInterval;
    bool hitNecessary = false;

    int currentRound;

public:
    CureEffect(int instant, int continuous = 0, int rounds = 0, int interval = 1);
    virtual ~CureEffect();

    bool NeedHit();
    int CureInstant();
    int CureContinuous();
    int CureOnce();
    bool UseUp();
};

class BurnEffect : public Effect {
private:
    int burnRounds;
    int currentRound;

public:
    BurnEffect();
    virtual ~BurnEffect();

    void PassRound();
    bool UseUp();
};

class RustEffect : public Effect {
private:
    int burnRounds;
    int currentRound;

public:
    RustEffect();
    virtual ~RustEffect();

    void PassRound();
    bool UseUp();
};

class DustEffect : public Effect {
private:
    int burnRounds;
    int currentRound;

public:
    DustEffect();
    virtual ~DustEffect();

    void PassRound();
    bool UseUp();
};

class MudEffect : public Effect {
private:
    int burnRounds;
    int currentRound;

public:
    MudEffect();
    virtual ~MudEffect();

    void PassRound();
    bool UseUp();
};

class Action {
protected:
    ACTION_TYPE type;
    ATTRIBUTE_TYPE attribute;
    int point;
    int power;

    std::vector<std::shared_ptr<Effect>> effects;

public:
    Action(ACTION_TYPE type, ATTRIBUTE_TYPE attribute, int point, int power, std::vector<std::shared_ptr<Effect>> effects);
    virtual ~Action();

    ACTION_TYPE GetType() const;
    ATTRIBUTE_TYPE GetAttribute() const;
    int GetPoint() const;
    int GetPower() const;
    virtual bool Validate(int power) const;

    std::shared_ptr<Effect> GetEffect(EFFECT_TYPE type);
    std::vector<std::shared_ptr<Effect>>& GetEffects();

    virtual std::string GetText() = 0;
};

class SkipAction : public Action {
public:
    SkipAction();

    virtual std::string GetText() { return "无"; }
};

class AccumulateAction : public Action {
public:
    AccumulateAction();

    virtual std::string GetText() { return "积攒真气炸弹"; }
};

class BombAction : public Action {
public:
    BombAction();

    void SetPower(int power);

    virtual std::string GetText() { return "释放真气炸弹"; }
};

class SingleAction : public Action {
private:
    std::string name;
    Realm realm;

    friend class DualAction;

public:
    SingleAction(const std::string& name, ATTRIBUTE_TYPE attribute, int point, int power, Realm realm,
        std::vector<std::shared_ptr<Effect>> effects = {});
    SingleAction(const SingleAction& action);

    std::string GetName() const;
    Realm GetRealm() const;
    std::vector<std::shared_ptr<Effect>>& GetEffects();

    virtual std::string GetText() { return name; }
};

class DualAction : public Action {
private:
    std::shared_ptr<SingleAction> action1, action2;

public:
    DualAction(std::shared_ptr<SingleAction> action1, std::shared_ptr<SingleAction> action2);
    virtual ~DualAction();

    std::shared_ptr<SingleAction> GetAction1();
    std::shared_ptr<SingleAction> GetAction2();

    virtual std::string GetText() { return std::string("(") + action1->GetName() + ", " + action2->GetName() + ")"; }
};

