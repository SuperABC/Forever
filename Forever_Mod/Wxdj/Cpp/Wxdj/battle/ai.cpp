#include "ai.h"

// 特征定义(招式/局面/出招记录)和训练端共用同一份头文件，保证两边完全一致
#include "../ai/transformer/features.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>

using namespace std;

namespace wxdj {

namespace {

const int FM = wxfeat::FM, SD = wxfeat::SD, HK = wxfeat::HK, HD = wxfeat::HD;
const int D = 128, HEADS = 4, LATENTS = 16, BLOCKS = 3;
const float NEG = -1e9f;

float Clamp(float v) { return wxfeat::Clamp(v); }

float Gelu(float v) { return 0.5f * v * (1.f + erff(v * 0.70710678118f)); }

void GeluInPlace(vector<float>& x) { for (auto& v : x) v = Gelu(v); }

vector<float> Add(const vector<float>& a, const vector<float>& b) {
	vector<float> y(a.size());
	for (size_t i = 0; i < a.size(); ++i) y[i] = a[i] + b[i];
	return y;
}

// y(n x out) = x(n x in) * W(out x in)^T + b
vector<float> Linear(const float* x, int n, int inD, const float* w, const float* b, int outD) {
	vector<float> y((size_t)n * outD);
	for (int r = 0; r < n; ++r) {
		const float* xr = x + (size_t)r * inD;
		float* yr = &y[(size_t)r * outD];
		for (int j = 0; j < outD; ++j) {
			float s = b ? b[j] : 0.f;
			const float* wj = w + (size_t)j * inD;
			for (int i = 0; i < inD; ++i) s += xr[i] * wj[i];
			yr[j] = s;
		}
	}
	return y;
}

vector<float> LayerNorm(const vector<float>& x, int n, int d, const float* w, const float* b) {
	vector<float> y(x.size());
	for (int r = 0; r < n; ++r) {
		const float* xr = &x[(size_t)r * d];
		float* yr = &y[(size_t)r * d];
		float m = 0.f, v = 0.f;
		for (int i = 0; i < d; ++i) m += xr[i];
		m /= d;
		for (int i = 0; i < d; ++i) { float c = xr[i] - m; v += c * c; }
		v /= d;
		float inv = 1.f / sqrtf(v + 1e-5f);
		for (int i = 0; i < d; ++i) yr[i] = (xr[i] - m) * inv * w[i] + b[i];
	}
	return y;
}

}  // namespace

AI::AI(int seat) : seat(seat), rng(random_device{}()) {
}

bool AI::Load(const string& file) {
	loaded = false;
	weights.clear();
	ifstream in(file, ios::binary);
	if (!in) return false;
	auto readInt = [&](int32_t& v) { in.read(reinterpret_cast<char*>(&v), 4); return (bool)in; };
	char magic[4];
	in.read(magic, 4);
	int32_t version = 0, count = 0;
	if (!in || memcmp(magic, "WXNN", 4) != 0 || !readInt(version) || version != 1 || !readInt(count) || count <= 0 || count > 10000) return false;
	for (int t = 0; t < count; ++t) {
		int32_t nameLen = 0, ndim = 0;
		if (!readInt(nameLen) || nameLen <= 0 || nameLen > 256) return false;
		string name(nameLen, '\0');
		in.read(&name[0], nameLen);
		if (!readInt(ndim) || ndim < 0 || ndim > 4) return false;
		Tensor tensor;
		size_t total = 1;
		for (int k = 0; k < ndim; ++k) {
			int32_t d = 0;
			if (!readInt(d) || d <= 0 || d > 1000000) return false;
			tensor.shape.push_back(d);
			total *= (size_t)d;
		}
		if (total > 50000000) return false;
		tensor.data.resize(total);
		in.read(reinterpret_cast<char*>(tensor.data.data()), (streamsize)(total * sizeof(float)));
		if (!in) return false;
		weights[name] = std::move(tensor);
	}
	// 结构对不上(特征维数、潜变量数、模型宽度)就拒绝加载，避免用错权重出怪招
	auto shapeIs = [&](const string& n, int a, int b) {
		auto it = weights.find(n);
		return it != weights.end() && it->second.shape.size() == 2 && it->second.shape[0] == a && it->second.shape[1] == b;
	};
	if (!shapeIs("latents", LATENTS, D) || !shapeIs("move_enc.net.0.weight", D, FM) || !shapeIs("state_enc.net.0.weight", D, SD) ||
		!shapeIs("hist_enc.net.0.weight", D, HD) || !shapeIs("hist_pos", HK, D) || !shapeIs("cls_head.net.2.weight", 5, D)) {
		weights.clear();
		return false;
	}
	loaded = true;
	Reset();
	return true;
}

void AI::Reset() {
	history.clear();
	snapshot.clear();
}

const vector<float>& AI::W(const string& name) const {
	auto it = weights.find(name);
	if (it == weights.end()) throw runtime_error("missing weight: " + name);
	return it->second.data;
}

vector<float> AI::Mlp(const vector<float>& x, int n, const string& prefix, int inD, int hD, int outD) const {
	auto h = Linear(x.data(), n, inD, W(prefix + ".net.0.weight").data(), W(prefix + ".net.0.bias").data(), hD);
	GeluInPlace(h);
	return Linear(h.data(), n, hD, W(prefix + ".net.2.weight").data(), W(prefix + ".net.2.bias").data(), outD);
}

vector<float> AI::Lin(const vector<float>& x, int n, const string& prefix, int inD, int outD) const {
	return Linear(x.data(), n, inD, W(prefix + ".weight").data(), W(prefix + ".bias").data(), outD);
}

vector<float> AI::Ln(const vector<float>& x, int n, const string& prefix) const {
	return LayerNorm(x, n, D, W(prefix + ".weight").data(), W(prefix + ".bias").data());
}

// nn.MultiheadAttention(batch_first)的前向：q(nq x D)，kv(nk x D)，kv里不含padding(只传有效的key)
vector<float> AI::Mha(const vector<float>& q, int nq, const vector<float>& kv, int nk, const string& prefix) const {
	const auto& inW = W(prefix + ".in_proj_weight");
	const auto& inB = W(prefix + ".in_proj_bias");
	auto q2 = Linear(q.data(), nq, D, inW.data(), inB.data(), D);
	auto k2 = Linear(kv.data(), nk, D, inW.data() + (size_t)D * D, inB.data() + D, D);
	auto v2 = Linear(kv.data(), nk, D, inW.data() + (size_t)2 * D * D, inB.data() + 2 * D, D);
	const int hd = D / HEADS;
	const float scale = 1.f / sqrtf((float)hd);
	vector<float> out((size_t)nq * D, 0.f), sc(nk);
	for (int h = 0; h < HEADS; ++h) {
		int ho = h * hd;
		for (int i = 0; i < nq; ++i) {
			float mx = -INFINITY;
			for (int j = 0; j < nk; ++j) {
				float s = 0.f;
				const float* qo = &q2[(size_t)i * D + ho];
				const float* ko = &k2[(size_t)j * D + ho];
				for (int c = 0; c < hd; ++c) s += qo[c] * ko[c];
				s *= scale;
				sc[j] = s;
				if (s > mx) mx = s;
			}
			float z = 0.f;
			for (int j = 0; j < nk; ++j) { sc[j] = expf(sc[j] - mx); z += sc[j]; }
			for (int j = 0; j < nk; ++j) {
				float p = sc[j] / z;
				const float* vo = &v2[(size_t)j * D + ho];
				float* oo = &out[(size_t)i * D + ho];
				for (int c = 0; c < hd; ++c) oo[c] += p * vo[c];
			}
		}
	}
	return Lin(out, nq, prefix + ".out_proj", D, D);
}

AI::Forward AI::Run(const vector<float>& state, const vector<float>& hist, const vector<char>& histValid,
	const vector<float>& msFeat, int nS, const vector<float>& moFeat, int nO) const {
	const auto& kind = W("kind");
	const auto& role = W("role");
	const auto& pos = W("hist_pos");

	auto st = Mlp(state, 1, "state_enc", SD, D, D);
	vector<float> stTok(D);
	for (int i = 0; i < D; ++i) stTok[i] = st[i] + kind[i];
	auto hs = Mlp(hist, HK, "hist_enc", HD, D, D);
	int nH = 0;
	for (char v : histValid) if (v) ++nH;

	auto embed = [&](const vector<float>& feat, int n, int roleIdx) {
		auto e = Mlp(feat, n, "move_enc", FM, D, D);
		for (int r = 0; r < n; ++r)
			for (int i = 0; i < D; ++i) e[(size_t)r * D + i] += kind[2 * D + i] + role[roleIdx * D + i];
		return e;
	};
	auto es = embed(msFeat, nS, 0);
	auto eo = embed(moFeat, nO, 1);

	// 有效的token：局面 + 有效的记录 + 自己招式 + 对手招式
	int nk = 1 + nH + nS + nO;
	vector<float> kv((size_t)nk * D);
	copy(stTok.begin(), stTok.end(), kv.begin());
	int p = 1;
	for (int k = 0; k < HK; ++k) {
		if (!histValid[k]) continue;
		for (int i = 0; i < D; ++i) kv[(size_t)p * D + i] = hs[(size_t)k * D + i] + pos[(size_t)k * D + i] + kind[D + i];
		++p;
	}
	copy(es.begin(), es.end(), kv.begin() + (size_t)p * D);
	p += nS;
	copy(eo.begin(), eo.end(), kv.begin() + (size_t)p * D);

	vector<float> lat = W("latents");
	for (int b = 0; b < BLOCKS; ++b) {
		string pre = "blocks." + to_string(b);
		auto kvn = Ln(kv, nk, pre + ".nkv");
		lat = Add(lat, Mha(Ln(lat, LATENTS, pre + ".n1"), LATENTS, kvn, nk, pre + ".cross"));
		auto x = Ln(lat, LATENTS, pre + ".n2");
		lat = Add(lat, Mha(x, LATENTS, x, LATENTS, pre + ".self_attn"));
		auto f = Lin(Ln(lat, LATENTS, pre + ".n3"), LATENTS, pre + ".ffn.0", D, 2 * D);
		GeluInPlace(f);
		lat = Add(lat, Lin(f, LATENTS, pre + ".ffn.2", 2 * D, D));
	}
	lat = Ln(lat, LATENTS, "out_norm");
	vector<float> h(D, 0.f);
	for (int r = 0; r < LATENTS; ++r)
		for (int i = 0; i < D; ++i) h[i] += lat[(size_t)r * D + i] / LATENTS;
	for (int i = 0; i < D; ++i) h[i] += stTok[i];           // 训练端: h = 潜变量均值 + (state_enc(state)+kind[0])

	// 招式读取潜变量，得到带上下文的表示
	auto q = Ln(es, nS, "ctx_norm_q");
	auto ckv = Ln(lat, LATENTS, "ctx_norm_kv");
	auto ctx = Add(es, Mha(q, nS, ckv, LATENTS, "ctx_attn"));
	auto f = Ln(ctx, nS, "ctx_ffn.0");
	f = Lin(f, nS, "ctx_ffn.1", D, 2 * D);
	GeluInPlace(f);
	ctx = Add(ctx, Lin(f, nS, "ctx_ffn.3", 2 * D, D));

	Forward out;
	out.cls = Mlp(h, 1, "cls_head", D, D, 5);
	auto k1 = Lin(ctx, nS, "k1", D, D);
	auto q1 = Lin(h, 1, "q1", D, D);
	out.first.assign(nS, 0.f);
	for (int m = 0; m < nS; ++m) {
		float s = 0.f;
		for (int i = 0; i < D; ++i) s += q1[i] * k1[(size_t)m * D + i];
		out.first[m] = s / sqrtf((float)D);
	}
	out.value = Mlp(h, 1, "value", D, D, 1)[0];
	out.h = std::move(h);
	out.ctx = std::move(ctx);
	return out;
}

vector<float> AI::SecondLogits(const vector<float>& h, const vector<float>& ctx, int nS, int pick,
	const vector<char>& allowed) const {
	vector<float> embIn(ctx.begin() + (size_t)pick * D, ctx.begin() + (size_t)(pick + 1) * D);
	auto pe = Lin(embIn, 1, "pick_emb", D, D);
	vector<float> x(D);
	for (int i = 0; i < D; ++i) x[i] = h[i] + pe[i];
	auto q = Lin(x, 1, "q2", D, D);
	auto k = Lin(ctx, nS, "k2", D, D);
	vector<float> l(nS);
	for (int m = 0; m < nS; ++m) {
		if (!allowed[m]) { l[m] = NEG; continue; }
		float s = 0.f;
		for (int i = 0; i < D; ++i) s += q[i] * k[(size_t)m * D + i];
		l[m] = s / sqrtf((float)D);
	}
	return l;
}

// 局面特征：先自己再对手，各20维，然后4个场地标志和回合数(照 env.cpp 的 FillState)
static void FillState(const Controller& ctl, int seat, int round, float* o) {
	memset(o, 0, sizeof(float) * SD);
	int i = 0;
	for (int k = 0; k < 2; ++k) {
		int idx = k == 0 ? seat : 1 - seat;
		auto c = ctl.GetCultivator(idx);
		float mhp = (float)max(1, c->GetMaxHP()), matk = (float)max(1, c->GetMaxATK()), mmp = (float)max(1, c->GetMaxMP());
		o[i++] = Clamp((float)c->GetCurrentHP() / mhp);
		o[i++] = Clamp((float)c->GetCurrentATK() / matk);
		o[i++] = Clamp((float)c->GetCurrentMP() / mmp);
		o[i++] = Clamp((float)c->GetBomb() / mmp);
		o[i++] = logf(mhp) / 12.f;
		o[i++] = logf(matk) / 12.f;
		o[i++] = logf(mmp) / 12.f;
		o[i++] = c->GetAGI() / 30.f;
		o[i++] = Clamp((float)c->GetRecover() / mmp);
		o[i++] = ctl.IsLocked(idx) ? 1.f : 0.f;
		float dot = 0, reb = 0, rec = 0, cure = 0;
		for (const auto& e : ctl.GetEffectsOf(idx)) {
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
		o[i++] = c->GetMaxAGI() / 30.f;
		o[i++] = c->GetAGILoss() / 5.f;
		o[i++] = c->GetAGIRecover() / 2.f;
		o[i++] = c->GetBombMultiplier() / 10.f;
	}
	for (const auto& e : ctl.GetEnvironmentEffects()) {
		switch (e->GetType()) {
		case EFFECT_BURN: o[i] = 1.f; break;
		case EFFECT_RUST: o[i + 1] = 1.f; break;
		case EFFECT_DUST: o[i + 2] = 1.f; break;
		case EFFECT_MUD: o[i + 3] = 1.f; break;
		default: break;
		}
	}
	i += 4;
	o[i++] = round / 40.f;
	wxfeat::StateExtras(ctl, seat, o + SD - 8);
}

// 出招记录里的一方：类别/属性/消耗/威力/效果种类(照 env.cpp 的 ActionRecord)
static void ActionRecord(const Action& a, const Cultivator& c, float* o) {
	memset(o, 0, sizeof(float) * 24);
	int cls = static_cast<int>(a.GetType());
	if (cls >= 0 && cls < 5) o[cls] = 1.f;
	if (cls >= 3) {
		int attr = static_cast<int>(a.GetAttribute());
		if (attr >= 0 && attr < 7) o[5 + attr] = 1.f;
	}
	o[12] = Clamp((float)a.GetPoint() / max(1, c.GetMaxMP()));
	o[13] = Clamp((float)a.GetPower() / max(1, c.GetMaxATK()));
	static const EFFECT_TYPE types[10] = { EFFECT_PHYSICAL, EFFECT_PENETRATE, EFFECT_REDUCE, EFFECT_REBOUND, EFFECT_ABSORB,
		EFFECT_LOCK, EFFECT_DOT, EFFECT_REBATE, EFFECT_RECOVER, EFFECT_CURE };
	for (int k = 0; k < 10; ++k) if (a.GetEffect(types[k])) o[14 + k] = 1.f;
}

void AI::BeginRound(const Controller& controller) {
	snapshot.clear();
	for (int idx = 0; idx < 2; ++idx) {
		auto c = controller.GetCultivator(idx);
		snapshot.push_back({ (float)c->GetCurrentHP() / c->GetMaxHP(), (float)c->GetCurrentATK() / c->GetMaxATK(),
			(float)c->GetCurrentMP() / c->GetMaxMP(), controller.IsLocked(idx) });
	}
}

void AI::EndRound(const Controller& controller, const vector<shared_ptr<Action>>& actions, const vector<Log>& logs) {
	if (snapshot.size() != 2 || actions.size() != 2) return;
	const int s = seat, o = 1 - seat;
	bool dodged[2] = { false, false };
	for (const auto& lg : logs)
		if (lg.type == LOG_DODGE && lg.object >= 0 && lg.object < 2) dodged[lg.object] = true;
	vector<float> row(HD, 0.f);
	row[0] = 1.f;
	ActionRecord(*actions[s], *controller.GetCultivator(s), &row[1]);
	ActionRecord(*actions[o], *controller.GetCultivator(o), &row[25]);
	auto now = [&](int idx, float* r) {
		auto c = controller.GetCultivator(idx);
		r[0] = (float)c->GetCurrentHP() / c->GetMaxHP();
		r[1] = (float)c->GetCurrentATK() / c->GetMaxATK();
		r[2] = (float)c->GetCurrentMP() / c->GetMaxMP();
	};
	float ns[3], no[3];
	now(s, ns);
	now(o, no);
	const float* ss[3] = { &snapshot[s].hp, &snapshot[s].atk, &snapshot[s].mp };
	const float* so[3] = { &snapshot[o].hp, &snapshot[o].atk, &snapshot[o].mp };
	for (int k = 0; k < 3; ++k) {
		row[49 + k] = Clamp(ns[k] - *ss[k]);
		row[52 + k] = Clamp(no[k] - *so[k]);
	}
	row[55] = dodged[s] ? 1.f : 0.f;
	row[56] = dodged[o] ? 1.f : 0.f;
	row[57] = snapshot[s].locked ? 1.f : 0.f;
	row[58] = snapshot[o].locked ? 1.f : 0.f;
	history.push_back(std::move(row));
	if ((int)history.size() > HK) history.erase(history.begin());
	snapshot.clear();
}

void AI::BuildInputs(Controller& controller, int round, const vector<int>& moves, vector<float>& state, vector<float>& hist,
	vector<char>& histValid, vector<float>& msFeat, vector<float>& moFeat) const {
	auto me = controller.GetCultivator(seat);
	auto opp = controller.GetCultivator(1 - seat);
	const auto& actions = me->GetActions();
	const int n = (int)moves.size();
	msFeat.assign((size_t)n * FM, 0.f);
	moFeat.assign((size_t)n * FM, 0.f);
	for (int k = 0; k < n; ++k) {
		SingleAction a = actions[moves[k]];     // 特征函数要非const引用，复制一份
		wxfeat::MoveFeatures(a, *me, *opp, &msFeat[(size_t)k * FM]);
		wxfeat::MoveFeatures(a, *opp, *me, &moFeat[(size_t)k * FM]);
	}
	hist.assign((size_t)HK * HD, 0.f);
	histValid.assign(HK, 0);
	int off = HK - (int)history.size();
	for (int k = 0; k < (int)history.size(); ++k) {
		copy(history[k].begin(), history[k].end(), hist.begin() + (size_t)(off + k) * HD);
		histValid[off + k] = 1;
	}
	state.assign(SD, 0.f);
	FillState(controller, seat, round, state.data());
}

int AI::Sample(const vector<float>& logits, bool greedy) {
	if (greedy) return (int)(max_element(logits.begin(), logits.end()) - logits.begin());
	float mx = *max_element(logits.begin(), logits.end());
	vector<double> p(logits.size());
	double z = 0.0;
	for (size_t i = 0; i < logits.size(); ++i) { p[i] = exp((double)logits[i] - mx); z += p[i]; }
	double r = uniform_real_distribution<double>(0.0, 1.0)(rng) * z;
	for (size_t i = 0; i < p.size(); ++i) {
		r -= p[i];
		if (r < 0) return (int)i;
	}
	return (int)p.size() - 1;
}

AI::Output AI::Evaluate(Controller& controller, int round, const vector<int>& moves) {
	Output out;
	if (!loaded || moves.empty()) return out;
	vector<float> state, hist, msFeat, moFeat;
	vector<char> histValid;
	BuildInputs(controller, round, moves, state, hist, histValid, msFeat, moFeat);
	auto f = Run(state, hist, histValid, msFeat, (int)moves.size(), moFeat, (int)moves.size());
	out.cls = f.cls;
	out.first = f.first;
	out.value = f.value;
	return out;
}

shared_ptr<Action> AI::Choose(Controller& controller, int round, const vector<int>& moves, bool greedy) {
	auto me = controller.GetCultivator(seat);
	if (!loaded || moves.empty()) return nullptr;
	vector<float> state, hist, msFeat, moFeat;
	vector<char> histValid;
	BuildInputs(controller, round, moves, state, hist, histValid, msFeat, moFeat);
	const int n = (int)moves.size();
	Forward f;
	try {
		f = Run(state, hist, histValid, msFeat, n, moFeat, n);
	}
	catch (const exception&) {
		loaded = false;      // 权重缺项等：停用AI，调用方收到空指针后退回别的出招方式
		return nullptr;
	}

	int cls = Sample(f.cls, greedy);
	if (cls == 0) return make_shared<SkipAction>();
	if (cls == 1) return make_shared<AccumulateAction>();
	if (cls == 2) return make_shared<BombAction>();
	int m1 = Sample(f.first, greedy);
	if (cls == 3) return me->GetAction(moves[m1]);

	// 双招：第二招必须和第一招同式(招式名里没有"第X式"的不受限制)
	const auto& actions = me->GetActions();
	string key1 = wxfeat::FormKey(actions[moves[m1]].GetName());
	vector<char> allowed(n, 0);
	for (int m = 0; m < n; ++m) {
		string key = wxfeat::FormKey(actions[moves[m]].GetName());
		allowed[m] = key1.empty() || key.empty() || key == key1;
	}
	int m2 = Sample(SecondLogits(f.h, f.ctx, n, m1, allowed), greedy);
	return me->GetAction(moves[m1], moves[m2]);
}

}
