#include "modifier.h"

#include "cultivator.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>


using namespace std;

namespace wxdj {

static const char* const KIND_NAMES[MOD_KIND_COUNT] = {
	"mp_round", "atk_round", "hp_round",
	"skip_agi", "agi_recover", "agi_max", "mp_max",
	"acc_ratio", "bomb_power",
	"base_power", "dmg_dealt", "dmg_taken", "field_mult", "crit_rate", "crit_dmg", "dodge", "hit", "cost",
	"deal_mp", "deal_atk", "deal_hp", "take_mp", "take_atk", "take_hp", "rebound", "immune",
	"now_mp", "now_atk", "now_hp", "now_agi", "now_field", "clear_field", "unlock", "now_shield",
	"resist", "elem_power",
};

const char* ModKindName(MOD_KIND kind) {
	return (kind >= 0 && kind < MOD_KIND_COUNT) ? KIND_NAMES[kind] : "";
}

void ModifierSet::Add(const Modifier& m) {
	if (m.kind == MOD_DMG_TAKEN && m.value < 0) return;   // 易伤只增不减：减伤只来自减伤效果，保证受到伤害乘区不会变成0
	mods.push_back(m);
}

void ModifierSet::RemoveBySource(const string& source) {
	mods.erase(remove_if(mods.begin(), mods.end(), [&](const Modifier& m) { return m.source == source; }), mods.end());
}

void ModifierSet::Clear() {
	mods.clear();
}

const vector<Modifier>& ModifierSet::All() const {
	return mods;
}

bool ModifierSet::CondHolds(const ModCond& cond, const Cultivator& holder) {
	if (cond.stat == MOD_STAT_NONE) return true;
	double cur = 0, max = 1;
	switch (cond.stat) {
	case MOD_STAT_HP: cur = holder.GetCurrentHP(); max = holder.GetMaxHP(); break;
	case MOD_STAT_ATK: cur = holder.GetCurrentATK(); max = holder.GetMaxATK(); break;
	case MOD_STAT_MP: cur = holder.GetCurrentMP(); max = holder.GetBaseMaxMP(); break;
	case MOD_STAT_AGI: cur = holder.GetAGI(); max = holder.GetBaseMaxAGI(); break;
	default: break;
	}
	if (max <= 0) max = 1;
	double pct = cur / max * 100.0;
	return cond.less ? pct < cond.pct : pct > cond.pct;
}

double ModifierSet::Sum(MOD_KIND kind, ATTRIBUTE_TYPE attr, const Cultivator& holder) const {
	double total = 0;
	for (const auto& m : mods) {
		if (m.kind != kind) continue;
		if (!(m.attrMask & (1u << static_cast<int>(attr)))) continue;
		if (!CondHolds(m.cond, holder)) continue;
		total += m.value;
	}
	return total;
}

bool ModifierSet::Any(MOD_KIND kind, ATTRIBUTE_TYPE attr, const Cultivator& holder) const {
	for (const auto& m : mods) {
		if (m.kind != kind) continue;
		if (!(m.attrMask & (1u << static_cast<int>(attr)))) continue;
		if (!CondHolds(m.cond, holder)) continue;
		return true;
	}
	return false;
}

void ModifierSet::Tick() {
	for (auto& m : mods) {
		if (m.duration > 0) --m.duration;
	}
	mods.erase(remove_if(mods.begin(), mods.end(), [](const Modifier& m) { return m.duration == 0; }), mods.end());
}

static bool ParseAttrList(const string& text, unsigned& mask) {
	mask = 0;
	stringstream ss(text);
	string item;
	while (getline(ss, item, ',')) {
		if (item == "none") mask |= 1u << ATTRIBUTE_NONE;
		else if (item == "metal") mask |= 1u << ATTRIBUTE_METAL;
		else if (item == "wood") mask |= 1u << ATTRIBUTE_WOOD;
		else if (item == "water") mask |= 1u << ATTRIBUTE_WATER;
		else if (item == "fire") mask |= 1u << ATTRIBUTE_FIRE;
		else if (item == "earth") mask |= 1u << ATTRIBUTE_EARTH;
		else if (item == "all") mask |= 1u << ATTRIBUTE_ALL;
		else if (item == "elements") mask |= (1u << ATTRIBUTE_METAL) | (1u << ATTRIBUTE_WOOD) | (1u << ATTRIBUTE_WATER) | (1u << ATTRIBUTE_FIRE) | (1u << ATTRIBUTE_EARTH);
		else if (item == "any") mask |= MOD_ATTR_ALL;
		else return false;
	}
	return mask != 0;
}

bool ParseModifier(const string& line, Modifier& out) {
	istringstream iss(line);
	string token;
	if (!(iss >> token)) return false;
	if (token == "mod") { if (!(iss >> token)) return false; }
	int kind = -1;
	for (int k = 0; k < MOD_KIND_COUNT; ++k) if (token == KIND_NAMES[k]) { kind = k; break; }
	if (kind < 0) return false;

	Modifier m;
	m.kind = static_cast<MOD_KIND>(kind);
	if (!(iss >> token)) {
		// 没有数值的种类(清除场地、解除禁锢)
		if (m.kind != MOD_CLEAR_FIELD && m.kind != MOD_UNLOCK) return false;
		out = m;
		return true;
	}
	if (m.kind == MOD_CLEAR_FIELD || m.kind == MOD_UNLOCK) {
		// 数值被忽略，后面的token当参数继续解析
	}
	else {
		if (!token.empty() && token.back() == '%') { m.percent = true; token.pop_back(); }
		char* end = nullptr;
		m.value = strtod(token.c_str(), &end);
		if (end == token.c_str() || *end != '\0') return false;
		if (m.kind == MOD_DMG_TAKEN && m.value < 0) return false;   // 易伤只增不减
		token.clear();
	}
	auto handle = [&](const string& t) -> bool {
		if (t == "%") { m.percent = true; return true; }
		if (t.rfind("attr=", 0) == 0) return ParseAttrList(t.substr(5), m.attrMask);
		if (t.rfind("dur=", 0) == 0) { m.duration = atoi(t.c_str() + 4); return m.duration != 0; }
		if (t.rfind("timing=", 0) == 0) {
			string v = t.substr(7);
			if (v == "start") m.timing = MOD_AT_START; else if (v == "end") m.timing = MOD_AT_END; else return false;
			return true;
		}
		if (t.rfind("src=", 0) == 0) { m.source = t.substr(4); return true; }
		if (t.rfind("cond=", 0) == 0) {
			string v = t.substr(5);
			size_t pos = v.find_first_of("<>");
			if (pos == string::npos || pos == 0) return false;
			string stat = v.substr(0, pos);
			if (stat == "hp") m.cond.stat = MOD_STAT_HP;
			else if (stat == "atk") m.cond.stat = MOD_STAT_ATK;
			else if (stat == "mp") m.cond.stat = MOD_STAT_MP;
			else if (stat == "agi") m.cond.stat = MOD_STAT_AGI;
			else return false;
			m.cond.less = v[pos] == '<';
			m.cond.pct = atof(v.c_str() + pos + 1);
			return true;
		}
		return false;
	};
	if (!token.empty() && !handle(token)) return false;
	while (iss >> token) {
		if (!handle(token)) return false;
	}
	out = m;
	return true;
}

}
