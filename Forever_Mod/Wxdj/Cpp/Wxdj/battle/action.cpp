#include "action.h"


using namespace std;

namespace wxdj {

vector<string> Action::attributeText = {
	"无属性", "金属性", "木属性", "水属性", "火属性", "土属性", "五行属性"
};

Action::Action(ACTION_TYPE type, ATTRIBUTE_TYPE attribute, int point, int power,
	vector<shared_ptr<Effect>> effects) :
	type(type), attribute(attribute), point(point), power(power), basePower(power), effects(effects) {

}

Action::~Action() {

}

ACTION_TYPE Action::GetType() const {
	return type;
}

ATTRIBUTE_TYPE Action::GetAttribute() const {
	return attribute;
}

int Action::GetPoint() const {
	return point;
}

int Action::GetPower() const {
	return power;
}

int Action::GetBasePower() const {
	return basePower;
}

shared_ptr<Effect> Action::GetEffect(EFFECT_TYPE type) const {
	for (auto& effect : effects) {
		if (effect->GetType() == type) return effect;
	}
	return nullptr;
}

vector<shared_ptr<Effect>>& Action::GetEffects() {
	return effects;
}

SkipAction::SkipAction() : Action(ACTION_SKIP, ATTRIBUTE_NONE, 0, 0, {}) {

}

AccumulateAction::AccumulateAction() : Action(ACTION_ACCUMULATE, ATTRIBUTE_NONE, 0, 0, {}) {

}

BombAction::BombAction() : Action(ACTION_BOMB, ATTRIBUTE_NONE, 0, 0, {}) {
	effects.push_back(make_shared<PhysicalEffect>());
}

void Action::SetPower(int p) {
	power = p;
}

SingleAction::SingleAction(const string& name, ATTRIBUTE_TYPE attribute, int point, int power, Realm realm,
	vector<shared_ptr<Effect>> effects) :
	name(name), realm(realm), Action(ACTION_SINGLE, attribute, point, power, effects) {

}

SingleAction::SingleAction(const SingleAction& other)
	: Action(other.type, other.attribute, other.point, other.basePower, {}) {
	name = other.name;
	realm = other.realm;

	// 深拷贝每个效果
	for (auto& effect : other.effects) {
		switch (effect->GetType()) {
		case EFFECT_PHYSICAL:
			effects.push_back(make_shared<PhysicalEffect>(*dynamic_pointer_cast<PhysicalEffect>(effect)));
			break;
		case EFFECT_PENETRATE:
			effects.push_back(make_shared<PenetrateEffect>(*dynamic_pointer_cast<PenetrateEffect>(effect)));
			break;
		case EFFECT_REDUCE:
			effects.push_back(make_shared<ReduceEffect>(*dynamic_pointer_cast<ReduceEffect>(effect)));
			break;
		case EFFECT_REBOUND:
			effects.push_back(make_shared<ReboundEffect>(*dynamic_pointer_cast<ReboundEffect>(effect)));
			break;
		case EFFECT_ABSORB:
			effects.push_back(make_shared<AbsorbEffect>(*dynamic_pointer_cast<AbsorbEffect>(effect)));
			break;
		case EFFECT_LOCK:
			effects.push_back(make_shared<LockEffect>(*dynamic_pointer_cast<LockEffect>(effect)));
			break;
		case EFFECT_DOT:
			effects.push_back(make_shared<DotEffect>(*dynamic_pointer_cast<DotEffect>(effect)));
			break;
		case EFFECT_REBATE:
			effects.push_back(make_shared<RebateEffect>(*dynamic_pointer_cast<RebateEffect>(effect)));
			break;
		case EFFECT_RECOVER:
			effects.push_back(make_shared<RecoverEffect>(*dynamic_pointer_cast<RecoverEffect>(effect)));
			break;
		case EFFECT_CURE:
			effects.push_back(make_shared<CureEffect>(*dynamic_pointer_cast<CureEffect>(effect)));
			break;
		case EFFECT_INCREASE:
			effects.push_back(make_shared<IncreaseEffect>(*dynamic_pointer_cast<IncreaseEffect>(effect)));
			break;
		case EFFECT_SHIELD:
			effects.push_back(make_shared<ShieldEffect>(*dynamic_pointer_cast<ShieldEffect>(effect)));
			break;
		case EFFECT_STATIC:
			effects.push_back(make_shared<StaticEffect>(*dynamic_pointer_cast<StaticEffect>(effect)));
			break;
		default:
			break;
		}
	}
}

string SingleAction::GetName() const {
	return name;
}

Realm SingleAction::GetRealm() const {
	return realm;
}

vector<shared_ptr<Effect>>& SingleAction::GetEffects() {
	return effects;
}

DualAction::DualAction(shared_ptr<SingleAction> a1, shared_ptr<SingleAction> a2)
	: action1(a1), action2(a2),
	Action(ACTION_DUAL,
		DUAL_ATTRIBUTE_PRIMARY.at({ a1->GetAttribute(), a2->GetAttribute() }),
		static_cast<int>(DUAL_COST_FACTOR * (a1->GetPoint() + a2->GetPoint())),
		static_cast<int>(DUAL_POWER_FACTOR * (a1->GetPower() + a2->GetPower())),
		{}) {
	// increase/static不并进双招的效果：两个基础招式各自带自己的连招增伤/静止增伤(见Controller::ApplyPower)
	for (const auto& e : a1->effects) {
		if (e->GetType() != EFFECT_INCREASE && e->GetType() != EFFECT_STATIC) effects.push_back(e);
	}
	// 两个招式有同名效果(同一种效果类型)时只取第一个招式的，比如逆天神功+地狱阴魔拳取逆天神功的禁锢，顺序互换就取地狱阴魔拳的
	for (const auto& e : a2->effects) {
		if (e->GetType() == EFFECT_INCREASE || e->GetType() == EFFECT_STATIC) continue;
		bool exists = false;
		for (const auto& own : effects) {
			if (own->GetType() == e->GetType()) { exists = true; break; }
		}
		if (!exists) effects.push_back(e);
	}
}

DualAction::~DualAction() {

}

shared_ptr<SingleAction> DualAction::GetAction1() const {
	return action1;
}

shared_ptr<SingleAction> DualAction::GetAction2() const {
	return action2;
}

string DualAction::GetText() const {
	return "(" + action1->GetName() + ", " + action2->GetName() + ")";
}

}
