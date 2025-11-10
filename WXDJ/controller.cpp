#include "controller.h"

#include <stdexcept>


using namespace std;

bool Controller::CheckCounter(ATTRIBUTE_TYPE a1, ATTRIBUTE_TYPE a2) {
    return ATTRIBUTE_COUNTERS.find({ a1, a2 }) != ATTRIBUTE_COUNTERS.end();
}

double Controller::DodgeChance(int attackerAGI, int defenderAGI) {
    return static_cast<double>(
        defenderAGI * defenderAGI) / (attackerAGI * attackerAGI + defenderAGI * defenderAGI);
}

int Controller::CheckReaction(ATTRIBUTE_TYPE a1, ATTRIBUTE_TYPE a2) {
    if (a1 == ATTRIBUTE_METAL) {
        if (a2 == ATTRIBUTE_FIRE)return EFFECT_BURN;
        if (a2 == ATTRIBUTE_WATER)return EFFECT_RUST;
    }
    if (a1 == ATTRIBUTE_EARTH) {
        if (a2 == ATTRIBUTE_FIRE)return EFFECT_DUST;
        if (a2 == ATTRIBUTE_WATER)return EFFECT_MUD;
    }
    if (a1 == ATTRIBUTE_FIRE) {
        if (a2 == ATTRIBUTE_METAL)return EFFECT_BURN;
        if (a2 == ATTRIBUTE_EARTH)return EFFECT_DUST;
    }
    if (a1 == ATTRIBUTE_WATER) {
        if (a2 == ATTRIBUTE_METAL)return EFFECT_RUST;
        if (a2 == ATTRIBUTE_EARTH)return EFFECT_MUD;
    }

    return EFFECT_NONE;
}

bool Controller::FindReaction(EFFECT_TYPE reaction) {
    auto it = find_if(environment.begin(), environment.end(), [&reaction](shared_ptr<Effect> iter) {
        return iter->GetType() == reaction;
        });

    return it != environment.end();
}

void Controller::PutoutReaction(ATTRIBUTE_TYPE attribute) {
    switch (attribute) {
    case ATTRIBUTE_METAL: {
        for (auto it = environment.begin(); it != environment.end(); ) {
            if ((*it)->GetType() == EFFECT_DUST || (*it)->GetType() == EFFECT_MUD) {
                it = environment.erase(it);
            }
            else {
                ++it;
            }
        }
        break;
    }
    case ATTRIBUTE_WATER: {
        for (auto it = environment.begin(); it != environment.end(); ) {
            if ((*it)->GetType() == EFFECT_BURN || (*it)->GetType() == EFFECT_DUST) {
                it = environment.erase(it);
            }
            else {
                ++it;
            }
        }
        break;
    }
    case ATTRIBUTE_FIRE: {
        for (auto it = environment.begin(); it != environment.end(); ) {
            if ((*it)->GetType() == EFFECT_RUST || (*it)->GetType() == EFFECT_MUD) {
                it = environment.erase(it);
            }
            else {
                ++it;
            }
        }
        break;
    }
    case ATTRIBUTE_EARTH: {
        for (auto it = environment.begin(); it != environment.end(); ) {
            if ((*it)->GetType() == EFFECT_BURN || (*it)->GetType() == EFFECT_RUST) {
                it = environment.erase(it);
            }
            else {
                ++it;
            }
        }
        break;
    }
    default:
        break;
    }
}

Controller::Controller(vector<shared_ptr<Player>> players) : players(players) {
    effects.resize(players.size());
    status.resize(players.size());

    rng.seed(random_device()());
}

Controller::~Controller() {
    // shared_ptr 会自动管理内存，不需要手动删除
}

void Controller::ResetRound() {
    for (auto& s : status) {
        s = 0;
    }
}

void Controller::StartTurn() {
    ResetRound();

    ExecEffect(STAGE_START);

    for (auto it = players.begin(); it != players.end(); ++it) {
        int i = distance(players.begin(), it);
        RecoverMP(i, -1);
    }
}

void Controller::ActionTurn(vector<pair<shared_ptr<Action>, int>> actions) {
    if (actions.size() != players.size())
        throw runtime_error("Players and actions mismatch.");

    ExecEffect(STAGE_ACTION);

    for (int i = 0; i < players.size(); i++) {
        if (actions[i].first->GetType() == ACTION_SKIP) {
            SkipMove(i);
        }
        else if (actions[i].first->GetType() == ACTION_ACCUMULATE) {
            MakeMove(i, actions[i].second, actions[i].first->GetPower(), actions[i].first->GetText(), actions[i].first->GetPoint());
            players[i]->AccumulatePower();
        }
        else if (status[i] & 0x01) {
            SkipMove(i);
        }
        else if (actions[i].first->GetType() == ACTION_BOMB) {
            MakeMove(i, actions[i].second, actions[i].first->GetPower(), actions[i].first->GetText(), actions[i].first->GetPoint());
            dynamic_pointer_cast<BombAction>(actions[i].first)->SetPower(players[i]->BombPower());
        }
        else {
            MakeMove(i, actions[i].second, actions[i].first->GetPower(), actions[i].first->GetText(), actions[i].first->GetPoint());

            auto dot = actions[i].first->GetEffect(EFFECT_DOT);
            if (dot && !(dynamic_pointer_cast<DotEffect>(dot))->NeedHit()) {
                int amount = (dynamic_pointer_cast<DotEffect>(dot))->DotInstant();
                int object = actions[i].second;
                logs.push_back(Log(i, object, amount, EFFECT_DOT));
                TakeDamage(i, object, actions[i].first->GetEffects(), actions[object].first->GetEffects(),
                    actions[i].first->GetAttribute(), amount);
                AddEffect(object, make_shared<DotEffect>(*(dynamic_pointer_cast<DotEffect>(dot).get())));
            }

            auto rebate = actions[i].first->GetEffect(EFFECT_REBATE);
            if (rebate && !(dynamic_pointer_cast<RebateEffect>(rebate))->NeedHit()) {
                int amount = (dynamic_pointer_cast<RebateEffect>(rebate))->RebateInstant();
                logs.push_back(Log(i, -1, amount, EFFECT_REBATE));
                RecoverMP(i, amount);
                AddEffect(i, make_shared<RebateEffect>(*(dynamic_pointer_cast<RebateEffect>(rebate).get())));
            }

            auto recover = actions[i].first->GetEffect(EFFECT_RECOVER);
            if (recover && !(dynamic_pointer_cast<RecoverEffect>(recover))->NeedHit()) {
                int amount = (dynamic_pointer_cast<RecoverEffect>(recover))->RecoverInstant();
                logs.push_back(Log(i, -1, amount, EFFECT_RECOVER));
                RecoverATK(i, amount);
                AddEffect(i, make_shared<RecoverEffect>(*(dynamic_pointer_cast<RecoverEffect>(recover).get())));
            }

            auto cure = actions[i].first->GetEffect(EFFECT_CURE);
            if (cure && !(dynamic_pointer_cast<CureEffect>(cure))->NeedHit()) {
                int amount = (dynamic_pointer_cast<CureEffect>(cure))->CureInstant();
                logs.push_back(Log(i, -1, amount, EFFECT_CURE));
                RecoverHP(i, amount);
                AddEffect(i, make_shared<CureEffect>(*(dynamic_pointer_cast<CureEffect>(cure).get())));
            }
        }
    }
    for (auto action : actions) {
        PutoutReaction(action.first->GetAttribute());
    }
}

void Controller::CheckTurn(vector<pair<shared_ptr<Action>, int>> actions) {
    if (actions.size() != players.size())
        throw runtime_error("Players and actions mismatch.");

    ExecEffect(STAGE_CHECK);

    for (int i = 0; i < players.size(); i++) {
        if (actions[i].first->GetType() == ACTION_SKIP)continue;
        if (actions[i].first->GetType() == ACTION_ACCUMULATE)continue;
        if (status[i] & 0x01)continue;

        int j = actions[i].second;

        if (actions[i].first->GetType() == ACTION_BOMB) {
            if (actions[j].first->GetType() == ACTION_SKIP ||
                actions[j].first->GetType() == ACTION_ACCUMULATE ||
                actions[j].second != i || (status[j] & 0x01)) {
                // 玩家i施放真气炸弹，玩家j跳过，计算双方受到的伤害
                float powerEnemy = actions[i].first->GetPower() * (FindReaction(EFFECT_RUST) ? 1.0f :
                    ((1.f / (players[j]->GetAGI() * players[j]->GetAGI() + 1.0f) + 1.f) * 0.8f));
                float powerSelf = actions[i].first->GetPower() * (FindReaction(EFFECT_RUST) ? 1.0f :
                    ((1.f / (players[i]->GetAGI() * players[j]->GetAGI() + 1.0f) + 1.f) * 0.4f));
                TakeDamage(i, j, actions[i].first->GetEffects(), actions[j].first->GetEffects(),
                    actions[i].first->GetAttribute(), powerEnemy);
                TakeDamage(i, i, actions[i].first->GetEffects(), actions[i].first->GetEffects(),
                    actions[i].first->GetAttribute(), powerSelf);
            }
            else if (actions[j].first->GetType() == ACTION_BOMB && actions[j].second == i) {
                // 玩家i施放真气炸弹，玩家j施放真气炸弹，计算玩家j受到的伤害
                float power = sqrt(pow(actions[i].first->GetPower(), 2) + pow(actions[j].first->GetPower(), 2));
                float powerEnemy = power * (FindReaction(EFFECT_RUST) ? 1.0f :
                    ((1.f / (players[j]->GetAGI() * players[j]->GetAGI() + 1.0f) + 1.f) * 0.8f));
                TakeDamage(j, j, actions[j].first->GetEffects(), actions[j].first->GetEffects(),
                    actions[j].first->GetAttribute(), powerEnemy);
            }
            else {
                // 玩家i施放真气炸弹，玩家j施放普通招式，计算玩家j受到的伤害
                float powerEnemy = 0.f;
                if (actions[j].first->GetPower() < actions[i].first->GetPower()) {
                    powerEnemy = actions[i].first->GetPower() - actions[j].first->GetPower();
                    powerEnemy *= (FindReaction(EFFECT_RUST) ? 1.0f :
                        ((1.f / (players[j]->GetAGI() * players[j]->GetAGI() + 1.0f) + 1.f) * 0.8f));
                    TakeDamage(i, j, actions[i].first->GetEffects(), actions[j].first->GetEffects(),
                        actions[i].first->GetAttribute(), powerEnemy);
                }
            }
        }
        else {
            if (actions[j].first->GetType() == ACTION_SKIP ||
                actions[j].first->GetType() == ACTION_ACCUMULATE ||
                actions[j].second != i || (status[j] & 0x01)) {
                // 玩家i施放普通招式，玩家j跳过，计算玩家j受到的伤害
                uniform_real_distribution<double> dist(0.0, 1.0);
                double dodgeChance = FindReaction(EFFECT_RUST) ? 0 : DodgeChance(players[i]->GetAGI(), players[j]->GetAGI());
                if (!(status[j] & 0x01) && dist(rng) < dodgeChance) {
                    DodgeSuccess(i, j);
                    continue;
                }
                else {
                    bool hit = true;
                    if (FindReaction(EFFECT_DUST)) {
                        hit = (dist(rng) < 0.5f);
                    }
                    if (hit) {
                        HitSuccess(i, j, actions[i].first, actions[j].first);
                        TakeDamage(i, j, actions[i].first->GetEffects(), actions[j].first->GetEffects(),
                            actions[i].first->GetAttribute(), actions[i].first->GetPoint());
                    }
                    else {
                        HitFail(i);
                    }
                }
            }
            else if (actions[j].first->GetType() == ACTION_BOMB && actions[j].second == i) {
                // 玩家i施放普通招式，玩家j施放真气炸弹，计算玩家j受到的伤害
                float powerEnemy = 0.f;
                if (actions[i].first->GetPower() < actions[j].first->GetPower()) {
                    powerEnemy = actions[j].first->GetPower();
                    powerEnemy *= (FindReaction(EFFECT_RUST) ? 1.0f :
                        ((1.f / (players[j]->GetAGI() * players[j]->GetAGI() + 1.0f) + 1.f) * 0.4f));
                    TakeDamage(j, j, actions[j].first->GetEffects(), actions[j].first->GetEffects(),
                        actions[j].first->GetAttribute(), powerEnemy);
                }
                else if (actions[i].first->GetPower() > 2 * actions[j].first->GetPower()) {
                    powerEnemy = actions[i].first->GetPower() - 2 * actions[j].first->GetPower();
                    TakeDamage(i, j, actions[i].first->GetEffects(), actions[j].first->GetEffects(),
                        actions[i].first->GetAttribute(), powerEnemy);
                }
                else {
                    powerEnemy = 2 * actions[j].first->GetPower() - actions[i].first->GetPower();
                    powerEnemy *= (FindReaction(EFFECT_RUST) ? 1.0f :
                        ((1.f / (players[j]->GetAGI() * players[j]->GetAGI() + 1.0f) + 1.f) * 0.4f));
                    TakeDamage(j, j, actions[j].first->GetEffects(), actions[j].first->GetEffects(),
                        actions[j].first->GetAttribute(), powerEnemy);
                }
            }
            else {
                // 玩家i施放普通招式，玩家j施放普通招式，计算玩家j受到的伤害
                uniform_real_distribution<double> dist(0.0, 1.0);
                if (actions[j].second == i) {
                    if (j < i)continue;

                    int point1 = actions[i].first->GetPoint();
                    int point2 = actions[j].first->GetPoint();
                    ATTRIBUTE_TYPE attribute1 = actions[i].first->GetAttribute();
                    ATTRIBUTE_TYPE attribute2 = actions[j].first->GetAttribute();

                    if (CheckCounter(attribute1, attribute2)) {
                        point2 /= 2;
                    }
                    if (CheckCounter(attribute2, attribute1)) {
                        point1 /= 2;
                    }
                    switch (CheckReaction(attribute1, attribute2)) {
                    case EFFECT_BURN:
                        AddEnvironment(make_shared<BurnEffect>());
                        break;
                    case EFFECT_RUST:
                        AddEnvironment(make_shared<RustEffect>());
                        break;
                    case EFFECT_DUST:
                        AddEnvironment(make_shared<DustEffect>());
                        break;
                    case EFFECT_MUD:
                        AddEnvironment(make_shared<MudEffect>());
                        break;
                    default:
                        break;
                    }

                    if (point1 > point2) {
                        bool hit = true;
                        if (FindReaction(EFFECT_DUST)) {
                            hit = (dist(rng) < 0.5f);
                        }
                        if (hit) {
                            HitSuccess(i, j, actions[i].first, actions[j].first);
                            HitSuccess(j, i, actions[j].first, actions[i].first);
                            TakeDamage(i, j, actions[i].first->GetEffects(), actions[j].first->GetEffects(),
                                actions[i].first->GetAttribute(), point1 - point2);
                            if (auto effect = actions[i].first->GetEffect(EFFECT_PENETRATE)) {
                                int penerate = point2 * (dynamic_pointer_cast<PenetrateEffect>(effect))->GetRatio(actions[j].first->GetAttribute());
                                logs.push_back(Log(i, j, actions[i].first->GetAttribute(), penerate, 0, EFFECT_PENETRATE));
                                TakeDamage(i, j, actions[i].first->GetEffects(), actions[j].first->GetEffects(),
                                    actions[i].first->GetAttribute(), penerate, false);
                            }
                            if (auto effect = actions[j].first->GetEffect(EFFECT_PENETRATE)) {
                                int penerate = point2 * (dynamic_pointer_cast<PenetrateEffect>(effect))->GetRatio(actions[i].first->GetAttribute());
                                logs.push_back(Log(j, i, actions[j].first->GetAttribute(), penerate, 0, EFFECT_PENETRATE));
                                TakeDamage(j, i, actions[j].first->GetEffects(), actions[i].first->GetEffects(),
                                    actions[j].first->GetAttribute(), penerate, false);
                            }
                        }
                        else {
                            HitFail(i);
                        }
                    }
                    else if (point2 > point1) {
                        bool hit = true;
                        if (FindReaction(EFFECT_DUST)) {
                            hit = (dist(rng) < 0.5f);
                        }
                        if (hit) {
                            HitSuccess(i, j, actions[i].first, actions[j].first);
                            HitSuccess(j, i, actions[j].first, actions[i].first);
                            TakeDamage(j, i, actions[j].first->GetEffects(), actions[i].first->GetEffects(),
                                actions[j].first->GetAttribute(), point2 - point1);
                            if (auto effect = actions[i].first->GetEffect(EFFECT_PENETRATE)) {
                                int penerate = point1 * (dynamic_pointer_cast<PenetrateEffect>(effect))->GetRatio(actions[j].first->GetAttribute());
                                logs.push_back(Log(i, j, actions[i].first->GetAttribute(), penerate, 0, EFFECT_PENETRATE));
                                TakeDamage(i, j, actions[i].first->GetEffects(), actions[j].first->GetEffects(),
                                    actions[i].first->GetAttribute(), penerate, false);
                            }
                            if (auto effect = actions[j].first->GetEffect(EFFECT_PENETRATE)) {
                                int penerate = point1 * (dynamic_pointer_cast<PenetrateEffect>(effect))->GetRatio(actions[i].first->GetAttribute());
                                logs.push_back(Log(j, i, actions[j].first->GetAttribute(), penerate, 0, EFFECT_PENETRATE));
                                TakeDamage(j, i, actions[j].first->GetEffects(), actions[i].first->GetEffects(),
                                    actions[j].first->GetAttribute(), penerate, false);
                            }
                        }
                        else {
                            HitFail(i);
                        }
                    }
                    else {
                        bool hit = true;
                        if (FindReaction(EFFECT_DUST)) {
                            hit = (dist(rng) < 0.5f);
                        }
                        if (hit) {
                            HitSuccess(i, j, actions[i].first, actions[j].first);
                            HitSuccess(j, i, actions[j].first, actions[i].first);
                            if (auto effect = actions[i].first->GetEffect(EFFECT_PENETRATE)) {
                                int penerate = point2 * (dynamic_pointer_cast<PenetrateEffect>(effect))->GetRatio(actions[j].first->GetAttribute());
                                logs.push_back(Log(i, j, actions[i].first->GetAttribute(), penerate, 0, EFFECT_PENETRATE));
                                TakeDamage(i, j, actions[i].first->GetEffects(), actions[j].first->GetEffects(),
                                    actions[i].first->GetAttribute(), penerate, false);
                            }
                            if (auto effect = actions[j].first->GetEffect(EFFECT_PENETRATE)) {
                                int penerate = point1 * (dynamic_pointer_cast<PenetrateEffect>(effect))->GetRatio(actions[i].first->GetAttribute());
                                logs.push_back(Log(j, i, actions[j].first->GetAttribute(), penerate, 0, EFFECT_PENETRATE));
                                TakeDamage(j, i, actions[j].first->GetEffects(), actions[i].first->GetEffects(),
                                    actions[j].first->GetAttribute(), penerate, false);
                            }
                        }
                        else {
                            HitFail(i);
                        }
                    }
                }
                else {
                    bool hit = true;
                    if (FindReaction(EFFECT_DUST)) {
                        hit = (dist(rng) < 0.5f);
                    }
                    if (hit) {
                        HitSuccess(i, j, actions[i].first, actions[j].first);
                        TakeDamage(i, j, actions[i].first->GetEffects(), actions[j].first->GetEffects(),
                            actions[i].first->GetAttribute(), actions[i].first->GetPoint());
                    }
                    else {
                        HitFail(i);
                    }
                }
            }
        }
    }
}

void Controller::EndTurn() {
    ExecEffect(STAGE_END);
    for (auto it = environment.begin(); it != environment.end(); ) {
        bool useUp;
        if (auto burnEffect = dynamic_pointer_cast<BurnEffect>(*it)) {
            useUp = burnEffect->UseUp();
        }
        else if (auto rustEffect = dynamic_pointer_cast<RustEffect>(*it)) {
            useUp = rustEffect->UseUp();
        }
        else if (auto dustEffect = dynamic_pointer_cast<DustEffect>(*it)) {
            useUp = dustEffect->UseUp();
        }
        else if (auto mudEffect = dynamic_pointer_cast<MudEffect>(*it)) {
            useUp = mudEffect->UseUp();
        }
        if (useUp) {
            it = environment.erase(it);
        }
        else {
            ++it;
        }
    }

    for (auto player : players) {
        player->UpdateRealm();
    }
}

void Controller::AddEffect(int player, shared_ptr<Effect> effect) {
    effects[player].push_back(effect);
}

void Controller::AddEnvironment(shared_ptr<Effect> effect) {
    auto it = find_if(environment.begin(), environment.end(), [&effect](shared_ptr<Effect> iter) {
        return iter->GetType() == effect->GetType();
        });
    if (it == environment.end()) {
        environment.push_back(effect);
    }
    else {
        (*it) = effect;
    }
}

void Controller::ExecEffect(STAGE_TYPE stage) {
    for (int i = 0; i < effects.size(); i++) {
        for (int j = 0; j < effects[i].size(); j++) {
            switch (effects[i][j]->GetType()) {
            case EFFECT_LOCK:
                if (stage == STAGE_START) {
                    status[i] |= 0x01;
                    for (int k = j + 1; k < effects[i].size(); k++) {
                        effects[i][k - 1] = effects[i][k];
                    }
                    effects[i].pop_back();
                    j--;
                }
                break;
            case EFFECT_DOT:
                if (stage == STAGE_START) {
                    int amount = (dynamic_pointer_cast<DotEffect>(effects[i][j]))->DotOnce();
                    if (amount == 0)break;
                    logs.push_back(Log(-1, i, amount, EFFECT_DOT));
                    TakeDamage(-1, i, {}, {}, ATTRIBUTE_NONE, amount, false);
                    if ((dynamic_pointer_cast<DotEffect>(effects[i][j]))->UseUp()) {
                        for (int k = j + 1; k < effects[i].size(); k++) {
                            effects[i][k - 1] = effects[i][k];
                        }
                        effects[i].pop_back();
                        j--;
                    }
                }
                break;
            case EFFECT_REBATE:
                if (stage == STAGE_START) {
                    int amount = (dynamic_pointer_cast<RebateEffect>(effects[i][j]))->RebateOnce();
                    if (amount == 0)break;
                    logs.push_back(Log(i, -1, amount, EFFECT_REBATE));
                    RecoverMP(i, amount, false);
                    if ((dynamic_pointer_cast<RebateEffect>(effects[i][j]))->UseUp()) {
                        for (int k = j + 1; k < effects[i].size(); k++) {
                            effects[i][k - 1] = effects[i][k];
                        }
                        effects[i].pop_back();
                        j--;
                    }
                }
                break;
            case EFFECT_RECOVER:
                if (stage == STAGE_START) {
                    int amount = (dynamic_pointer_cast<RecoverEffect>(effects[i][j]))->RecoverOnce();
                    if (amount == 0)break;
                    logs.push_back(Log(i, -1, amount, EFFECT_RECOVER));
                    RecoverATK(i, amount, false);
                    if ((dynamic_pointer_cast<RecoverEffect>(effects[i][j]))->UseUp()) {
                        for (int k = j + 1; k < effects[i].size(); k++) {
                            effects[i][k - 1] = effects[i][k];
                        }
                        effects[i].pop_back();
                        j--;
                    }
                }
                break;
            case EFFECT_CURE:
                if (stage == STAGE_START) {
                    int amount = (dynamic_pointer_cast<CureEffect>(effects[i][j]))->CureOnce();
                    if (amount == 0)break;
                    logs.push_back(Log(i, -1, amount, EFFECT_CURE));
                    RecoverHP(i, amount, false);
                    if ((dynamic_pointer_cast<CureEffect>(effects[i][j]))->UseUp()) {
                        for (int k = j + 1; k < effects[i].size(); k++) {
                            effects[i][k - 1] = effects[i][k];
                        }
                        effects[i].pop_back();
                        j--;
                    }
                }
                break;
            default:
                break;
            }
        }
    }
}

vector<Log> Controller::GetNews() {
    vector<Log> news;
    for (auto it = logs.begin() + mark; it != logs.end(); ++it) {
        news.push_back(*it);
    }
    mark = logs.size();

    return news;
}

void Controller::RecoverMP(int player, int amount, bool log) {
    if (log)
        logs.push_back(Log(LOG_MP, player, amount));
    players[player]->RecoverMP(amount);
}

void Controller::ConsumeMP(int player, int amount, bool log) {
    if (log)
        logs.push_back(Log(LOG_CONSUME, player, amount));
    players[player]->ConsumeMP(amount);
}

void Controller::MakeMove(int subject, int object, int amount, string name, int point, bool log) {
    if (FindReaction(EFFECT_MUD)) {
        amount *= 1.5f;
    }

    if (log)
        logs.push_back(Log(subject, object, amount, name, point));
    players[subject]->ConsumeMP(amount);
}

void Controller::SkipMove(int player, bool log) {
    if (log)
        logs.push_back(Log(player));
}

void Controller::RecoverATK(int player, int amount, bool log) {
    if (log)
        logs.push_back(Log(LOG_ATK, player, amount));
    players[player]->RecoverATK(amount);
}

void Controller::RecoverHP(int player, int amount, bool log) {
    if (log)
        logs.push_back(Log(LOG_HP, player, amount));
    players[player]->RecoverHP(amount);
}

void Controller::HitSuccess(int subject, int object, shared_ptr<Action> offend, shared_ptr<Action> defend, bool log) {
    auto lock = offend->GetEffect(EFFECT_LOCK);
    if (lock) {
        float prob = (dynamic_pointer_cast<LockEffect>(lock))->GetProb();
        uniform_real_distribution<double> dist(0.0, 1.0);
        if (dist(rng) < prob) {
            if (log)
                logs.push_back(Log(subject, object, 0, EFFECT_LOCK));
            AddEffect(object, lock);
        }
        else {
            lock->SetMiss();
        }
    }

    auto dot = offend->GetEffect(EFFECT_DOT);
    if (dot && (dynamic_pointer_cast<DotEffect>(dot))->NeedHit()) {
        int amount = (dynamic_pointer_cast<DotEffect>(dot))->DotInstant();
        if (log)
            logs.push_back(Log(subject, object, amount, EFFECT_DOT));
        TakeDamage(subject, object, offend->GetEffects(), defend->GetEffects(), offend->GetAttribute(), amount);
        AddEffect(object, make_shared<DotEffect>(*(dynamic_pointer_cast<DotEffect>(dot).get())));
    }

    auto rebate = offend->GetEffect(EFFECT_REBATE);
    if (rebate && (dynamic_pointer_cast<RebateEffect>(rebate))->NeedHit()) {
        int amount = (dynamic_pointer_cast<RebateEffect>(rebate))->RebateInstant();
        if (log)
            logs.push_back(Log(subject, -1, amount, EFFECT_REBATE));
        RecoverMP(subject, amount);
        AddEffect(subject, make_shared<RebateEffect>(*(dynamic_pointer_cast<RebateEffect>(rebate).get())));
    }

    auto recover = offend->GetEffect(EFFECT_RECOVER);
    if (recover && (dynamic_pointer_cast<RecoverEffect>(recover))->NeedHit()) {
        int amount = (dynamic_pointer_cast<RecoverEffect>(recover))->RecoverInstant();
        if (log)
            logs.push_back(Log(subject, -1, amount, EFFECT_RECOVER));
        RecoverATK(subject, amount);
        AddEffect(subject, make_shared<RecoverEffect>(*(dynamic_pointer_cast<RecoverEffect>(recover).get())));
    }

    auto cure = offend->GetEffect(EFFECT_CURE);
    if (cure && (dynamic_pointer_cast<CureEffect>(cure))->NeedHit()) {
        int amount = (dynamic_pointer_cast<CureEffect>(cure))->CureInstant();
        if (log)
            logs.push_back(Log(subject, -1, amount, EFFECT_CURE));
        RecoverHP(subject, amount);
        AddEffect(subject, make_shared<CureEffect>(*(dynamic_pointer_cast<CureEffect>(cure).get())));
    }
}

void Controller::HitFail(int subject, bool log) {
    if (log)
        logs.push_back(Log(LOG_FAIL, subject, 0));
}

void Controller::TakeDamage(int subject, int object, vector<shared_ptr<Effect>> offend, vector<shared_ptr<Effect>> defend,
    ATTRIBUTE_TYPE attribute, int amount, bool log) {
    if (FindReaction(EFFECT_BURN)) {
        amount *= 1.5f;
    }

    auto physical = find_if(offend.begin(), offend.end(), [](shared_ptr<Effect> effect) {return effect->GetType() == EFFECT_PHYSICAL; });
    if (log)
        logs.push_back(Log(subject, object, attribute, physical != offend.end() ? 0 : amount, physical != offend.end() ? amount : 0));
    players[object]->TakeDamage(amount, physical != offend.end());

    auto reduce = find_if(defend.begin(), defend.end(), [](shared_ptr<Effect> effect) {return effect->GetType() == EFFECT_REDUCE; });
    if (reduce != defend.end()) {
        amount *= 1.0f - (dynamic_pointer_cast<ReduceEffect>(*reduce))->GetRatio(attribute);
    }

    if (subject == object)return;

    auto rebound = find_if(defend.begin(), defend.end(), [](shared_ptr<Effect> effect) {return effect->GetType() == EFFECT_REBOUND; });
    if (rebound != defend.end()) {
        int damage = amount * (dynamic_pointer_cast<ReboundEffect>(*rebound))->GetRatio(attribute);
        if (damage > 0) {
            logs.push_back(Log(object, subject, attribute, physical != offend.end() ? 0 : damage, physical != offend.end() ? damage : 0, EFFECT_REBOUND));
            TakeDamage(object, subject, defend, offend, attribute, physical != offend.end() ? 0 : damage, physical != offend.end() ? damage : 0, false);
        }
    }

    auto absorb = find_if(defend.begin(), defend.end(), [](shared_ptr<Effect> effect) {return effect->GetType() == EFFECT_ABSORB; });
    if (absorb != defend.end()) {
        int mp = amount * (dynamic_pointer_cast<AbsorbEffect>(*absorb))->GetRatio(attribute);
        logs.push_back(Log(object, subject, mp, EFFECT_ABSORB));
        RecoverMP(object, mp);
    }
}

void Controller::TakeDamage(int subject, int object, vector<shared_ptr<Effect>> offend, vector<shared_ptr<Effect>> defend,
    ATTRIBUTE_TYPE attribute, int atk, int hp, bool log) {
    int amount = atk > 0 ? atk : hp;
    bool physical = hp > 0;

    if (FindReaction(EFFECT_BURN)) {
        amount *= 1.5f;
    }

    if (log)
        logs.push_back(Log(subject, object, attribute, atk, hp));
    players[object]->TakeDamage(amount, physical);

    auto rebound = find_if(defend.begin(), defend.end(), [](shared_ptr<Effect> effect) {return effect->GetType() == EFFECT_REBOUND; });
    if (rebound != defend.end()) {
        int damage = amount * (dynamic_pointer_cast<ReboundEffect>(*rebound))->GetRatio(attribute);
        if (damage > 0) {
            logs.push_back(Log(object, subject, attribute, physical ? 0 : damage, physical ? damage : 0, EFFECT_REBOUND));
            TakeDamage(object, subject, defend, offend, attribute, physical ? 0 : damage, physical ? damage : 0, false);
        }
    }

    if (subject == object)return;

    auto absorb = find_if(defend.begin(), defend.end(), [](shared_ptr<Effect> effect) {return effect->GetType() == EFFECT_ABSORB; });
    if (absorb != defend.end()) {
        int mp = amount * (dynamic_pointer_cast<AbsorbEffect>(*absorb))->GetRatio(attribute);
        logs.push_back(Log(object, subject, mp, EFFECT_ABSORB));
        RecoverMP(object, mp);
    }
}

void Controller::DodgeSuccess(int subject, int object, bool log) {
    if (log)
        logs.push_back(Log(subject, object));
    players[object]->DodgeSuccess();
}