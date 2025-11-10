#include "player.h"
#include "controller.h"

#include <iostream>
#include <algorithm>

#define RANDOM_MOVE
//#define SPECIFIC_MOVE


using namespace std;

vector<string> attributeText = {
    "无属性",
    "金属性",
    "木属性",
    "水属性",
    "火属性",
    "土属性",
    "五行属性"
};

int GetRandom(int range) {
    if (range <= 0)return 0;

    mt19937 rng(random_device{}());
    uniform_int_distribution<int> dist(0, range - 1);
    int ret = dist(rng);

    return ret;
}

void PrintNews(vector<Log> news, vector<shared_ptr<Player>> players) {
    // TODO: 增加元素反应日志

    for (auto n : news) {
        switch (n.type) {
        case LOG_MP:
            cout << players[n.subject]->GetName() << "真气量回复至" << players[n.subject]->GetCurrentMP() << endl;
            break;
        case LOG_ATK:
            cout << players[n.subject]->GetName() << "武力值回复至" << players[n.subject]->GetCurrentATK() << endl;
            break;
        case LOG_HP:
            cout << players[n.subject]->GetName() << "健康值回复至" << players[n.subject]->GetCurrentHP() << endl;
            break;
        case LOG_CONSUME:
            cout << players[n.subject]->GetName() << "消耗" << n.amount << "点武力值" << endl;
            break;
        case LOG_SKIP:
            cout << players[n.subject]->GetName() << "跳过回合" << endl;
            break;
        case LOG_MOVE:
            cout << players[n.subject]->GetName() << "消耗" << n.amount << "点真气对" << players[n.object]->GetName() << "出招：" <<
                n.name << ", 点数" << n.pointAtk << endl;
            break;
        case LOG_DAMAGE:
            cout << players[n.subject]->GetName() << "对" << players[n.object]->GetName() << "造成";
            if (n.pointAtk > 0) {
                cout << attributeText[n.attribute] << n.pointAtk << "点招式武力伤害" << endl;
            }
            else {
                cout << n.pointHp << "点招式物理伤害" << endl;
            }
            break;
        case LOG_DODGE:
            cout << players[n.object]->GetName() << "闪避成功" << endl;
            break;
        case LOG_FAIL:
            cout << players[n.subject]->GetName() << "出招失败" << endl;
            break;
        case LOG_EFFECT:
            switch (n.effect) {
            case EFFECT_PENETRATE:
                cout << players[n.subject]->GetName() << "对" << players[n.object]->GetName() << "造成";
                if (n.pointAtk > 0) {
                    cout << attributeText[n.attribute] << n.pointAtk << "点穿透武力伤害" << endl;
                }
                else {
                    cout << n.pointHp << "点穿透物理伤害" << endl;
                }
                break;
            case EFFECT_REBOUND:
                cout << players[n.subject]->GetName() << "对" << players[n.object]->GetName() << "造成";
                if (n.pointAtk > 0) {
                    cout << attributeText[n.attribute] << n.pointAtk << "点反弹武力伤害" << endl;
                }
                else {
                    cout << n.pointHp << "点反弹物理伤害" << endl;
                }
                break;
            case EFFECT_ABSORB:
                cout << players[n.subject]->GetName() << "吸收" << n.amount << "点真气" << endl;
                break;
            case EFFECT_LOCK:
                cout << players[n.object]->GetName() << "被" << players[n.subject]->GetName() << "禁锢" << endl;
                break;
            case EFFECT_DOT:
                cout << players[n.object]->GetName() << "受到" << n.amount << "点持续武力伤害" << endl;
                break;
            case EFFECT_REBATE:
                cout << players[n.subject]->GetName() << "恢复" << n.amount << "点真气" << endl;
                break;
            case EFFECT_RECOVER:
                cout << players[n.subject]->GetName() << "恢复" << n.amount << "点武力值" << endl;
                break;
            case EFFECT_CURE:
                cout << players[n.subject]->GetName() << "恢复" << n.amount << "点健康值" << endl;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }
}

int main() {
    auto player1 = make_shared<Player>("张三", "./act/xiaohua.txt", 
        1000, 1200, 1600, 20, 40);
    auto player2 = make_shared<Player>("李四", "./act/xiaohua.txt",
        1000, 1080, 1800, 22, 40);

    vector<shared_ptr<Player>> players = { player1, player2 };
    Controller game(players);

    cout << "《五行斗诀》战斗开始！" << endl;
    cout << player1->GetName() << " VS " << player2->GetName() << endl;

    int turnCount = 0;
    while (player1->IsAlive() && player2->IsAlive()) {
        cout << "\n--- 第 " << ++turnCount << " 回合 ---" << endl;
        cout << player1->GetName() << ": HP=" << player1->GetCurrentHP()
            << ", ATK=" << player1->GetCurrentATK()
            << ", MP=" << player1->GetCurrentMP() << endl;
        cout << player2->GetName() << ": HP=" << player2->GetCurrentHP()
            << ", ATK=" << player2->GetCurrentATK()
            << ", MP=" << player2->GetCurrentMP() << endl;

        game.StartTurn();
        PrintNews(game.GetNews(), players);

        shared_ptr<Action> action1 = nullptr, action2 = nullptr;

#ifdef RANDOM_MOVE
        int actionCount1 = player1->GetActions().size();
        int actionCount2 = player2->GetActions().size();

        if (GetRandom(4)) {
            action1 = player1->GetAction(GetRandom(actionCount1 + 6));
        }
        else {
            action1 = player1->GetAction(GetRandom(actionCount1), GetRandom(actionCount1));
        }
        if (GetRandom(4)) {
            action2 = player2->GetAction(GetRandom(actionCount2 + 6));
        }
        else {
            action2 = player2->GetAction(GetRandom(actionCount2), GetRandom(actionCount2));
        }
#endif // RANDOM_MOVE
#ifdef SPECIFIC_MOVE
        action1 = player1->GetAction(11);
        action2 = player2->GetAction(4);
#endif

        game.ActionTurn({ { action1, 1 }, { action2, 0 } });
        PrintNews(game.GetNews(), players);

        game.CheckTurn({ { action1, 1 }, { action2, 0 } });
        PrintNews(game.GetNews(), players);

        game.EndTurn();
        PrintNews(game.GetNews(), players);

        cout << "回合结束" << endl;
    }

    if (!player1->IsAlive() && !player2->IsAlive()) {
        cout << "\n战斗结束！同归于尽!" << endl;
    }
    else {
        cout << "\n战斗结束！胜利者: " << (player1->IsAlive() ? player1->GetName() : player2->GetName()) << "!" << endl;
    }
}

