#include "cultivator.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>


using namespace std;

namespace wxdj {

Cultivator::Cultivator(const string& name, const string& file,
	int hp, int atk, int mp, int agi, int recover,
	float agiLoss, float agiRecover, float bombMultiplier) :
	name(name), maxHP(hp), currentHP(hp), maxATK(atk), currentATK(atk),
	maxMP(mp), currentMP(mp), AGI(static_cast<float>(agi)), maxAGI(static_cast<float>(agi)), agiLoss(agiLoss), agiRecover(agiRecover), bombMultiplier(bombMultiplier),
	currentRealm(YELLOW_INIT), recover(recover), bomb(0) {
	if (!LoadActions(file)) {
		throw runtime_error("Failed to load actions from file " + file + "\n");
	}
}

string Cultivator::GetName() const {
	return name;
}

int Cultivator::GetCurrentHP() const {
	return currentHP;
}

int Cultivator::GetCurrentATK() const {
	return currentATK;
}

int Cultivator::GetCurrentMP() const {
	return currentMP;
}

int Cultivator::GetMaxHP() const {
	return maxHP;
}

int Cultivator::GetMaxATK() const {
	return maxATK;
}

// 特性里"可以按基础值百分比"的数值：percent的按base*value/100，否则直接加(忽略属性掩码，条件照常判断)
static double ScaledSum(const Cultivator& c, MOD_KIND kind, double base) {
	double total = 0;
	for (const auto& m : c.Mods().All()) {
		if (m.kind != kind || !ModifierSet::CondHolds(m.cond, c)) continue;
		total += m.percent ? base * m.value / 100.0 : m.value;
	}
	return total;
}

int Cultivator::GetMaxMP() const {
	if (modSet.All().empty()) return maxMP;
	double adj = ScaledSum(*this, MOD_MP_MAX, maxMP);
	return adj == 0 ? maxMP : max(1, static_cast<int>(maxMP + adj));
}

int Cultivator::GetBaseMaxMP() const {
	return maxMP;
}

float Cultivator::GetBaseMaxAGI() const {
	return maxAGI;
}

unsigned Cultivator::GetElements() const {
	return elements;
}

void Cultivator::SetElements(unsigned mask) {
	elements = mask & ELEMENT_ALL_MASK;
}

bool Cultivator::HasElement(ATTRIBUTE_TYPE attr) const {
	return ElementCovers(elements, attr);
}

double Cultivator::Resist(ATTRIBUTE_TYPE attr) const {
	if (attr == ATTRIBUTE_NONE) return 1.0;
	auto one = [&](ATTRIBUTE_TYPE a) {
		double x = (elements & ElementBit(a) ? ELEM_RESIST_OWNED : 1.0);
		if (!modSet.All().empty()) x += modSet.Sum(MOD_RESIST, a, *this);
		return max(0.0, x);
	};
	if (attr == ATTRIBUTE_ALL) {
		if (!ElementCovers(elements, ATTRIBUTE_ALL)) return 1.0;
		double total = 0;
		for (int a = ATTRIBUTE_METAL; a <= ATTRIBUTE_EARTH; ++a) total += one(static_cast<ATTRIBUTE_TYPE>(a));
		return total / 5.0;
	}
	return one(attr);
}

double Cultivator::ElemPowerMul(ATTRIBUTE_TYPE attr) const {
	double x = ELEM_POWER_BASE;
	if (!modSet.All().empty()) x += modSet.Sum(MOD_ELEM_POWER, attr, *this);
	return max(0.0, x);
}

ModifierSet& Cultivator::Mods() {
	return modSet;
}

const ModifierSet& Cultivator::Mods() const {
	return modSet;
}

void Cultivator::AdjustMP(int delta) {
	if (delta > 0) RecoverMP(delta);
	else if (delta < 0) currentMP = max(0, currentMP + delta);
}

void Cultivator::AdjustATK(int delta) {
	if (delta > 0) RecoverATK(delta);
	else if (delta < 0) currentATK += delta;
}

void Cultivator::AdjustHP(int delta) {
	if (delta > 0) RecoverHP(delta);
	else if (delta < 0) currentHP = max(0, currentHP + delta);
}

void Cultivator::AdjustAGI(float delta) {
	AGI = max(1.f, min(GetMaxAGI(), AGI + delta));
}

int Cultivator::GetBomb() const {
	return bomb;
}

int Cultivator::GetRecover() const {
	return recover;
}

float Cultivator::GetBombMultiplier() const {
	return bombMultiplier;
}

float Cultivator::GetAGI() const {
	return AGI;
}

float Cultivator::GetMaxAGI() const {
	if (modSet.All().empty()) return maxAGI;
	double adj = ScaledSum(*this, MOD_AGI_MAX, maxAGI);
	return adj == 0 ? maxAGI : max(1.f, static_cast<float>(maxAGI + adj));
}

float Cultivator::GetAGILoss() const {
	return agiLoss;
}

float Cultivator::GetAGIRecover() const {
	return agiRecover;
}

bool Cultivator::CanDodge() const {
	return AGI - SkipLoss() >= 1.f - 1e-4f;
}

void Cultivator::RecoverAGI() {
	float rec = agiRecover;
	if (!modSet.All().empty()) {
		double adj = ScaledSum(*this, MOD_AGI_RECOVER, 0);
		if (adj != 0) rec = std::max(0.f, static_cast<float>(rec + adj));
	}
	AGI = std::min(GetMaxAGI(), AGI + rec);
}

int Cultivator::GetCurrentRealm() const {
	return currentRealm;
}

const vector<SingleAction>& Cultivator::GetActions() const {
	return actions;
}

shared_ptr<Action> Cultivator::GetAction(int idx) {
	// 索引范围：0 ~ actions.size()-1 为单招，actions.size() ~ actions.size()+2 为积攒，actions.size()+3 为炸弹，其余为跳过
	if (idx < 0 || idx >= static_cast<int>(actions.size()) + 4)
		return make_shared<SkipAction>();
	if (idx >= static_cast<int>(actions.size()) && idx <= static_cast<int>(actions.size()) + 2)
		return make_shared<AccumulateAction>();
	if (idx == static_cast<int>(actions.size()) + 3)
		return make_shared<BombAction>();
	return make_shared<SingleAction>(actions[idx]);
}

shared_ptr<Action> Cultivator::GetAction(string name) {
	for (auto& action : actions) {
		if (action.GetName() == name)
			return make_shared<SingleAction>(action);
	}
	return make_shared<SkipAction>();
}

// 双招的两招必须同式：招式名是"家族名+第X式"，比较"第X式"部分；名字里没有"第"的招式不受限制。
// 否则可以拿高式招式搭配低式的其它属性招式，便宜地改变高式招式的属性。
static bool SameForm(const SingleAction& a, const SingleAction& b) {
	string name1 = a.GetName(), name2 = b.GetName();
	size_t pos1 = name1.find("第"), pos2 = name2.find("第");
	if (pos1 == string::npos || pos2 == string::npos) return true;
	return name1.substr(pos1) == name2.substr(pos2);
}

shared_ptr<Action> Cultivator::GetAction(int idx1, int idx2) {
	if (idx1 < 0 || idx1 >= static_cast<int>(actions.size()) ||
		idx2 < 0 || idx2 >= static_cast<int>(actions.size()))
		return make_shared<SkipAction>();
	if (!SameForm(actions[idx1], actions[idx2])) return make_shared<SkipAction>();
	auto a1 = make_shared<SingleAction>(actions[idx1]);
	auto a2 = make_shared<SingleAction>(actions[idx2]);
	return make_shared<DualAction>(a1, a2);
}

shared_ptr<Action> Cultivator::GetAction(string name1, string name2) {
	shared_ptr<SingleAction> a1 = nullptr, a2 = nullptr;
	for (auto& action : actions) {
		if (action.GetName() == name1) a1 = make_shared<SingleAction>(action);
		if (action.GetName() == name2) a2 = make_shared<SingleAction>(action);
	}
	if (a1 && a2 && SameForm(*a1, *a2)) return make_shared<DualAction>(a1, a2);
	return make_shared<SkipAction>();
}

bool Cultivator::IsAlive() const {
	return currentHP > 0 && currentATK > 0;
}

bool Cultivator::ValidateAction(const shared_ptr<Action> action) const {
	return action->GetPoint() <= currentMP;
}

void Cultivator::RecoverMP(int amount) {
	if (amount <= 0) {
		currentMP += recover;
	}
	else {
		currentMP += amount;
	}
	int cap = GetMaxMP();
	if (currentMP > cap * 2) {
		currentHP = 0;
	}
	else if (currentMP > cap) {
		currentMP = cap;
	}
}

void Cultivator::UndoMP(int amount) {
	currentMP = max(0, currentMP - amount);
}

void Cultivator::ConsumeMP(int amount) {
	if (currentMP >= amount) {
		currentMP -= amount;
	}
	else {
		int deficit = amount - currentMP;
		currentMP = 0;
		currentATK = max(0, currentATK - deficit * 2);
	}
}

void Cultivator::RecoverATK(int amount) {
	currentATK = min(maxATK, currentATK + amount);
}

void Cultivator::RecoverHP(int amount) {
	currentHP = min(maxHP, currentHP + amount);
}

void Cultivator::TakeDamage(int amount, bool physical) {
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

void Cultivator::DodgeSuccess() {

}

void Cultivator::ConsumeAGI() {
	AGI = std::max(1.f, AGI - SkipLoss());
	++skipStreak;
}

float Cultivator::SkipLoss() const {
	float loss = agiLoss + SKIP_STREAK_EXTRA_LOSS * skipStreak;
	if (!modSet.All().empty()) {
		double adj = ScaledSum(*this, MOD_SKIP_AGI, 0);
		if (adj != 0) loss = std::max(0.f, static_cast<float>(loss + adj));
	}
	return loss;
}

void Cultivator::ResetSkipStreak() {
	skipStreak = 0;
}

int Cultivator::GetSkipStreak() const {
	return skipStreak;
}

int Cultivator::PeekChain(const string& family, int form) const {
	if (form <= 1) return 0;
	for (const auto& e : chain) {
		if (e.family == family && e.form == form - 1) return e.len + 1;
	}
	return 0;
}

void Cultivator::CommitChain(const vector<pair<string, int>>& moves) {
	vector<ChainEntry> next;
	for (const auto& m : moves) {
		next.push_back({ m.first, m.second, PeekChain(m.first, m.second) });
	}
	chain = next;
}

void Cultivator::ResetChain() {
	chain.clear();
}

const vector<Cultivator::ChainEntry>& Cultivator::GetChain() const {
	return chain;
}

void Cultivator::UpdateRealm() {
	for (const auto& req : REALM_ATK_REQUIREMENTS) {
		if (currentATK >= req.second) {
			currentRealm = req.first;
		}
		else {
			break;
		}
	}
}

void Cultivator::AccumulatePower() {
	// 消耗当前真气的一部分，按倍数存进炸弹
	float fraction = ACCUMULATE_FRACTION;
	if (!modSet.All().empty()) {
		double adj = ScaledSum(*this, MOD_ACC_RATIO, 0);
		if (adj != 0) fraction = static_cast<float>(std::min(1.0, std::max(0.0, fraction + adj)));
	}
	int spent = static_cast<int>(fraction * currentMP);
	currentMP -= spent;
	bomb += static_cast<int>(spent * bombMultiplier);
}

int Cultivator::BombPower() {
	int p = bomb;
	bomb = 0;
	return p;
}

bool Cultivator::LoadActions(const string& filename) {
	ifstream file(filename);
	if (!file.is_open()) return false;

	string line;
	while (getline(file, line)) {
		if (line.empty() || line[0] == '#') continue;

		istringstream iss(line);
		string name, attributeStr, realmStr;
		int mp, atk, numEffects;
		if (!(iss >> name >> attributeStr >> mp >> atk >> realmStr >> numEffects)) {
			continue;
		}

		ATTRIBUTE_TYPE attr = ParseAttribute(attributeStr);
		Realm realm = ParseRealm(realmStr);
		vector<shared_ptr<Effect>> effects;
		for (int i = 0; i < numEffects; ++i) {
			if (!getline(file, line)) break;
			auto effect = ParseEffect(line);
			if (effect) effects.push_back(effect);
		}

		actions.emplace_back(name, attr, mp, atk, realm, effects);
	}

	file.close();
	return true;
}

Realm Cultivator::ParseRealm(const string& realmStr) {
	size_t dash = realmStr.find('-');
	if (dash == string::npos) {
		return YELLOW_INIT;
	}
	int major = stoi(realmStr.substr(0, dash));
	int minor = stoi(realmStr.substr(dash + 1));
	int base = (major - 1) * 6;
	switch (minor) {
	case 1: return static_cast<Realm>(base + YELLOW_INIT);
	case 2: return static_cast<Realm>(base + YELLOW_INIT_PEAK);
	case 3: return static_cast<Realm>(base + YELLOW_MID);
	case 4: return static_cast<Realm>(base + YELLOW_MID_PEAK);
	case 5: return static_cast<Realm>(base + YELLOW_LATE);
	case 6: return static_cast<Realm>(base + YELLOW_LATE_PEAK);
	default:
		return YELLOW_INIT;
	}
}

ATTRIBUTE_TYPE Cultivator::ParseAttribute(const string& attrStr) {
	if (attrStr == "none") return ATTRIBUTE_NONE;
	if (attrStr == "metal") return ATTRIBUTE_METAL;
	if (attrStr == "wood") return ATTRIBUTE_WOOD;
	if (attrStr == "water") return ATTRIBUTE_WATER;
	if (attrStr == "fire") return ATTRIBUTE_FIRE;
	if (attrStr == "earth") return ATTRIBUTE_EARTH;
	if (attrStr == "all") return ATTRIBUTE_ALL;
	return ATTRIBUTE_NONE;
}

shared_ptr<Effect> Cultivator::ParseEffect(const string& effectLine) {
	istringstream iss(effectLine);
	string effectType;
	iss >> effectType;

	if (effectType == "physical") {
		// 带7个比例(对方属性：无/金/木/水/火/土/五行)，不带参数时等价于全部100%
		float none, metal, wood, water, fire, earth, all;
		if (iss >> none >> metal >> wood >> water >> fire >> earth >> all)
			return make_shared<PhysicalEffect>(none, metal, wood, water, fire, earth, all);
		return make_shared<PhysicalEffect>();
	}
	else if (effectType == "penetrate") {
		float none, metal, wood, water, fire, earth, all;
		if (iss >> none >> metal >> wood >> water >> fire >> earth >> all)
			return make_shared<PenetrateEffect>(none, metal, wood, water, fire, earth, all);
	}
	else if (effectType == "reduce") {
		float none, metal, wood, water, fire, earth, all;
		if (iss >> none >> metal >> wood >> water >> fire >> earth >> all)
			return make_shared<ReduceEffect>(none, metal, wood, water, fire, earth, all);
	}
	else if (effectType == "rebound") {
		float none, metal, wood, water, fire, earth, all;
		if (iss >> none >> metal >> wood >> water >> fire >> earth >> all)
			return make_shared<ReboundEffect>(none, metal, wood, water, fire, earth, all);
	}
	else if (effectType == "absorb") {
		float none, metal, wood, water, fire, earth, all;
		if (iss >> none >> metal >> wood >> water >> fire >> earth >> all)
			return make_shared<AbsorbEffect>(none, metal, wood, water, fire, earth, all);
	}
	else if (effectType == "lock") {
		float prob;
		if (iss >> prob) return make_shared<LockEffect>(prob);
	}
	else if (effectType == "dot") {
		int instant, continuous = 0, rounds = 0, interval = 1;
		if (iss >> instant >> continuous >> rounds >> interval)
			return make_shared<DotEffect>(instant, continuous, rounds, interval);
		else if (iss >> instant)
			return make_shared<DotEffect>(instant);
	}
	else if (effectType == "rebate") {
		int instant, continuous = 0, rounds = 0, interval = 1;
		if (iss >> instant >> continuous >> rounds >> interval)
			return make_shared<RebateEffect>(instant, continuous, rounds, interval);
		else if (iss >> instant)
			return make_shared<RebateEffect>(instant);
	}
	else if (effectType == "recover") {
		int instant, continuous = 0, rounds = 0, interval = 1;
		if (iss >> instant >> continuous >> rounds >> interval)
			return make_shared<RecoverEffect>(instant, continuous, rounds, interval);
		else if (iss >> instant)
			return make_shared<RecoverEffect>(instant);
	}
	else if (effectType == "cure") {
		int instant, continuous = 0, rounds = 0, interval = 1;
		if (iss >> instant >> continuous >> rounds >> interval)
			return make_shared<CureEffect>(instant, continuous, rounds, interval);
		else if (iss >> instant)
			return make_shared<CureEffect>(instant);
	}
	else if (effectType == "increase") {
		float ratio;
		if (iss >> ratio) return make_shared<IncreaseEffect>(ratio);
	}
	else if (effectType == "shield") {
		int strength, rounds;
		if (iss >> strength >> rounds) return make_shared<ShieldEffect>(strength, rounds);
	}
	return nullptr;
}

}
