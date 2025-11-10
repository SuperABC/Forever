#pragma once

#include "def.h"
#include "action.h"


class Player {
private:
    std::string name;
    int maxHP;
    int currentHP;
    int maxATK;
    int currentATK;
    int maxMP;
    int currentMP;
    int AGI;
    Realm currentRealm;

    std::vector<SingleAction> actions;  // 每个玩家自己的招式列表

    int recover;
    int bomb;

    // 从文件加载招式的辅助函数
    bool LoadActions(const std::string& filename);
    Realm ParseRealm(const std::string& realmStr);
    ATTRIBUTE_TYPE ParseAttribute(const std::string& attrStr);
    std::shared_ptr<Effect> ParseEffect(const std::string& effectLine);

public:
    Player(const std::string& name, const std::string& actionFile,
        int hp, int atk, int mp, int agi, int recover);

    std::string GetName() const;
    int GetCurrentHP() const;
    int GetCurrentATK() const;
    int GetCurrentMP() const;
    int GetAGI() const;
    int GetCurrentRealm() const;

    const std::vector<SingleAction>& GetActions() const;
    std::shared_ptr<Action> GetAction(int idx);
    std::shared_ptr<Action> GetAction(std::string name);
    std::shared_ptr<Action> GetAction(int idx1, int idx2);
    std::shared_ptr<Action> GetAction(std::string name1, std::string name2);

    bool IsAlive() const;
    bool ValidateAction(const std::shared_ptr<Action> action) const;

    void RecoverMP(int amount);
    void ConsumeMP(int amount);
    void RecoverATK(int amount);
    void RecoverHP(int amount);
    void TakeDamage(int amount, bool physical = false);
    void DodgeSuccess();
    void UpdateRealm();
    void AccumulatePower();
    int BombPower();
};