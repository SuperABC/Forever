#define _ALLOW_KEYWORD_MACROS 1
// 通用对战模型的训练环境：批量并行跑对局，导出成C接口给Python(ctypes)用。
// 直接编译游戏的battle/规则源码(规则和系数与游戏完全一致)；特征见features.h。
// 用法见README.md。
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <map>
#include <iostream>
#define private public
#include "cultivator.h"
#include "controller.h"
#include "action.h"
#undef private
#include "features.h"

using namespace std;
using namespace wxdj;
using namespace wxfeat;

#ifdef _WIN32
#define API extern "C" __declspec(dllexport)
#else
#define API extern "C" __attribute__((visibility("default")))
#endif

static const int MAX_ROUNDS = 80;

// PvE模式：双方有五行属性/暴击/特性(见wx_pve_set)，特征在PvP的FM/SD后面追加FX/SX维(features.h)。PvP模式下维数和以前完全一样。
static bool g_pve = false;
static int gFM = FM, gSD = SD;

namespace {

vector<shared_ptr<Cultivator>> g_pool;           // 招式文件模板
unordered_map<string, int> g_keyIds;            // 字符串 -> 小整数(式/家族编号)
mutex g_keyMutex;

int KeyId(const string& s) {
	if (s.empty()) return -1;
	lock_guard<mutex> lock(g_keyMutex);
	auto it = g_keyIds.find(s);
	if (it != g_keyIds.end()) return it->second;
	int id = (int)g_keyIds.size();
	g_keyIds[s] = id;
	return id;
}

struct Hist { float v[HK][HD]; int len = 0; };

// PvE对局配置：每方的五行属性掩码、特性文本(每行一条"mod ..."，同ai.html的特性调试面板)，以及基础暴击
struct PveCfg {
	unsigned elem[2] = { 0, 0 };
	string traits[2];
	double critRate = 0, critDmg = 0.5;
};

struct Game {
	PveCfg cfg;
	shared_ptr<Cultivator> c[2];
	unique_ptr<Controller> ct;
	int round = 0;
	bool done = true;
	int outcome = 0;            // +1 席位0胜，-1 席位1胜，0 平局/未结束
	Hist hist[2];
	vector<vector<float>> moveFeat[2];   // [fighter][move] -> FM
	vector<int> form[2], fam[2];
};

vector<Game> g_games;

int ThreadCount() { return (int)max(1u, thread::hardware_concurrency()); }

template <class F> void ParallelFor(int n, F&& f) {
	int T = min(ThreadCount(), n);
	if (T <= 1) { for (int i = 0; i < n; ++i) f(i); return; }
	atomic<int> next(0);
	vector<thread> th;
	for (int t = 0; t < T; ++t) th.emplace_back([&] { for (int i; (i = next++) < n;) f(i); });
	for (auto& x : th) x.join();
}

float Loss(const Cultivator& c) {
	if (!c.IsAlive()) return 1.f;
	float h = (float)c.currentHP / c.maxHP, a = (float)c.currentATK / c.maxATK;
	return max(0.f, 1.f - (h + a) / 2.f);
}

void BuildMoveCache(Game& g) {
	for (int f = 0; f < 2; ++f) {
		auto& acts = g.c[f]->actions;
		g.moveFeat[f].assign(acts.size(), vector<float>(gFM));
		g.form[f].assign(acts.size(), -1);
		g.fam[f].assign(acts.size(), -1);
		for (size_t i = 0; i < acts.size(); ++i) {
			MoveFeatures(acts[i], *g.c[f], *g.c[1 - f], g.moveFeat[f][i].data());
			if (g_pve) MoveExtrasPvE(acts[i], *g.c[f], *g.c[1 - f], g.moveFeat[f][i].data() + FM);
			g.form[f][i] = KeyId(FormKey(acts[i].GetName()));
			g.fam[f][i] = KeyId(FamilyKey(acts[i].GetName()));
		}
	}
}

void FillState(Game& g, int seat, float* o) {
	memset(o, 0, sizeof(float) * gSD);
	int i = 0;
	for (int k = 0; k < 2; ++k) {
		int idx = k == 0 ? seat : 1 - seat;
		Cultivator& c = *g.c[idx];
		float mhp = (float)max(1, c.maxHP), matk = (float)max(1, c.maxATK), mmp = (float)max(1, c.maxMP);
		o[i++] = Clamp((float)c.currentHP / mhp);
		o[i++] = Clamp((float)c.currentATK / matk);
		o[i++] = Clamp((float)c.currentMP / mmp);
		o[i++] = Clamp((float)c.bomb / mmp);
		o[i++] = logf(mhp) / 12.f;
		o[i++] = logf(matk) / 12.f;
		o[i++] = logf(mmp) / 12.f;
		o[i++] = c.AGI / 30.f;
		o[i++] = Clamp((float)c.recover / mmp);
		o[i++] = (g.ct->status[idx] & 1) ? 1.f : 0.f;
		float dot = 0, reb = 0, rec = 0, cure = 0;
		for (auto& e : g.ct->effects[idx]) {
			switch (e->GetType()) {
			case EFFECT_DOT: { auto* d = static_cast<DotEffect*>(e.get()); dot += (float)d->DotContinuous() * d->DotRounds(); break; }
			case EFFECT_REBATE: { auto* d = static_cast<RebateEffect*>(e.get()); reb += (float)d->RebateContinuous() * d->RebateRounds(); break; }
			case EFFECT_RECOVER: { auto* d = static_cast<RecoverEffect*>(e.get()); rec += (float)d->RecoverContinuous() * d->RecoverRounds(); break; }
			case EFFECT_CURE: { auto* d = static_cast<CureEffect*>(e.get()); cure += (float)d->CureContinuous() * d->CureRounds(); break; }
			default: break;
			}
		}
		o[i++] = Clamp(dot / matk);
		o[i++] = Clamp(reb / mmp);
		o[i++] = Clamp(rec / matk);
		o[i++] = Clamp(cure / mhp);
		o[i++] = c.maxAGI / 30.f;
		o[i++] = c.agiLoss / 5.f;
		o[i++] = c.agiRecover / 2.f;
		o[i++] = c.bombMultiplier / 10.f;
	}
	for (auto& e : g.ct->environment) {
		switch (e->GetType()) {
		case EFFECT_BURN: o[i] = 1.f; break;
		case EFFECT_RUST: o[i + 1] = 1.f; break;
		case EFFECT_DUST: o[i + 2] = 1.f; break;
		case EFFECT_MUD: o[i + 3] = 1.f; break;
		default: break;
		}
	}
	i += 4;
	o[i++] = g.round / 40.f;
	StateExtras(*g.ct, seat, o + SD - 8);
	if (g_pve) StateExtrasPvE(*g.ct, seat, o + SD);
}

shared_ptr<Action> Build(Cultivator& c, int cls, int m1, int m2) {
	switch (cls) {
	case 1: return make_shared<AccumulateAction>();
	case 2: return make_shared<BombAction>();
	case 3: return c.GetAction(m1);
	case 4: return c.GetAction(m1, m2);
	default: return make_shared<SkipAction>();
	}
}

// 出招记录里的一方：类别/属性/消耗/威力/效果种类
void ActionRecord(const Action& a, int cls, const Cultivator& c, float* o) {
	memset(o, 0, sizeof(float) * 24);
	if (cls >= 0 && cls < 5) o[cls] = 1.f;
	if (cls >= 3) {
		int attr = (int)a.GetAttribute();
		if (attr >= 0 && attr < 7) o[5 + attr] = 1.f;
	}
	o[12] = Clamp((float)a.GetPoint() / max(1, c.maxMP));
	o[13] = Clamp((float)a.GetPower() / max(1, c.maxATK));
	static const EFFECT_TYPE types[10] = { EFFECT_PHYSICAL, EFFECT_PENETRATE, EFFECT_REDUCE, EFFECT_REBOUND, EFFECT_ABSORB,
		EFFECT_LOCK, EFFECT_DOT, EFFECT_REBATE, EFFECT_RECOVER, EFFECT_CURE };
	for (int k = 0; k < 10; ++k) if (a.GetEffect(types[k])) o[14 + k] = 1.f;
}

void Push(Hist& h, const float* row) {
	if (h.len < HK) { memcpy(h.v[h.len], row, sizeof(float) * HD); ++h.len; return; }
	memmove(h.v[0], h.v[1], sizeof(float) * HD * (HK - 1));
	memcpy(h.v[HK - 1], row, sizeof(float) * HD);
}

}  // namespace

API void wx_dims(int* out) { out[0] = gFM; out[1] = gSD; out[2] = HK; out[3] = HD; }

// 切换PvP/PvE模式(维数随之变化)：必须在创建对局(wx_init/wx_reset)之前调用，python端读维数要在它之后
API void wx_set_mode(int pve) {
	g_pve = pve != 0;
	gFM = g_pve ? FM_PVE : FM;
	gSD = g_pve ? SD_PVE : SD;
}

// 配置下一次wx_reset这盘对局的PvE设置(只在PvE模式下生效)：双方五行属性掩码(第n位=属性值n，金1木2水3火4土5)、
// 基础暴击率/暴击伤害、双方的特性文本(每行一条，格式同modifier.h的ParseModifier；解析失败的行忽略)
API void wx_pve_set(int gid, int elemA, int elemB, float critRate, float critDmg, const char* traitsA, const char* traitsB) {
	PveCfg& c = g_games[gid].cfg;
	c.elem[0] = (unsigned)elemA; c.elem[1] = (unsigned)elemB;
	c.critRate = critRate; c.critDmg = critDmg;
	c.traits[0] = traitsA ? traitsA : ""; c.traits[1] = traitsB ? traitsB : "";
}

API void wx_pool_clear() { g_pool.clear(); }

API int wx_pool_add(const char* path) {
	try {
		g_pool.push_back(make_shared<Cultivator>("T", path, 1000, 1200, 800, 21, 20));
	} catch (...) { return -1; }
	return (int)g_pool.size() - 1;
}

API int wx_pool_size() { return (int)g_pool.size(); }
API int wx_pool_moves(int idx) { return (int)g_pool[idx]->actions.size(); }

API void wx_init(int n) { g_games.clear(); g_games.resize(n); }

// stats: hp, atk, mp, agi, rec, agiLoss, agiRecover, bombMultiplier (8个)
API void wx_reset(int gid, int tplA, int tplB, const float* statsA, const float* statsB) {
	Game& g = g_games[gid];
	const int tpl[2] = { tplA, tplB };
	const float* st[2] = { statsA, statsB };
	for (int k = 0; k < 2; ++k) {
		g.c[k] = make_shared<Cultivator>(*g_pool[tpl[k]]);
		Cultivator& c = *g.c[k];
		c.maxHP = c.currentHP = (int)st[k][0];
		c.maxATK = c.currentATK = (int)st[k][1];
		c.maxMP = c.currentMP = (int)st[k][2];
		c.AGI = c.maxAGI = st[k][3];
		c.recover = (int)st[k][4];
		c.agiLoss = st[k][5];
		c.agiRecover = st[k][6];
		c.bombMultiplier = st[k][7];
		c.bomb = 0;
		g.hist[k].len = 0;
	}
	g.ct = make_unique<Controller>(vector<shared_ptr<Cultivator>>{ g.c[0], g.c[1] });
	if (g_pve) {
		for (int k = 0; k < 2; ++k) g.c[k]->SetElements(g.cfg.elem[k]);
		g.ct->SetCritBase(g.cfg.critRate, g.cfg.critDmg);
		for (int k = 0; k < 2; ++k) {
			istringstream lines(g.cfg.traits[k]);
			string line;
			while (getline(lines, line)) {
				Modifier m;
				if (ParseModifier(line, m)) g.ct->AddModifier(k, m);
			}
		}
		g.cfg = PveCfg();   // 配置只用一次，下一盘不设就是默认(无属性、无暴击、无特性)
	}
	g.round = 1;
	g.done = false;
	g.outcome = 0;
	BuildMoveCache(g);
	g.ct->StartTurn();
	g.ct->logs.clear(); g.ct->mark = 0;
}

API int wx_nmoves(int gid, int fighter) { return (int)g_games[gid].moveFeat[fighter].size(); }

// 取一个战斗者的全部招式特征：out是M*FM，form/fam是M；不足M的补零，form/fam补-2(表示不存在)。
API void wx_moves(int gid, int fighter, int M, float* out, int* form, int* fam) {
	Game& g = g_games[gid];
	int n = (int)g.moveFeat[fighter].size();
	memset(out, 0, sizeof(float) * M * gFM);
	for (int i = 0; i < M; ++i) {
		if (i < n) {
			memcpy(out + (size_t)i * gFM, g.moveFeat[fighter][i].data(), sizeof(float) * gFM);
			form[i] = g.form[fighter][i]; fam[i] = g.fam[fighter][i];
		} else { form[i] = -2; fam[i] = -2; }
	}
}

// 批量取局面+记录：state是n*2*SD，hist是n*2*HK*HD(最旧的在前，不足的在前面补零)，lenOut是n*2
API void wx_obs_batch(int n, const int* gids, float* state, float* hist, int* done) {
	ParallelFor(n, [&](int b) {
		Game& g = g_games[gids[b]];
		done[b] = g.done ? 1 : 0;
		for (int s = 0; s < 2; ++s) {
			FillState(g, s, state + ((size_t)b * 2 + s) * gSD);
			float* h = hist + (((size_t)b * 2 + s) * HK) * HD;
			memset(h, 0, sizeof(float) * HK * HD);
			int len = g.hist[s].len;
			for (int k = 0; k < len; ++k) memcpy(h + (size_t)(HK - len + k) * HD, g.hist[s].v[k], sizeof(float) * HD);
		}
	});
}

// act是n*2*3(cls, m1, m2)，reward是n*2，outcome是n(+1席位0胜/-1席位1胜/0)，done是n
API void wx_step_batch(int n, const int* gids, const int* act, float* reward, int* done, int* outcome) {
	ParallelFor(n, [&](int b) {
		Game& g = g_games[gids[b]];
		reward[b * 2] = reward[b * 2 + 1] = 0.f;
		if (g.done) { done[b] = 1; outcome[b] = g.outcome; return; }

		Cultivator& c0 = *g.c[0]; Cultivator& c1 = *g.c[1];
		float lossBefore[2] = { Loss(c0), Loss(c1) };
		float before[2][3] = { { (float)c0.currentHP / c0.maxHP, (float)c0.currentATK / c0.maxATK, (float)c0.currentMP / c0.maxMP },
			{ (float)c1.currentHP / c1.maxHP, (float)c1.currentATK / c1.maxATK, (float)c1.currentMP / c1.maxMP } };
		bool locked[2] = { (g.ct->status[0] & 1) != 0, (g.ct->status[1] & 1) != 0 };
		const int* a = act + (size_t)b * 6;
		int cls[2] = { a[0], a[3] };
		auto x = Build(c0, a[0], a[1], a[2]);
		auto y = Build(c1, a[3], a[4], a[5]);
		// 无效的招式(越界/不同式)Build返回跳过，记录里的类别也改成跳过
		if (cls[0] >= 3 && x->GetType() == ACTION_SKIP) cls[0] = 0;
		if (cls[1] >= 3 && y->GetType() == ACTION_SKIP) cls[1] = 0;

		g.ct->ActionTurn({ { x, 1 }, { y, 0 } });
		bool dodged[2] = { false, false };   // 席位s闪避了对手的攻击
		bool lockHit[2] = { false, false };  // 席位s禁锢了对手
		g.ct->CheckTurn({ { x, 1 }, { y, 0 } });
		g.ct->EndTurn();
		for (auto& log : g.ct->logs) {
			if (log.type == LOG_DODGE && log.object >= 0 && log.object < 2) dodged[log.object] = true;
			if (log.type == LOG_EFFECT && log.effect == EFFECT_LOCK && log.subject >= 0 && log.subject < 2) lockHit[log.subject] = true;
		}
		g.ct->logs.clear(); g.ct->mark = 0;

		float rec[2][24];
		ActionRecord(*x, cls[0], c0, rec[0]);
		ActionRecord(*y, cls[1], c1, rec[1]);
		Cultivator* cs[2] = { &c0, &c1 };
		for (int s = 0; s < 2; ++s) {
			int o = 1 - s;
			float row[HD]; memset(row, 0, sizeof(row));
			row[0] = 1.f;
			ActionRecord(s == 0 ? *x : *y, cls[s], *cs[s], row + 1);
			ActionRecord(s == 0 ? *y : *x, cls[o], *cs[o], row + 25);
			row[49] = Clamp((float)cs[s]->currentHP / cs[s]->maxHP - before[s][0]);
			row[50] = Clamp((float)cs[s]->currentATK / cs[s]->maxATK - before[s][1]);
			row[51] = Clamp((float)cs[s]->currentMP / cs[s]->maxMP - before[s][2]);
			row[52] = Clamp((float)cs[o]->currentHP / cs[o]->maxHP - before[o][0]);
			row[53] = Clamp((float)cs[o]->currentATK / cs[o]->maxATK - before[o][1]);
			row[54] = Clamp((float)cs[o]->currentMP / cs[o]->maxMP - before[o][2]);
			row[55] = dodged[s] ? 1.f : 0.f; row[56] = dodged[o] ? 1.f : 0.f;
			row[57] = locked[s] ? 1.f : 0.f;  row[58] = locked[o] ? 1.f : 0.f;
			Push(g.hist[s], row);
		}
		(void)lockHit;

		float lossAfter[2] = { Loss(c0), Loss(c1) };
		for (int s = 0; s < 2; ++s) reward[b * 2 + s] = (lossAfter[1 - s] - lossBefore[1 - s]) - (lossAfter[s] - lossBefore[s]);
		bool alive0 = c0.IsAlive(), alive1 = c1.IsAlive();
		if (!alive0 || !alive1) {
			g.done = true;
			g.outcome = (alive0 && !alive1) ? 1 : (!alive0 && alive1) ? -1 : 0;
			if (g.outcome != 0) { reward[b * 2] += (float)g.outcome; reward[b * 2 + 1] -= (float)g.outcome; }
			else { reward[b * 2] -= 1.f; reward[b * 2 + 1] -= 1.f; }   // 同归于尽：双方都算输
		}
		else if (g.round >= MAX_ROUNDS) {
			// 回合数到上限：和输掉的惩罚一样，双方都算输(防止双方拖时间不分胜负)
			g.done = true; g.outcome = 0;
			reward[b * 2] -= 1.f; reward[b * 2 + 1] -= 1.f;
		}
		else {
			++g.round;
			g.ct->StartTurn();
			g.ct->logs.clear(); g.ct->mark = 0;
		}
		done[b] = g.done ? 1 : 0;
		outcome[b] = g.outcome;
	});
}
