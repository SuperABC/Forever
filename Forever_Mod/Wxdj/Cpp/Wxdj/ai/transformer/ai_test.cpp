#include "ai.h"
#include <cstdio>
using namespace wxdj;
using namespace std;
int main(int argc, char** argv) {
	string act = argv[1], bin = argv[2];
	auto p = make_shared<Cultivator>("P", act, 25800, 38900, 24800, 10, 619);
	auto e = make_shared<Cultivator>("E", act, 25800, 38900, 24800, 10, 619);
	Controller c({ p, e });
	AI ai(1);
	if (!ai.Load(bin)) { printf("LOAD FAIL\n"); return 1; }
	vector<int> moves;
	const auto& acts = e->GetActions();
	// 每个家族取第20式：名字以"第二十式"结尾
	for (size_t i = 0; i < acts.size(); ++i) { string n = acts[i].GetName(); if (n.size() >= 12 && n.compare(n.size() - 12, 12, "\xE7\xAC\xAC\xE4\xBA\x8C\xE5\x8D\x81\xE5\xBC\x8F") == 0) moves.push_back((int)i); }
	printf("moves %zu\n", moves.size());
	auto dump = [&](int round) {
		auto o = ai.Evaluate(c, round, moves);
		printf("R%d cls", round);
		for (float v : o.cls) printf(" %.5f", v);
		printf("\nR%d first", round);
		for (float v : o.first) printf(" %.5f", v);
		printf("\nR%d value %.5f\n", round, o.value);
	};
	c.StartTurn();
	dump(1);
	// 第1回合：玩家 无招第二十式 vs 敌方 铁手功第二十式(确定性对撞，不含随机)
	ai.BeginRound(c);
	auto a0 = p->GetAction(string("无招第二十式"));
	auto a1 = e->GetAction(string("铁手功第二十式"));
	c.ActionTurn({ {a0, 1}, {a1, 0} });
	auto logs = c.GetNews();
	c.CheckTurn({ {a0, 1}, {a1, 0} });
	for (auto& l : c.GetNews()) logs.push_back(l);
	c.EndTurn();
	for (auto& l : c.GetNews()) logs.push_back(l);
	ai.EndRound(c, { a0, a1 }, logs);
	c.StartTurn();
	dump(2);
	return 0;
}
