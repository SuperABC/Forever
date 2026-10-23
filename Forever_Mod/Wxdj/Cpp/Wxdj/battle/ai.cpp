#include "ai.h"

#include <algorithm>
#include <fstream>


using namespace std;

namespace wxdj {

PolicyAI::PolicyAI() : rng(random_device{}()) {

}

bool PolicyAI::Load(const string& file, int count) {
	ifstream in(file);
	if (!in) return false;

	int fileFamilies = 0;
	if (!(in >> fileFamilies) || fileFamilies != count) return false;
	familyCount = count;

	classProb.assign(STATE_COUNT, array<double, CLASS_COUNT>());
	for (auto& row : classProb) {
		for (double& p : row) {
			if (!(in >> p)) return false;
		}
	}
	singleProb.assign(CONTEXT_COUNT, vector<double>(familyCount));
	dualProb = singleProb;
	for (auto& row : singleProb) {
		for (double& p : row) {
			if (!(in >> p)) return false;
		}
	}
	for (auto& row : dualProb) {
		for (double& p : row) {
			if (!(in >> p)) return false;
		}
	}
	return true;
}

void PolicyAI::Reset() {
	opponentAccumulate = 0;
	opponentAttribute = 7;
}

void PolicyAI::Observe(const Action& opponentAction) {
	if (opponentAction.GetType() == ACTION_ACCUMULATE) ++opponentAccumulate;
	else if (opponentAction.GetType() == ACTION_BOMB) opponentAccumulate = 0;

	if (opponentAction.GetType() == ACTION_SINGLE || opponentAction.GetType() == ACTION_DUAL)
		opponentAttribute = static_cast<int>(opponentAction.GetAttribute());
	else
		opponentAttribute = 7;
}

// 真气档位：占上限的比例分成4档
static int MpBucket(const Cultivator& me) {
	double ratio = static_cast<double>(me.GetCurrentMP()) / me.GetMaxMP();
	return ratio < 0.088 ? 0 : ratio < 0.294 ? 1 : ratio < 0.588 ? 2 : 3;
}

// 活力档位：健康和武力占比里较小的那个，分成3档
static int VitalityBucket(const Cultivator& c) {
	double hp = static_cast<double>(c.GetCurrentHP()) / c.GetMaxHP();
	double atk = static_cast<double>(c.GetCurrentATK()) / c.GetMaxATK();
	double v = min(hp, atk);
	return v > 0.66 ? 0 : v > 0.33 ? 1 : 2;
}

int PolicyAI::StateOf(const Cultivator& me, const Cultivator& opponent) const {
	int bomb = me.GetBomb() <= 0 ? 0 : static_cast<double>(me.GetBomb()) / me.GetMaxMP() <= 0.235 ? 1 : 2;
	return (((MpBucket(me) * 3 + bomb) * 3 + VitalityBucket(me)) * 3 + VitalityBucket(opponent)) * 3
		+ min(opponentAccumulate, 2);
}

int PolicyAI::ContextOf(const Cultivator& me) const {
	return MpBucket(me) * 8 + opponentAttribute;
}

int PolicyAI::PickFrom(const double* weights, int count) {
	double sum = 0;
	for (int i = 0; i < count; ++i) sum += weights[i];
	double r = uniform_real_distribution<double>(0, sum)(rng);
	for (int i = 0; i < count; ++i) {
		r -= weights[i];
		if (r < 0) return i;
	}
	return count - 1;
}

shared_ptr<Action> PolicyAI::Choose(Cultivator& me, const Cultivator& opponent, const vector<int>& familyActions) {
	if (classProb.empty() || static_cast<int>(familyActions.size()) != familyCount)
		return make_shared<SkipAction>();

	int cls = PickFrom(classProb[StateOf(me, opponent)].data(), CLASS_COUNT);
	int ctx = ContextOf(me);
	switch (cls) {
	case 0:
		return make_shared<SkipAction>();
	case 1:
		return make_shared<AccumulateAction>();
	case 2:
		return make_shared<BombAction>();
	case 3:
		return me.GetAction(familyActions[PickFrom(singleProb[ctx].data(), familyCount)]);
	default: {
		int first = PickFrom(dualProb[ctx].data(), familyCount);
		int second = PickFrom(dualProb[ctx].data(), familyCount);
		return me.GetAction(familyActions[first], familyActions[second]);
	}
	}
}

}
