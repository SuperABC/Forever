#include "player.h"
#include <fstream>
#include <sstream>


using namespace std;

Player::Player(const string& name, const string& actionFile,
    int hp, int atk, int mp, int agi, int recover)
    : name(name), maxHP(hp), currentHP(hp), maxATK(atk), currentATK(atk),
    maxMP(mp), currentMP(mp), AGI(agi), currentRealm(YELLOW_INIT), recover(recover), bomb(0) {

    if (!LoadActions(actionFile)) {
        throw runtime_error("Failed to load actions from file " + actionFile);
    }
}

bool Player::LoadActions(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        istringstream iss(line);
        string name, attributeStr, realmStr;
        int mp, atk, numEffects;

        if (!(iss >> name >> attributeStr >> mp >> atk >> realmStr >> numEffects)) {
            continue;
        }

        ATTRIBUTE_TYPE attribute = ParseAttribute(attributeStr);
        Realm realm = ParseRealm(realmStr);
        vector<shared_ptr<Effect>> effects;
        for (int i = 0; i < numEffects; ++i) {
            if (!getline(file, line)) {
                break;
            }
            auto effect = ParseEffect(line);
            if (effect) {
                effects.push_back(effect);
            }
        }

        actions.emplace_back(name, attribute, atk, mp, realm, effects);
    }

    file.close();
    cout << "Loaded " << actions.size() << " actions from " << filename << endl;
    return true;
}

Realm Player::ParseRealm(const string& realmStr) {
    size_t dashPos = realmStr.find('-');
    if (dashPos == string::npos) {
        cerr << "Error: Invalid realm format: " << realmStr << endl;
        return YELLOW_INIT;
    }

    int major = stoi(realmStr.substr(0, dashPos));
    int minor = stoi(realmStr.substr(dashPos + 1));

    int base = (major - 1) * 6;
    switch (minor) {
    case 1: return static_cast<Realm>(base + YELLOW_INIT);
    case 2: return static_cast<Realm>(base + YELLOW_INIT_PEAK);
    case 3: return static_cast<Realm>(base + YELLOW_MID);
    case 4: return static_cast<Realm>(base + YELLOW_MID_PEAK);
    case 5: return static_cast<Realm>(base + YELLOW_LATE);
    case 6: return static_cast<Realm>(base + YELLOW_LATE_PEAK);
    default:
        cerr << "Error: Invalid realm minor: " << minor << endl;
        return YELLOW_INIT;
    }
}

ATTRIBUTE_TYPE Player::ParseAttribute(const string& attrStr) {
    if (attrStr == "none") return ATTRIBUTE_NONE;
    if (attrStr == "metal") return ATTRIBUTE_METAL;
    if (attrStr == "wood") return ATTRIBUTE_WOOD;
    if (attrStr == "water") return ATTRIBUTE_WATER;
    if (attrStr == "fire") return ATTRIBUTE_FIRE;
    if (attrStr == "earth") return ATTRIBUTE_EARTH;
    if (attrStr == "all") return ATTRIBUTE_ALL;

    return ATTRIBUTE_NONE;
}

shared_ptr<Effect> Player::ParseEffect(const string& effectLine) {
    istringstream iss(effectLine);
    string effectType;
    iss >> effectType;

    if (effectType == "physical") {
        return make_shared<PhysicalEffect>();
    }
    else if (effectType == "penetrate") {
        float none, metal, wood, water, fire, earth, all;
        if (iss >> none >> metal >> wood >> water >> fire >> earth >> all) {
            return make_shared<PenetrateEffect>(none, metal, wood, water, fire, earth, all);
        }
    }
    else if (effectType == "reduce") {
        float none, metal, wood, water, fire, earth, all;
        if (iss >> none >> metal >> wood >> water >> fire >> earth >> all) {
            return make_shared<ReduceEffect>(none, metal, wood, water, fire, earth, all);
        }
    }
    else if (effectType == "rebound") {
        float none, metal, wood, water, fire, earth, all;
        if (iss >> none >> metal >> wood >> water >> fire >> earth >> all) {
            return make_shared<ReboundEffect>(none, metal, wood, water, fire, earth, all);
        }
    }
    else if (effectType == "absorb") {
        float none, metal, wood, water, fire, earth, all;
        if (iss >> none >> metal >> wood >> water >> fire >> earth >> all) {
            return make_shared<AbsorbEffect>(none, metal, wood, water, fire, earth, all);
        }
    }
    else if (effectType == "lock") {
        float prob;
        if (iss >> prob) {
            return make_shared<LockEffect>(prob);
        }
    }
    else if (effectType == "dot") {
        int instant, continuous = 0, rounds = 0, interval = 1;
        if (iss >> instant >> continuous >> rounds >> interval) {
            return make_shared<DotEffect>(instant, continuous, rounds, interval);
        }
        else if (iss >> instant) {
            return make_shared<DotEffect>(instant);
        }
    }
    else if (effectType == "rebate") {
        int instant, continuous = 0, rounds = 0, interval = 1;
        if (iss >> instant >> continuous >> rounds >> interval) {
            return make_shared<RebateEffect>(instant, continuous, rounds, interval);
        }
        else if (iss >> instant) {
            return make_shared<RebateEffect>(instant);
        }
    }
    else if (effectType == "recover") {
        int instant, continuous = 0, rounds = 0, interval = 1;
        if (iss >> instant >> continuous >> rounds >> interval) {
            return make_shared<RecoverEffect>(instant, continuous, rounds, interval);
        }
        else if (iss >> instant) {
            return make_shared<RecoverEffect>(instant);
        }
    }
    else if (effectType == "cure") {
        int instant, continuous = 0, rounds = 0, interval = 1;
        if (iss >> instant >> continuous >> rounds >> interval) {
            return make_shared<CureEffect>(instant, continuous, rounds, interval);
        }
        else if (iss >> instant) {
            return make_shared<CureEffect>(instant);
        }
    }

    return nullptr;
}

string Player::GetName() const {
    return name;
}
int Player::GetCurrentHP() const {
    return currentHP;
}
int Player::GetCurrentATK() const {
    return currentATK;
}
int Player::GetCurrentMP() const {
    return currentMP;
}
int Player::GetAGI() const {
    return AGI;
}
int Player::GetCurrentRealm() const {
    return currentRealm;
}

const vector<SingleAction>& Player::GetActions() const {
    return actions;
}

shared_ptr<Action> Player::GetAction(int idx) {
    if (idx < 0 || idx >= actions.size() + 4) return make_shared<SkipAction>();
    if (idx >= actions.size() && idx <= actions.size() + 2) return make_shared<AccumulateAction>();
    if (idx == actions.size() + 3) return make_shared<BombAction>();
    return make_shared<SingleAction>(actions[idx]);
}

shared_ptr<Action> Player::GetAction(string name) {
    for (auto action : actions) {
        if (action.GetName() == name) {
            return make_shared<SingleAction>(action);
        }
    }
    return make_shared<SkipAction>();
}

shared_ptr<Action> Player::GetAction(int idx1, int idx2) {
    if (idx1 < 0 || idx1 >= actions.size()) return make_shared<SkipAction>();
    if (idx2 < 0 || idx2 >= actions.size()) return make_shared<SkipAction>();

    auto action1 = make_shared<SingleAction>(actions[idx1]);
    auto action2 = make_shared<SingleAction>(actions[idx2]);
    return make_shared<DualAction>(action1, action2);
}

shared_ptr<Action> Player::GetAction(string name1, string name2) {
    shared_ptr<SingleAction> action1 = nullptr, action2 = nullptr;
    for (auto action : actions) {
        if (action.GetName() == name1) {
            action1 = make_shared<SingleAction>(action);
            break;
        }
    }
    for (auto action : actions) {
        if (action.GetName() == name2) {
            action2 = make_shared<SingleAction>(action);
            break;
        }
    }
    if (action1 && action2) {
        return make_shared<DualAction>(action1, action2);
    }

    return make_shared<SkipAction>();
}

bool Player::IsAlive() const {
    return currentHP > 0 && currentATK > 0;
}

bool Player::ValidateAction(const shared_ptr<Action> action) const {
    return action->Validate(currentMP);
}

void Player::RecoverMP(int amount) {
    if (amount <= 0) {
        currentMP += recover;
    }
    else {
        currentMP += amount;
    }
    if (currentMP > maxMP * 2) {
        currentHP = 0;
    }
    else if (currentMP > maxMP) {
        currentMP = maxMP;
    }
}

void Player::ConsumeMP(int amount) {
    if (currentMP >= amount) {
        currentMP -= amount;
    }
    else {
        int deficit = amount - currentMP;
        currentMP = 0;
        currentATK = max(0, currentATK - deficit * 2);
    }
}

void Player::RecoverATK(int amount) {
    currentATK = min(maxATK, currentATK + amount);
}

void Player::RecoverHP(int amount) {
    currentHP = min(maxHP, currentHP + amount);
}

void Player::TakeDamage(int amount, bool physical) {
    if (physical) {
        currentHP = max(0, currentHP - amount);
    }
    else {
        if (amount >= currentATK * 0.6) {
            currentHP = max(0, currentHP - amount);
        }
        else if (amount >= currentATK * 0.3) {
            currentHP = max(0, static_cast<int>(currentHP - amount * 0.5f));
        }
        currentATK -= amount;
    }
}

void Player::DodgeSuccess() {

}

void Player::UpdateRealm() {
    for (const auto& requirement : REALM_ATK_REQUIREMENTS) {
        if (currentATK >= requirement.second) {
            currentRealm = requirement.first;
        }
        else {
            break;
        }
    }
}

void Player::AccumulatePower() {
    bomb += 0.4f * currentMP;
    currentMP *= 0.6f;
}

int Player::BombPower() {
    int power = bomb;
    bomb = 0;
    return power;
}