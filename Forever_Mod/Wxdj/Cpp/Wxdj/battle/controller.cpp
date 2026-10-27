#include "controller.h"

#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <random>


using namespace std;

namespace wxdj {

Log::Log(LOG_TYPE type, int player, int amount) :
	type(type), subject(player), object(-1), attribute(ATTRIBUTE_NONE),
	pointAtk(0), pointHp(0), amount(amount), name(), effect(EFFECT_NONE) {

}

Log::Log(int player) :
	type(LOG_SKIP), subject(player), object(-1), attribute(ATTRIBUTE_NONE),
	pointAtk(0), pointHp(0), amount(0), name(), effect(EFFECT_NONE) {

}

Log::Log(int subject, int object, int amount, std::string name, int point) :
	type(LOG_MOVE), subject(subject), object(object), attribute(ATTRIBUTE_NONE),
	pointAtk(point), pointHp(0), amount(amount), name(name), effect(EFFECT_NONE) {

}

Log::Log(int subject, int object, ATTRIBUTE_TYPE attribute, int pointAtk, int pointHp) :
	type(LOG_DAMAGE), subject(subject), object(object), attribute(attribute),
	pointAtk(pointAtk), pointHp(pointHp), amount(0), name(), effect(EFFECT_NONE) {

}

Log::Log(int subject, int object) :
	type(LOG_DODGE), subject(subject), object(object), attribute(ATTRIBUTE_NONE),
	pointAtk(0), pointHp(0), amount(0), name(), effect(EFFECT_NONE) {

}

Log::Log(int subject, int object, ATTRIBUTE_TYPE attribute, int pointAtk, int pointHp, EFFECT_TYPE effect) :
	type(LOG_EFFECT), subject(subject), object(object), attribute(attribute),
	pointAtk(pointAtk), pointHp(pointHp), amount(0), name(), effect(effect) {

}

Log::Log(int subject, int object, int amount, EFFECT_TYPE effect) :
	type(LOG_EFFECT), subject(subject), object(object), attribute(ATTRIBUTE_NONE),
	pointAtk(0), pointHp(0), amount(amount), name(), effect(effect) {

}

Controller::Controller() :
	cultivators(),
	effects(this->cultivators.size()),
	environment(),
	status(this->cultivators.size()),
	regenMP(this->cultivators.size(), 0),
	skipAgi(this->cultivators.size(), 0.f),
	mark(0),
	logs(),
	rng(std::random_device{}()) {

}

Controller::Controller(std::vector<std::shared_ptr<Cultivator>> cultivators) :
	cultivators(cultivators),
	effects(this->cultivators.size()),
	environment(),
	status(this->cultivators.size()),
	regenMP(this->cultivators.size(), 0),
	skipAgi(this->cultivators.size(), 0.f),
	mark(0),
	logs(),
	rng(std::random_device{}()) {

}

Controller::~Controller() {

}

void Controller::AddCultivator(shared_ptr<Cultivator> cultivator) {
	cultivators.push_back(cultivator);
	effects.emplace_back();
	status.push_back(0);
	skipAgi.push_back(0.f);
	regenMP.push_back(0);
}

shared_ptr<Cultivator> Controller::GetCultivator(int idx) const {
	if (idx < 0 || idx >= (int)cultivators.size())
		throw runtime_error("Cultivator index out of range.");
	return cultivators[idx];
}

void Controller::ResetRound() {
	for (auto& s : status) s = 0;
	for (auto& s : skipAgi) s = 0.f;
}

void Controller::StartTurn() {
	ResetRound();
	ExecEffect(STAGE_START);
	for (int i = 0; i < (int)cultivators.size(); ++i) {
		int mpBefore = cultivators[i]->GetCurrentMP();
		RecoverMP(i, -1);
		regenMP[i] = cultivators[i]->GetCurrentMP() - mpBefore;
		cultivators[i]->RecoverAGI();
	}
}

void Controller::ActionTurn(vector<pair<shared_ptr<Action>, int>> actions) {
	if (actions.size() != cultivators.size())
		throw runtime_error("Cultivators and actions mismatch.");

	ExecEffect(STAGE_ACTION);

	// 静止：这回合积攒炸弹，或者没被禁锢却放了空炸弹(跳过不算)；要在所有人出招之前算好，因为放炸弹会清空炸弹存量
	staticNow.assign(cultivators.size(), 0);
	for (int j = 0; j < (int)cultivators.size(); ++j) {
		auto t = actions[j].first->GetType();
		staticNow[j] = t == ACTION_ACCUMULATE || (t == ACTION_BOMB && !(status[j] & 0x01) && cultivators[j]->GetBomb() <= 0);
	}

	for (int i = 0; i < (int)cultivators.size(); ++i) {
		auto& act = actions[i].first;
		int target = actions[i].second;

		if (act->GetType() == ACTION_SKIP) {
			cultivators[i]->ResetChain();
			SkipMove(i);
			PaySkip(i);
		}
		else if (act->GetType() == ACTION_ACCUMULATE) {
			status[i] |= 0x02;   // 这一回合在积攒：受到的伤害多ACCUMULATE_VULNERABILITY(见Vulnerability)
			cultivators[i]->ResetChain();
			MakeMove(i, target, act->GetPoint(), act->GetText(), act->GetPower());
			cultivators[i]->AccumulatePower();
			cultivators[i]->ResetSkipStreak();
		}
		else if (status[i] & 0x01) {
			cultivators[i]->ResetChain();
			SkipMove(i);
			PaySkip(i);
		}
		else if (act->GetType() == ACTION_BOMB && cultivators[i]->GetBomb() <= 0) {
			cultivators[i]->ResetChain();
			// 空炸弹：没有积攒过就释放，等于一次"纯跳过"——不翻倍敏捷、不损失敏捷(闪避照常用自己的敏捷)，也不出招；
			// 代价是这个回合不回复真气(把回合开始自然回复的真气撤销)
			SkipMove(i);
			cultivators[i]->UndoMP(regenMP[i]);
		}
		else if (act->GetType() == ACTION_BOMB) {
			cultivators[i]->ResetChain();
			// 战报里写明这次释放的炸弹能量(出招时才取出，所以先取再记)
			int bombPower = cultivators[i]->BombPower();
			if (!cultivators[i]->Mods().All().empty()) {
				double adj = cultivators[i]->Mods().Sum(MOD_BOMB_POWER, ATTRIBUTE_NONE, *cultivators[i]);
				if (adj != 0) bombPower = max(0, static_cast<int>(bombPower * (1.0 + adj)));
			}
			MakeMove(i, target, act->GetPoint(), act->GetText() + "(能量" + to_string(bombPower) + ")", act->GetPower());
			dynamic_pointer_cast<BombAction>(act)->SetPower(bombPower);
			cultivators[i]->ResetSkipStreak();
		}
		else {
			cultivators[i]->ResetSkipStreak();
			int chainBonus = 0, critPercent = 0, staticPercent = 0;
			vector<ChainStep> chainSteps;
			ApplyPower(i, target, act, chainBonus, critPercent, staticPercent, chainSteps);
			int cost = act->GetPoint();
			if (!cultivators[i]->Mods().All().empty()) {
				double adj = cultivators[i]->Mods().Sum(MOD_COST, act->GetAttribute(), *cultivators[i]);
				if (adj != 0) cost = max(0, static_cast<int>(cost * (1.0 + adj)));
			}
			MakeMove(i, target, cost, act->GetText(), act->GetPower());
			// 连招增伤战报：每个有增伤的基础招式一条(amount=自己的 比例x步数 的百分比，name="招式名|比例%|步数")；
			// 双招再加一条合计(name为空，amount=整个双招的威力增加百分比，按两个招式的基础威力加权)
			for (const auto& st : chainSteps) {
				logs.emplace_back(i, target, static_cast<int>(lround(st.ratio * st.steps * 100.0)), EFFECT_INCREASE);
				ostringstream pct;
				pct << setprecision(4) << st.ratio * 100.0;
				logs.back().name = st.name + "|" + pct.str() + "|" + to_string(st.steps);
			}
			if (act->GetType() == ACTION_DUAL && chainBonus > 0) logs.emplace_back(i, target, chainBonus, EFFECT_INCREASE);
			if (staticPercent > 0) logs.emplace_back(i, target, staticPercent, EFFECT_STATIC);
			if (critPercent > 0) logs.emplace_back(i, target, critPercent, EFFECT_CRIT);

			auto dot = act->GetEffect(EFFECT_DOT);
			if (dot && !dynamic_pointer_cast<DotEffect>(dot)->NeedHit()) {
				int amount = dynamic_pointer_cast<DotEffect>(dot)->DotInstant();
				logs.emplace_back(i, target, amount, EFFECT_DOT);
				TakeDamage(i, target, act->GetEffects(), actions[target].first->GetEffects(),
					act->GetAttribute(), amount, true, actions[target].first->GetAttribute());
				AddEffect(target, make_shared<DotEffect>(*dynamic_pointer_cast<DotEffect>(dot).get()));
			}

			auto rebate = act->GetEffect(EFFECT_REBATE);
			if (rebate && !dynamic_pointer_cast<RebateEffect>(rebate)->NeedHit()) {
				int amount = dynamic_pointer_cast<RebateEffect>(rebate)->RebateInstant();
				logs.emplace_back(i, -1, amount, EFFECT_REBATE);
				RecoverMP(i, amount);
				AddEffect(i, make_shared<RebateEffect>(*dynamic_pointer_cast<RebateEffect>(rebate).get()));
			}

			auto recover = act->GetEffect(EFFECT_RECOVER);
			if (recover && !dynamic_pointer_cast<RecoverEffect>(recover)->NeedHit()) {
				int amount = dynamic_pointer_cast<RecoverEffect>(recover)->RecoverInstant();
				logs.emplace_back(i, -1, amount, EFFECT_RECOVER);
				RecoverATK(i, amount);
				AddEffect(i, make_shared<RecoverEffect>(*dynamic_pointer_cast<RecoverEffect>(recover).get()));
			}

			auto cure = act->GetEffect(EFFECT_CURE);
			if (cure && !dynamic_pointer_cast<CureEffect>(cure)->NeedHit()) {
				int amount = dynamic_pointer_cast<CureEffect>(cure)->CureInstant();
				logs.emplace_back(i, -1, amount, EFFECT_CURE);
				RecoverHP(i, amount);
				AddEffect(i, make_shared<CureEffect>(*dynamic_pointer_cast<CureEffect>(cure).get()));
			}

			auto shield = act->GetEffect(EFFECT_SHIELD);
			if (shield) {
				// 盾量 = 出招者武力上限 x 比例；盾继承自己的五行属性
				auto proto = dynamic_pointer_cast<ShieldEffect>(shield);
				int strength = static_cast<int>(proto->Ratio() * static_cast<float>(cultivators[i]->GetMaxATK()));
				auto s = make_shared<ShieldEffect>(strength, proto->RemainingRounds(), cultivators[i]->GetElements());
				logs.emplace_back(i, -1, s->Strength(), EFFECT_SHIELD);
				AddEffect(i, s);
			}
		}
	}

	for (auto& [action, _] : actions) {
		PutoutReaction(action->GetAttribute());
	}
}

void Controller::CheckTurn(vector<pair<shared_ptr<Action>, int>> actions) {
	if (actions.size() != cultivators.size())
		throw runtime_error("Cultivators and actions mismatch.");

	ExecEffect(STAGE_CHECK);

	// 被禁锢的一方这回合出的招(除了积攒)已经被跳过，它的减伤/反弹/吸收/物理等效果也不能生效，
	// 当作跳过处理；否则对方打过来时，被跳过的招式上的减伤还会白白生效。
	for (int j = 0; j < (int)cultivators.size(); ++j) {
		if ((status[j] & 0x01) && actions[j].first->GetType() != ACTION_ACCUMULATE) {
			actions[j].first = make_shared<SkipAction>();
		}
		// 空炸弹(释放时炸弹能量为0)只是一次纯跳过，不是炸弹
		else if (actions[j].first->GetType() == ACTION_BOMB && actions[j].first->GetPower() <= 0) {
			actions[j].first = make_shared<SkipAction>();
		}
	}

	for (int i = 0; i < (int)cultivators.size(); ++i) {
		auto& act_i = actions[i].first;
		int target_i = actions[i].second;

		if (act_i->GetType() == ACTION_SKIP) continue;
		if (act_i->GetType() == ACTION_ACCUMULATE) continue;
		if (status[i] & 0x01) continue;

		int j = target_i;
		auto& act_j = actions[j].first;

		if (act_i->GetType() == ACTION_BOMB) {
			// 玩家 i 施放真气炸弹，
			if (act_j->GetType() == ACTION_SKIP ||
				act_j->GetType() == ACTION_ACCUMULATE ||
				actions[j].second != (int)i ||
				(status[j] & 0x01)) {
				// 玩家 i 施放真气炸弹，目标 j 未正面应对，计算双方伤害
				float powerEnemy = act_i->GetPower() * (FindReaction(EFFECT_RUST) ? BOMB_RUST_FACTOR : BOMB_HIT_FACTOR_PASSIVE);
				float powerSelf = act_i->GetPower() * (FindReaction(EFFECT_RUST) ? BOMB_RUST_FACTOR : BOMB_BACKFIRE_FACTOR);
				TakeDamage(i, j, act_i->GetEffects(), act_j->GetEffects(),
					act_i->GetAttribute(), (int)powerEnemy, true, act_j->GetAttribute());
				TakeDamage(i, i, act_i->GetEffects(), act_i->GetEffects(),
					act_i->GetAttribute(), (int)powerSelf);
			}
			else if (act_j->GetType() == ACTION_BOMB && actions[j].second == (int)i) {
				// 双方互扔真气炸弹：两个炸弹之和乘反噬系数，双方一起受到(每对玩家各算一次，j 这一份由 j 自己的循环给 i)
				float sum = (float)act_i->GetPower() + (float)act_j->GetPower();
				float powerEnemy = sum * (FindReaction(EFFECT_RUST) ? BOMB_RUST_FACTOR : BOMB_BACKFIRE_FACTOR);
				TakeDamage(j, j, act_j->GetEffects(), act_j->GetEffects(),
					act_j->GetAttribute(), (int)powerEnemy);
			}
			else {
				// 目标 j 施放普通招式，若真气炸弹威力更大，则目标 j 受到差额伤害
				if (act_j->GetPower() < act_i->GetPower()) {
					float powerEnemy = (float)(act_i->GetPower() - act_j->GetPower());
					powerEnemy *= (FindReaction(EFFECT_RUST) ? BOMB_RUST_FACTOR : BOMB_HIT_FACTOR_ACTIVE);
					TakeDamage(i, j, act_i->GetEffects(), act_j->GetEffects(),
						act_i->GetAttribute(), (int)powerEnemy, true, act_j->GetAttribute());
				}
			}
		}
		else {
			// 玩家 i 施放普通招式
			if (act_j->GetType() == ACTION_SKIP ||
				act_j->GetType() == ACTION_ACCUMULATE ||
				actions[j].second != (int)i ||
				(status[j] & 0x01)) {
				// 玩家 i 施放普通招式，目标 j 未正面应对，计算双方伤害
				uniform_real_distribution<double> dist(0.0, 1.0);
				// 对方跳过(含被禁锢而无法出招，但没有选积攒)时：付得起敏捷损失就用翻倍的敏捷闪避(被禁锢的一方也能)，
				// 付不起(闪避之后敏捷会低于1)就不翻倍、不扣敏捷，仍用自己原来的敏捷闪避；没跳过的一方照常用自己的敏捷闪避，被禁锢又选了积攒的也一样用当前敏捷闪避
				float defenderAGI = cultivators[j]->GetAGI();
				bool canDodge = true;   // 被禁锢又选了积攒的人也用自己当前的敏捷闪避
				if (skipAgi[j] > 0.f) { defenderAGI = skipAgi[j]; canDodge = true; }
				double dodgeChance = DodgeProb(i, j, act_i->GetAttribute(), defenderAGI);
				if (canDodge && dist(rng) < dodgeChance) {
					DodgeSuccess(i, j);
					BuildDualEnvironment(act_i);   // 双招被闪避=未命中，它的效果计入场地效果
					continue;
				}
				else {
					bool hit = RollHit(i, act_i->GetAttribute());
					if (hit) {
						HitSuccess(i, j, act_i, act_j);
						TakeDamage(i, j, act_i->GetEffects(), act_j->GetEffects(),
							act_i->GetAttribute(), act_i->GetPower(), true, act_j->GetAttribute());
					}
					else {
						HitFail(i);
						BuildDualEnvironment(act_i);
					}
				}
			}
			else if (act_j->GetType() == ACTION_BOMB && actions[j].second == (int)i) {
				// 目标 j 施放真气炸弹且指向玩家 i，只计算目标 j 受到的伤害
				float powerEnemy = 0.f;
				if (act_i->GetPower() < act_j->GetPower()) {
					powerEnemy = (float)act_j->GetPower();
					powerEnemy *= (FindReaction(EFFECT_RUST) ? BOMB_RUST_FACTOR : BOMB_BACKFIRE_FACTOR);
					TakeDamage(j, j, act_j->GetEffects(), act_j->GetEffects(),
						act_j->GetAttribute(), (int)powerEnemy);
				}
				else if (act_i->GetPower() > 2 * act_j->GetPower()) {
					powerEnemy = (float)(act_i->GetPower() - 2 * act_j->GetPower());
					TakeDamage(i, j, act_i->GetEffects(), act_j->GetEffects(),
						act_i->GetAttribute(), (int)powerEnemy, true, act_j->GetAttribute());
				}
				else {
					powerEnemy = (float)(2 * act_j->GetPower() - act_i->GetPower());
					powerEnemy *= (FindReaction(EFFECT_RUST) ? BOMB_RUST_FACTOR : BOMB_BACKFIRE_FACTOR);
					TakeDamage(j, j, act_j->GetEffects(), act_j->GetEffects(),
						act_j->GetAttribute(), (int)powerEnemy);
				}
			}
			else {
				// 双方互相施放普通招式，仅该分支会同时计算双方伤害，需要控制每对玩家只出现一次
				if (j < i) continue;

				uniform_real_distribution<double> dist(0.0, 1.0);
				int point1 = act_i->GetPower();
				int point2 = act_j->GetPower();
				ATTRIBUTE_TYPE attr1 = act_i->GetAttribute();
				ATTRIBUTE_TYPE attr2 = act_j->GetAttribute();

				if (CheckCounter(attr1, attr2)) point2 /= 2;
				if (CheckCounter(attr2, attr1)) point1 /= 2;

				// 双方出招的最终属性碰撞产生的场地效果；双招自己的两个基础招式构造的场地效果只在双招未命中时才产生(见下面未命中的分支)
				bool clashReaction = AddReactionEnvironment(CheckReaction(attr1, attr2));
				auto buildOnMiss = [&]() {
					if (clashReaction) return;
					BuildDualEnvironment(act_i);
					BuildDualEnvironment(act_j);
				};

				if (point1 > point2) {
					bool hit = RollHit(i, attr1);
					if (hit) {
						HitSuccess(i, j, act_i, act_j);
						HitSuccess(j, i, act_j, act_i);
						TakeDamage(i, j, act_i->GetEffects(), act_j->GetEffects(),
							attr1, point1 - point2, true, attr2);
						if (auto effect = act_i->GetEffect(EFFECT_PENETRATE)) {
							int pen = (int)(point2 * (dynamic_pointer_cast<PenetrateEffect>(effect)->GetRatio(attr2)));
							logs.emplace_back(i, j, attr1, pen, 0, EFFECT_PENETRATE);
							TakeDamage(i, j, act_i->GetEffects(), act_j->GetEffects(),
								attr1, pen, false, attr2);
						}
						if (auto effect = act_j->GetEffect(EFFECT_PENETRATE)) {
							int pen = (int)(point2 * (dynamic_pointer_cast<PenetrateEffect>(effect)->GetRatio(attr1)));
							logs.emplace_back(j, i, attr2, pen, 0, EFFECT_PENETRATE);
							TakeDamage(j, i, act_j->GetEffects(), act_i->GetEffects(),
								attr2, pen, false, attr1);
						}
					}
					else {
						HitFail(i);
						buildOnMiss();
					}
				}
				else if (point2 > point1) {
					bool hit = RollHit(i, attr1);
					if (hit) {
						HitSuccess(i, j, act_i, act_j);
						HitSuccess(j, i, act_j, act_i);
						TakeDamage(j, i, act_j->GetEffects(), act_i->GetEffects(),
							attr2, point2 - point1, true, attr1);
						if (auto effect = act_i->GetEffect(EFFECT_PENETRATE)) {
							int pen = (int)(point1 * (dynamic_pointer_cast<PenetrateEffect>(effect)->GetRatio(attr2)));
							logs.emplace_back(i, j, attr1, pen, 0, EFFECT_PENETRATE);
							TakeDamage(i, j, act_i->GetEffects(), act_j->GetEffects(),
								attr1, pen, false, attr2);
						}
						if (auto effect = act_j->GetEffect(EFFECT_PENETRATE)) {
							int pen = (int)(point1 * (dynamic_pointer_cast<PenetrateEffect>(effect)->GetRatio(attr1)));
							logs.emplace_back(j, i, attr2, pen, 0, EFFECT_PENETRATE);
							TakeDamage(j, i, act_j->GetEffects(), act_i->GetEffects(),
								attr2, pen, false, attr1);
						}
					}
					else {
						HitFail(i);
						buildOnMiss();
					}
				}
				else {
					bool hit = RollHit(i, attr1);
					if (hit) {
						HitSuccess(i, j, act_i, act_j);
						HitSuccess(j, i, act_j, act_i);
						if (auto effect = act_i->GetEffect(EFFECT_PENETRATE)) {
							int pen = (int)(point2 * (dynamic_pointer_cast<PenetrateEffect>(effect)->GetRatio(attr2)));
							logs.emplace_back(i, j, attr1, pen, 0, EFFECT_PENETRATE);
							TakeDamage(i, j, act_i->GetEffects(), act_j->GetEffects(),
								attr1, pen, false, attr2);
						}
						if (auto effect = act_j->GetEffect(EFFECT_PENETRATE)) {
							int pen = (int)(point1 * (dynamic_pointer_cast<PenetrateEffect>(effect)->GetRatio(attr1)));
							logs.emplace_back(j, i, attr2, pen, 0, EFFECT_PENETRATE);
							TakeDamage(j, i, act_j->GetEffects(), act_i->GetEffects(),
								attr2, pen, false, attr1);
						}
					}
					else {
						HitFail(i);
						buildOnMiss();
					}
				}
			}
		}
	}
}

void Controller::EndTurn() {
	ExecEffect(STAGE_END);

	// 场地效果的回合计数
	for (auto it = environment.begin(); it != environment.end(); ) {
		bool useUp = false;
		if (auto burn = dynamic_pointer_cast<BurnEffect>(*it)) {
			burn->PassRound();
			useUp = burn->UseUp();
		}
		else if (auto rust = dynamic_pointer_cast<RustEffect>(*it)) {
			rust->PassRound();
			useUp = rust->UseUp();
		}
		else if (auto dust = dynamic_pointer_cast<DustEffect>(*it)) {
			dust->PassRound();
			useUp = dust->UseUp();
		}
		else if (auto mud = dynamic_pointer_cast<MudEffect>(*it)) {
			mud->PassRound();
			useUp = mud->UseUp();
		}
		if (useUp) it = environment.erase(it);
		else ++it;
	}

	for (auto& c : cultivators) c->Mods().Tick();
	for (auto& c : cultivators) c->UpdateRealm();
}

void Controller::Seed(unsigned int seed) {
	rng.seed(seed);
}

void Controller::AddEffect(int idx, shared_ptr<Effect> effect) {
	if (idx >= 0 && idx < (int)effects.size())
		effects[idx].push_back(effect);
}

bool Controller::AddReactionEnvironment(int reaction) {
	switch (reaction) {
	case EFFECT_BURN: AddEnvironment(make_shared<BurnEffect>()); return true;
	case EFFECT_RUST: AddEnvironment(make_shared<RustEffect>()); return true;
	case EFFECT_DUST: AddEnvironment(make_shared<DustEffect>()); return true;
	case EFFECT_MUD: AddEnvironment(make_shared<MudEffect>()); return true;
	default: return false;
	}
}

void Controller::BuildDualEnvironment(const shared_ptr<Action>& act) {
	if (act->GetType() != ACTION_DUAL) return;
	auto dual = dynamic_pointer_cast<DualAction>(act);
	AddReactionEnvironment(CheckReaction(dual->GetAction1()->GetAttribute(), dual->GetAction2()->GetAttribute()));
}

void Controller::AddEnvironment(shared_ptr<Effect> effect) {
	auto it = find_if(environment.begin(), environment.end(),
		[effect](shared_ptr<Effect> e) { return e->GetType() == effect->GetType(); });
	if (it == environment.end()) environment.push_back(effect);
	else *it = effect;
}

void Controller::ExecEffect(STAGE_TYPE stage) {
	ExecModifiers(stage);
	for (int i = 0; i < (int)effects.size(); ++i) {
		for (int j = 0; j < (int)effects[i].size(); ++j) {
			auto eff = effects[i][j];   // 按值持有：持续伤害结算会让护盾被打光而从effects[i]里移除，引用/下标会失效
			switch (eff->GetType()) {
			case EFFECT_LOCK:
				if (stage == STAGE_START) {
					status[i] |= 0x01;
					effects[i].erase(effects[i].begin() + j);
					--j;
				}
				break;
			case EFFECT_DOT:
				if (stage == STAGE_START) {
					int dmg = dynamic_pointer_cast<DotEffect>(eff)->DotOnce();
					if (dmg != 0) {
						logs.emplace_back(-1, i, dmg, EFFECT_DOT);
						TakeDamage(-1, i, {}, {}, ATTRIBUTE_NONE, dmg, false);
						auto self = find(effects[i].begin(), effects[i].end(), eff);   // 护盾被移除后本效果的下标变了
						if (self != effects[i].end()) j = static_cast<int>(self - effects[i].begin());
					}
					if (dynamic_pointer_cast<DotEffect>(eff)->UseUp()) {
						effects[i].erase(effects[i].begin() + j);
						--j;
					}
				}
				break;
			case EFFECT_REBATE:
				if (stage == STAGE_START) {
					int mp = dynamic_pointer_cast<RebateEffect>(eff)->RebateOnce();
					if (mp != 0) {
						logs.emplace_back(i, -1, mp, EFFECT_REBATE);
						RecoverMP(i, mp, false);
					}
					if (dynamic_pointer_cast<RebateEffect>(eff)->UseUp()) {
						effects[i].erase(effects[i].begin() + j);
						--j;
					}
				}
				break;
			case EFFECT_RECOVER:
				if (stage == STAGE_START) {
					int atk = dynamic_pointer_cast<RecoverEffect>(eff)->RecoverOnce();
					if (atk != 0) {
						logs.emplace_back(i, -1, atk, EFFECT_RECOVER);
						RecoverATK(i, atk, false);
					}
					if (dynamic_pointer_cast<RecoverEffect>(eff)->UseUp()) {
						effects[i].erase(effects[i].begin() + j);
						--j;
					}
				}
				break;
			case EFFECT_SHIELD:
				if (stage == STAGE_END) {
					auto shield = dynamic_pointer_cast<ShieldEffect>(eff);
					shield->PassRound();
					if (shield->UseUp()) {
						effects[i].erase(effects[i].begin() + j);
						--j;
					}
				}
				break;
			case EFFECT_CURE:
				if (stage == STAGE_START) {
					int hp = dynamic_pointer_cast<CureEffect>(eff)->CureOnce();
					if (hp != 0) {
						logs.emplace_back(i, -1, hp, EFFECT_CURE);
						RecoverHP(i, hp, false);
					}
					if (dynamic_pointer_cast<CureEffect>(eff)->UseUp()) {
						effects[i].erase(effects[i].begin() + j);
						--j;
					}
				}
				break;
			default:
				break;
			}
		}
	}
}

const vector<shared_ptr<Effect>>& Controller::GetEffectsOf(int idx) const {
	return effects.at(idx);
}

const vector<shared_ptr<Effect>>& Controller::GetEnvironmentEffects() const {
	return environment;
}

bool Controller::IsLocked(int idx) const {
	return idx >= 0 && idx < static_cast<int>(status.size()) && (status[idx] & 0x01) != 0;
}

vector<pair<EFFECT_TYPE, int>> Controller::GetEnvironment() const {
	vector<pair<EFFECT_TYPE, int>> result;
	for (const auto& e : environment) {
		int remaining = 0;
		if (auto burn = dynamic_pointer_cast<BurnEffect>(e)) remaining = burn->RemainingRounds();
		else if (auto rust = dynamic_pointer_cast<RustEffect>(e)) remaining = rust->RemainingRounds();
		else if (auto dust = dynamic_pointer_cast<DustEffect>(e)) remaining = dust->RemainingRounds();
		else if (auto mud = dynamic_pointer_cast<MudEffect>(e)) remaining = mud->RemainingRounds();
		result.emplace_back(e->GetType(), remaining);
	}
	return result;
}

vector<Log> Controller::GetNews() {
	vector<Log> news;
	for (size_t i = mark; i < logs.size(); ++i) news.push_back(logs[i]);
	mark = (int)logs.size();
	return news;
}

void Controller::RecoverMP(int idx, int amount, bool log) {
	if (log) logs.emplace_back(LOG_MP, idx, amount);
	cultivators[idx]->RecoverMP(amount);
}

void Controller::ConsumeMP(int idx, int amount, bool log) {
	if (log) logs.emplace_back(LOG_CONSUME, idx, amount);
	cultivators[idx]->ConsumeMP(amount);
}

void Controller::MakeMove(int subject, int object, int amount, string name, int point, bool log) {
	if (FindReaction(EFFECT_MUD)) amount = static_cast<int>(amount * 1.5f);
	if (log) logs.emplace_back(subject, object, amount, name, point);
	// 真气不够时先扣光真气，缺的部分按2倍从武力值里透支(Cultivator::ConsumeMP)，这里把透支扣掉的武力值写进战报
	int deficit = amount - cultivators[subject]->GetCurrentMP();
	if (log && deficit > 0) logs.emplace_back(LOG_CONSUME, subject, deficit * 2);
	cultivators[subject]->ConsumeMP(amount);
}

void Controller::SkipMove(int idx, bool log) {
	if (log) logs.emplace_back(idx);
}

void Controller::RecoverATK(int idx, int amount, bool log) {
	if (log) logs.emplace_back(LOG_ATK, idx, amount);
	cultivators[idx]->RecoverATK(amount);
}

void Controller::RecoverHP(int idx, int amount, bool log) {
	if (log) logs.emplace_back(LOG_HP, idx, amount);
	cultivators[idx]->RecoverHP(amount);
}

void Controller::HitSuccess(int subject, int object, shared_ptr<Action> offend,
	shared_ptr<Action> defend, bool log) {
	auto lock = offend->GetEffect(EFFECT_LOCK);
	if (lock) {
		float prob = dynamic_pointer_cast<LockEffect>(lock)->GetProb();
		uniform_real_distribution<double> dist(0.0, 1.0);
		if (dist(rng) < prob) {
			if (log) logs.emplace_back(subject, object, 0, EFFECT_LOCK);
			AddEffect(object, lock);
		}
		else {
			lock->SetMiss();
		}
	}

	auto dot = offend->GetEffect(EFFECT_DOT);
	if (dot && dynamic_pointer_cast<DotEffect>(dot)->NeedHit()) {
		int amount = dynamic_pointer_cast<DotEffect>(dot)->DotInstant();
		if (log) logs.emplace_back(subject, object, amount, EFFECT_DOT);
		TakeDamage(subject, object, offend->GetEffects(), defend->GetEffects(),
			offend->GetAttribute(), amount, true, defend->GetAttribute());
		AddEffect(object, make_shared<DotEffect>(*dynamic_pointer_cast<DotEffect>(dot).get()));
	}

	auto rebate = offend->GetEffect(EFFECT_REBATE);
	if (rebate && dynamic_pointer_cast<RebateEffect>(rebate)->NeedHit()) {
		int amount = dynamic_pointer_cast<RebateEffect>(rebate)->RebateInstant();
		if (log) logs.emplace_back(subject, -1, amount, EFFECT_REBATE);
		RecoverMP(subject, amount);
		AddEffect(subject, make_shared<RebateEffect>(*dynamic_pointer_cast<RebateEffect>(rebate).get()));
	}

	auto recover = offend->GetEffect(EFFECT_RECOVER);
	if (recover && dynamic_pointer_cast<RecoverEffect>(recover)->NeedHit()) {
		int amount = dynamic_pointer_cast<RecoverEffect>(recover)->RecoverInstant();
		if (log) logs.emplace_back(subject, -1, amount, EFFECT_RECOVER);
		RecoverATK(subject, amount);
		AddEffect(subject, make_shared<RecoverEffect>(*dynamic_pointer_cast<RecoverEffect>(recover).get()));
	}

	auto cure = offend->GetEffect(EFFECT_CURE);
	if (cure && dynamic_pointer_cast<CureEffect>(cure)->NeedHit()) {
		int amount = dynamic_pointer_cast<CureEffect>(cure)->CureInstant();
		if (log) logs.emplace_back(subject, -1, amount, EFFECT_CURE);
		RecoverHP(subject, amount);
		AddEffect(subject, make_shared<CureEffect>(*dynamic_pointer_cast<CureEffect>(cure).get()));
	}
}

void Controller::HitFail(int subject, bool log) {
	if (log) logs.emplace_back(LOG_FAIL, subject, 0);
}

void Controller::TakeDamage(int subject, int object,
	vector<shared_ptr<Effect>> offend,
	vector<shared_ptr<Effect>> defend,
	ATTRIBUTE_TYPE attribute, int amount, bool log, ATTRIBUTE_TYPE defendAttribute) {
	// 特性：免疫该属性
	if (object >= 0 && object < static_cast<int>(cultivators.size()) && !cultivators[object]->Mods().All().empty() &&
		cultivators[object]->Mods().Any(MOD_IMMUNE, attribute, *cultivators[object])) {
		amount = 0;
	}

	// 场地乘区：灼烧是1.5，特性可以按属性以1为基准加减；没有特性时和以前一样
	double fieldAdj = 0;
	if (subject >= 0 && subject < static_cast<int>(cultivators.size()) && !cultivators[subject]->Mods().All().empty())
		fieldAdj = cultivators[subject]->Mods().Sum(MOD_FIELD_MULT, attribute, *cultivators[subject]);
	if (fieldAdj != 0) amount = static_cast<int>(amount * max(0.0, (FindReaction(EFFECT_BURN) ? 1.5 : 1.0) + fieldAdj));
	else if (FindReaction(EFFECT_BURN)) amount = static_cast<int>(amount * 1.5f);

	// 受到伤害乘区：减伤(海龙金身)和易伤是同一个乘区，乘区 = 1 + 易伤加和 - 减伤比例；易伤只增不减，所以乘区至少是 1 - 减伤比例
	auto reduce = find_if(defend.begin(), defend.end(),
		[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_REDUCE; });
	double vuln = Vulnerability(object, attribute);
	if (vuln != 0) {
		float ratio = reduce != defend.end() ? dynamic_pointer_cast<ReduceEffect>(*reduce)->GetRatio(attribute) : 0.f;
		amount = static_cast<int>(amount * max(0.0, 1.0 - ratio + vuln));
	}
	else if (reduce != defend.end()) {
		amount = static_cast<int>(amount * (1.0f - dynamic_pointer_cast<ReduceEffect>(*reduce)->GetRatio(attribute)));
	}

	// 属性抗性：实际伤害=伤害x2/(抗性+1)，在护盾之前结算
	if (double f = ResistFactor(object, attribute); f != 1.0) amount = static_cast<int>(lround(amount * f));

	// 护盾：同时保护武力和健康，先于物理转换/反弹
	amount = ApplyShield(object, amount, attribute);

	// 物理效果：按对方这一招的属性，有一部分伤害转换成物理伤害(扣健康值)，剩余的仍是招式伤害(扣武力值)
	float physicalRatio = 0.f;
	auto physical = find_if(offend.begin(), offend.end(),
		[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_PHYSICAL; });
	if (physical != offend.end()) {
		physicalRatio = min(1.f, max(0.f, dynamic_pointer_cast<PhysicalEffect>(*physical)->GetRatio(defendAttribute)));
	}
	int hpPart = static_cast<int>(amount * physicalRatio);
	int atkPart = amount - hpPart;

	// 反弹是减伤的升级版：受击者只承受反弹之后剩下的伤害，被反弹的那部分全部还给攻击者
	int reboundAtk = 0, reboundHp = 0;
	if (subject >= 0 && subject != object) {   // subject<0是持续伤害等没有出手方的伤害：不反弹(没有可反弹的对象)
		float ratio = 0.f;
		auto rebound = find_if(defend.begin(), defend.end(),
			[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_REBOUND; });
		if (rebound != defend.end()) ratio = dynamic_pointer_cast<ReboundEffect>(*rebound)->GetRatio(attribute);
		if (object >= 0 && !cultivators[object]->Mods().All().empty()) {
			double adj = cultivators[object]->Mods().Sum(MOD_REBOUND, attribute, *cultivators[object]);   // 特性的反弹比例加到反弹效果上
			if (adj != 0) ratio += static_cast<float>(adj);
		}
		ratio = min(1.f, max(0.f, ratio));
		if (ratio > 0.f) {
			reboundAtk = static_cast<int>(atkPart * ratio);
			reboundHp = static_cast<int>(hpPart * ratio);
		}
	}
	int ownAtk = atkPart - reboundAtk, ownHp = hpPart - reboundHp;

	if (log) logs.emplace_back(subject, object, attribute, ownAtk, ownHp);
	if (ownAtk > 0) cultivators[object]->TakeDamage(ownAtk, false);
	if (ownHp > 0) cultivators[object]->TakeDamage(ownHp, true);
	ApplyDamageTriggers(subject, object, attribute, ownAtk + ownHp);

	if (subject == object) return;

	if (reboundAtk > 0 || reboundHp > 0) {
		logs.emplace_back(object, subject, attribute, reboundAtk, reboundHp, EFFECT_REBOUND);
		TakeDamage(object, subject, defend, offend, attribute, reboundAtk, reboundHp, false);
	}

	auto absorb = find_if(defend.begin(), defend.end(),
		[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_ABSORB; });
	if (absorb != defend.end()) {
		int mp = static_cast<int>(amount * dynamic_pointer_cast<AbsorbEffect>(*absorb)->GetRatio(attribute));
		logs.emplace_back(object, subject, mp, EFFECT_ABSORB);
		RecoverMP(object, mp);
	}
}

void Controller::TakeDamage(int subject, int object,
	vector<shared_ptr<Effect>> offend,
	vector<shared_ptr<Effect>> defend,
	ATTRIBUTE_TYPE attribute, int atk, int hp, bool log) {
	// 反弹回来的伤害是从已经结算过(含灼烧加成)的伤害按比例算出来的，不再乘一次灼烧：否则
	// 灼烧x1.5会让双方互相反弹的伤害越弹越大(比例>2/3时永不衰减)，递归爆栈。
	// 另外限制反弹的递归深度，多层之后的伤害早已可以忽略。
	static thread_local int depth = 0;
	if (depth >= 8) return;
	struct DepthGuard { DepthGuard() { ++depth; } ~DepthGuard() { --depth; } } guard;

	auto reduce = find_if(defend.begin(), defend.end(),
		[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_REDUCE; });
	double vuln = Vulnerability(object, attribute);   // 同一个受到伤害乘区：1 + 易伤加和 - 减伤比例
	if (vuln != 0) {
		float ratio = reduce != defend.end() ? dynamic_pointer_cast<ReduceEffect>(*reduce)->GetRatio(attribute) : 0.f;
		double keep = max(0.0, 1.0 - ratio + vuln);
		atk = static_cast<int>(atk * keep);
		hp = static_cast<int>(hp * keep);
	}
	else if (reduce != defend.end()) {
		float keep = 1.0f - dynamic_pointer_cast<ReduceEffect>(*reduce)->GetRatio(attribute);
		atk = static_cast<int>(atk * keep);
		hp = static_cast<int>(hp * keep);
	}

	if (double f = ResistFactor(object, attribute); f != 1.0) {
		atk = static_cast<int>(lround(atk * f));
		hp = static_cast<int>(lround(hp * f));
	}

	// 护盾：穿透量按武力/健康原来的比例分摊
	if (atk + hp > 0) {
		int total = atk + hp;
		int through = ApplyShield(object, total, attribute);
		if (through != total) {
			atk = static_cast<int>(atk * (static_cast<double>(through) / total));
			hp = through - atk;
		}
	}

	// 反弹是减伤的升级版：只承受反弹之后剩下的伤害
	int reboundAtk = 0, reboundHp = 0;
	if (subject >= 0 && subject != object) {   // subject<0是持续伤害等没有出手方的伤害：不反弹(没有可反弹的对象)
		auto rebound = find_if(defend.begin(), defend.end(),
			[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_REBOUND; });
		if (rebound != defend.end()) {
			float ratio = min(1.f, max(0.f, dynamic_pointer_cast<ReboundEffect>(*rebound)->GetRatio(attribute)));
			reboundAtk = static_cast<int>(atk * ratio);
			reboundHp = static_cast<int>(hp * ratio);
		}
	}
	int ownAtk = atk - reboundAtk, ownHp = hp - reboundHp;

	if (log) logs.emplace_back(subject, object, attribute, ownAtk, ownHp);
	if (ownAtk > 0) cultivators[object]->TakeDamage(ownAtk, false);
	if (ownHp > 0) cultivators[object]->TakeDamage(ownHp, true);
	ApplyDamageTriggers(subject, object, attribute, ownAtk + ownHp);   // 反弹回来的伤害也触发特性

	if (subject == object) return;

	if (reboundAtk > 0 || reboundHp > 0) {
		logs.emplace_back(object, subject, attribute, reboundAtk, reboundHp, EFFECT_REBOUND);
		TakeDamage(object, subject, defend, offend, attribute, reboundAtk, reboundHp, false);
	}

	auto absorb = find_if(defend.begin(), defend.end(),
		[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_ABSORB; });
	if (absorb != defend.end()) {
		int mp = static_cast<int>((atk + hp) * dynamic_pointer_cast<AbsorbEffect>(*absorb)->GetRatio(attribute));
		logs.emplace_back(object, subject, mp, EFFECT_ABSORB);
		RecoverMP(object, mp);
	}
}

// 有效威力：本回合出的招(单招=1个，双招=2个基础招式)各自按"家族+式号"查连招链，再乘上特性的各个乘区。
//   单个基础招式的威力 = (基础威力 + 特性的基础威力加成) x (1 + 增伤比例 x 连续步数)；双招再乘DUAL_POWER_FACTOR；
//   然后依次乘暴击(1+暴击伤害)、自己的增伤乘区(1+加和)。目标的易伤和减伤是同一个乘区，在TakeDamage里结算。同一乘区内加算，乘区之间相乘。
// 没有任何加成时不动威力(保证没有increase、没有特性的招式和以前逐位一致，也不多抽随机数)。
void Controller::ApplyPower(int idx, int target, const shared_ptr<Action>& act, int& chainBonus, int& critPercent, int& staticPercent, vector<ChainStep>& chainSteps) {
	chainBonus = 0;
	staticPercent = 0;
	chainSteps.clear();
	critPercent = 0;
	vector<shared_ptr<SingleAction>> comps;
	if (act->GetType() == ACTION_SINGLE) comps.push_back(dynamic_pointer_cast<SingleAction>(act));
	else if (act->GetType() == ACTION_DUAL) {
		auto dual = dynamic_pointer_cast<DualAction>(act);
		comps.push_back(dual->GetAction1());
		comps.push_back(dual->GetAction2());
	}
	else {
		cultivators[idx]->ResetChain();
		return;
	}

	Cultivator& me = *cultivators[idx];
	bool dual = act->GetType() == ACTION_DUAL;
	vector<pair<string, int>> moves;
	double chainSum = 0, fullSum = 0, staticSum = 0;
	bool boosted = false, added = false;
	bool enemyStatic = target >= 0 && target < static_cast<int>(staticNow.size()) && target != idx && staticNow[target];
	for (auto& c : comps) {
		string family = FamilyOfName(c->GetName());
		int form = FormOfName(c->GetName());
		int n = me.PeekChain(family, form);
		double ratio = 0;
		if (auto inc = dynamic_pointer_cast<IncreaseEffect>(c->GetEffect(EFFECT_INCREASE))) ratio = inc->GetRatio();
		if (ratio * n > 0) { boosted = true; chainSteps.push_back({ c->GetName(), static_cast<float>(ratio), n }); }
		double base = c->GetBasePower();
		double add = 0;
		for (const auto& m : me.Mods().All()) {
			if (m.kind != MOD_BASE_POWER || !(m.attrMask & (1u << static_cast<int>(c->GetAttribute()))) || !ModifierSet::CondHolds(m.cond, me)) continue;
			add += m.percent ? base * m.value / 100.0 : m.value;
		}
		if (add != 0) added = true;
		chainSum += base * (1.0 + ratio * n);
		fullSum += (base + add) * (1.0 + ratio * n);
		// 静止增伤：按这个基础招式在威力里的份量加权(双招时只有带static的那一半贡献)
		if (enemyStatic) {
			if (auto st = dynamic_pointer_cast<StaticEffect>(c->GetEffect(EFFECT_STATIC)))
				staticSum += static_cast<double>(st->GetRatio()) * (base * (1.0 + ratio * n));
		}
		moves.emplace_back(family, form);
	}
	me.CommitChain(moves);

	if (boosted) {
		int chainPower = static_cast<int>(dual ? DUAL_POWER_FACTOR * chainSum : chainSum);
		chainBonus = max(0, static_cast<int>(lround((static_cast<double>(chainPower) / max(1, act->GetBasePower()) - 1.0) * 100.0)));
	}

	ATTRIBUTE_TYPE attr = act->GetAttribute();
	double mult = 1.0;
	bool zoned = false;
	// 自己拥有的属性的招式：威力倍率(默认1.5)；无属性招式/没有这个属性的玩家不触发
	if (me.HasElement(attr)) {
		mult *= me.ElemPowerMul(attr);
		zoned = true;
	}
	// 增伤乘区：特性的增伤加和 + 对静止敌人的增伤(都是加算)
	double staticAdd = chainSum > 0 ? staticSum / chainSum : 0.0;
	if (staticAdd > 0) staticPercent = static_cast<int>(lround(staticAdd * 100.0));
	{
		double dealt = me.Mods().All().empty() ? 0.0 : me.Mods().Sum(MOD_DMG_DEALT, attr, me);
		dealt += staticAdd;
		if (dealt != 0) { mult *= max(0.0, 1.0 + dealt); zoned = true; }
	}
	double rate = critBaseRate + (me.Mods().All().empty() ? 0.0 : me.Mods().Sum(MOD_CRIT_RATE, attr, me));
	if (rate > 0) {
		uniform_real_distribution<double> dist(0.0, 1.0);
		if (dist(rng) < rate) {
			double cd = max(0.0, critBaseDamage + (me.Mods().All().empty() ? 0.0 : me.Mods().Sum(MOD_CRIT_DMG, attr, me)));
			mult *= 1.0 + cd;
			critPercent = static_cast<int>(lround(cd * 100.0));
			zoned = true;
		}
	}

	if (!boosted && !added && !zoned) {
		act->SetPower(act->GetBasePower());
		return;
	}
	double power = (dual ? DUAL_POWER_FACTOR * fullSum : fullSum) * mult;
	act->SetPower(power >= 2147483647.0 ? 2147483647 : static_cast<int>(power));
}

// 护盾：所有盾一起减去同一个伤害值，穿透量=伤害-最大那块盾的有效容量；强度被打光的盾移除。
// 盾的五行属性覆盖这次伤害的属性时(同属性)，该盾扣除的量减半(容量翻倍，向上取整)；盾打光后剩下的伤害不再减半。
// 没有同属性的盾时和以前完全一样。
int Controller::ApplyShield(int object, int amount, ATTRIBUTE_TYPE attribute) {
	if (amount <= 0 || object < 0 || object >= static_cast<int>(effects.size())) return amount;
	auto keeps = [&](const shared_ptr<ShieldEffect>& s) { return ElementCovers(s->Mask(), attribute); };
	int maxCapacity = 0;
	for (auto& e : effects[object]) {
		if (e->GetType() != EFFECT_SHIELD) continue;
		auto shield = dynamic_pointer_cast<ShieldEffect>(e);
		long long capacity = keeps(shield) ? static_cast<long long>(lround(shield->Strength() / ELEM_SHIELD_KEEP)) : shield->Strength();
		maxCapacity = static_cast<int>(max<long long>(maxCapacity, min<long long>(capacity, 2147483647LL)));
	}
	if (maxCapacity <= 0) return amount;
	for (auto it = effects[object].begin(); it != effects[object].end(); ) {
		if ((*it)->GetType() == EFFECT_SHIELD) {
			auto shield = dynamic_pointer_cast<ShieldEffect>(*it);
			shield->Reduce(keeps(shield) ? static_cast<int>(ceil(amount * ELEM_SHIELD_KEEP)) : amount);
			if (shield->Strength() <= 0) { it = effects[object].erase(it); continue; }
		}
		++it;
	}
	int absorbed = min(amount, maxCapacity);
	logs.emplace_back(-1, object, absorbed, EFFECT_SHIELD);
	return amount - absorbed;
}

double Controller::ResistFactor(int object, ATTRIBUTE_TYPE attribute) const {
	if (object < 0 || object >= static_cast<int>(cultivators.size())) return 1.0;
	double x = cultivators[object]->Resist(attribute);
	return x == 1.0 ? 1.0 : 2.0 / (x + 1.0);
}

// ---- 特性 ----
void Controller::SetCritBase(double rate, double damage) {
	critBaseRate = rate;
	critBaseDamage = damage;
}

// 特性造成的资源增减(正=回复，负=扣除)，战报里写"资源|来源"
void Controller::ApplyTraitDelta(int holder, MOD_KIND resourceKind, int delta, const string& source) {
	if (delta == 0) return;
	Cultivator& c = *cultivators[holder];
	const char* resource = "mp";
	switch (resourceKind) {
	case MOD_MP_ROUND: case MOD_DEAL_MP: case MOD_TAKE_MP: case MOD_NOW_MP: c.AdjustMP(delta); resource = "mp"; break;
	case MOD_ATK_ROUND: case MOD_DEAL_ATK: case MOD_TAKE_ATK: case MOD_NOW_ATK: c.AdjustATK(delta); resource = "atk"; break;
	case MOD_HP_ROUND: case MOD_DEAL_HP: case MOD_TAKE_HP: case MOD_NOW_HP: c.AdjustHP(delta); resource = "hp"; break;
	default: return;
	}
	logs.emplace_back(holder, -1, delta, EFFECT_TRAIT);
	logs.back().name = string(resource) + "|" + source;
}

// 每回合类特性：真气/武力/健康的回复或扣除，按timing在回合开始或回合结束执行(武力/健康扣除会致死)
void Controller::ExecModifiers(STAGE_TYPE stage) {
	if (stage != STAGE_START && stage != STAGE_END) return;
	MOD_TIMING timing = stage == STAGE_START ? MOD_AT_START : MOD_AT_END;
	for (int i = 0; i < static_cast<int>(cultivators.size()); ++i) {
		Cultivator& c = *cultivators[i];
		if (c.Mods().All().empty()) continue;
		auto list = c.Mods().All();
		for (const auto& m : list) {
			if (m.timing != timing) continue;
			if (m.kind != MOD_MP_ROUND && m.kind != MOD_ATK_ROUND && m.kind != MOD_HP_ROUND) continue;
			if (!ModifierSet::CondHolds(m.cond, c)) continue;
			double base = m.kind == MOD_MP_ROUND ? c.GetMaxMP() : m.kind == MOD_ATK_ROUND ? c.GetMaxATK() : c.GetMaxHP();
			int delta = static_cast<int>(m.percent ? base * m.value / 100.0 : m.value);
			ApplyTraitDelta(i, m.kind, delta, m.source);
		}
	}
}

// 触发类特性：造成伤害时(持有者是出手方)、受到伤害时(持有者是受击方)回复或扣除真气/武力/健康
void Controller::ApplyDamageTriggers(int subject, int object, ATTRIBUTE_TYPE attribute, int damage) {
	if (damage <= 0) return;
	auto run = [&](int holder, MOD_KIND mp, MOD_KIND atk, MOD_KIND hp) {
		if (holder < 0 || holder >= static_cast<int>(cultivators.size())) return;
		Cultivator& c = *cultivators[holder];
		if (c.Mods().All().empty()) return;
		auto list = c.Mods().All();
		for (const auto& m : list) {
			if (m.kind != mp && m.kind != atk && m.kind != hp) continue;
			if (!(m.attrMask & (1u << static_cast<int>(attribute))) || !ModifierSet::CondHolds(m.cond, c)) continue;
			int delta = static_cast<int>(m.percent ? damage * m.value / 100.0 : m.value);
			ApplyTraitDelta(holder, m.kind, delta, m.source);
		}
	};
	if (subject >= 0 && subject != object) run(subject, MOD_DEAL_MP, MOD_DEAL_ATK, MOD_DEAL_HP);
	run(object, MOD_TAKE_MP, MOD_TAKE_ATK, MOD_TAKE_HP);
}

void Controller::AddModifier(int holder, const Modifier& m) {
	if (holder < 0 || holder >= static_cast<int>(cultivators.size())) return;
	Cultivator& c = *cultivators[holder];
	switch (m.kind) {
	case MOD_NOW_MP: case MOD_NOW_ATK: case MOD_NOW_HP: {
		double base = m.kind == MOD_NOW_MP ? c.GetMaxMP() : m.kind == MOD_NOW_ATK ? c.GetMaxATK() : c.GetMaxHP();
		ApplyTraitDelta(holder, m.kind, static_cast<int>(m.percent ? base * m.value / 100.0 : m.value), m.source);
		return;
	}
	case MOD_NOW_AGI: {
		double delta = m.percent ? c.GetMaxAGI() * m.value / 100.0 : m.value;
		c.AdjustAGI(static_cast<float>(delta));
		logs.emplace_back(holder, -1, static_cast<int>(delta), EFFECT_TRAIT);
		logs.back().name = "agi|" + m.source;
		return;
	}
	case MOD_NOW_FIELD:
		AddReactionEnvironment(static_cast<int>(m.value));
		return;
	case MOD_CLEAR_FIELD:
		environment.clear();
		return;
	case MOD_NOW_SHIELD: {
		// attr=五行(all)等价于五个属性齐全；none或不写attr=无属性盾；dur=-1不随回合衰减
		unsigned mask = 0;
		if (m.attrMask != MOD_ATTR_ALL)   // 没写attr(默认全部)=无属性盾
		for (int a = ATTRIBUTE_METAL; a <= ATTRIBUTE_ALL; ++a)
			if (m.attrMask & (1u << a)) mask |= ElementBit(static_cast<ATTRIBUTE_TYPE>(a));
		int strength = static_cast<int>(m.value);
		if (strength <= 0) return;
		auto s = make_shared<ShieldEffect>(strength, m.duration < 0 ? (1 << 30) : m.duration, mask);
		logs.emplace_back(holder, -1, strength, EFFECT_SHIELD);
		AddEffect(holder, s);
		return;
	}
	case MOD_UNLOCK:
		status[holder] &= ~0x01;
		effects[holder].erase(remove_if(effects[holder].begin(), effects[holder].end(),
			[](const shared_ptr<Effect>& e) { return e->GetType() == EFFECT_LOCK; }), effects[holder].end());
		return;
	default:
		c.Mods().Add(m);
	}
}

void Controller::RemoveModifiers(int holder, const string& source) {
	if (holder < 0 || holder >= static_cast<int>(cultivators.size())) return;
	cultivators[holder]->Mods().RemoveBySource(source);
}

double Controller::DodgeProb(int attacker, int defender, ATTRIBUTE_TYPE attribute, float defenderAGI) {
	bool rust = FindReaction(EFFECT_RUST);
	const Cultivator& d = *cultivators[defender];
	double adj = d.Mods().All().empty() ? 0.0 : d.Mods().Sum(MOD_DODGE, attribute, d);
	// 闪避率乘区：1 + 特性加和 - 锈蚀(1)；没有特性时锈蚀就是不能闪避
	if (adj == 0) return rust ? 0 : DodgeChance(cultivators[attacker]->GetAGI(), defenderAGI);
	double zone = 1.0 + adj - (rust ? 1.0 : 0.0);
	return min(1.0, max(0.0, DodgeChance(cultivators[attacker]->GetAGI(), defenderAGI) * zone));
}

// 易伤(dmg_taken)特性对attr属性的加和，只增不减(负数不生效)；没有特性时是0
// 另外这一回合在积攒炸弹的人，固定多一份ACCUMULATE_VULNERABILITY(所有属性、所有伤害来源，包括炸弹和持续伤害)
double Controller::Vulnerability(int object, ATTRIBUTE_TYPE attribute) const {
	if (object < 0 || object >= static_cast<int>(cultivators.size())) return 0.0;
	double v = (status[object] & 0x02) ? ACCUMULATE_VULNERABILITY : 0.0;
	if (cultivators[object]->Mods().All().empty()) return v;
	return v + max(0.0, cultivators[object]->Mods().Sum(MOD_DMG_TAKEN, attribute, *cultivators[object]));
}

bool Controller::RollHit(int attacker, ATTRIBUTE_TYPE attribute) {
	const Cultivator& a = *cultivators[attacker];
	double adj = a.Mods().All().empty() ? 0.0 : a.Mods().Sum(MOD_HIT, attribute, a);
	// 命中率乘区：1 + 特性加和 - 扬尘(0.5)；够1就必中，不抽随机数
	double zone = 1.0 + adj - (FindReaction(EFFECT_DUST) ? 0.5 : 0.0);
	if (zone >= 1.0) return true;
	if (zone <= 0.0) return false;
	uniform_real_distribution<double> dist(0.0, 1.0);
	return dist(rng) < zone;
}

// 跳过要损失敏捷：付得起(之后不低于1)就记下翻倍后的闪避敏捷，付不起就标记这回合没有闪避
void Controller::PaySkip(int idx) {
	Cultivator& c = *cultivators[idx];
	if (c.CanDodge()) {
		skipAgi[idx] = c.GetAGI() * SKIP_AGI_FACTOR;
		c.ConsumeAGI();
	}
	else {
		// 敏捷不够付这次跳过：不翻倍、不扣敏捷，但仍用自己原来的敏捷闪避(被禁锢也一样)
		skipAgi[idx] = c.GetAGI();
	}
}

void Controller::DodgeSuccess(int subject, int object, bool log) {
	if (log) logs.emplace_back(subject, object);
	cultivators[object]->DodgeSuccess();
}

bool Controller::CheckCounter(ATTRIBUTE_TYPE a1, ATTRIBUTE_TYPE a2) {
	return ATTRIBUTE_COUNTERS.find({ a1, a2 }) != ATTRIBUTE_COUNTERS.end();
}

double Controller::DodgeChance(float attackerAGI, float defenderAGI) {
	return static_cast<double>(defenderAGI * defenderAGI) /
		(attackerAGI * attackerAGI + defenderAGI * defenderAGI);
}

int Controller::CheckReaction(ATTRIBUTE_TYPE a1, ATTRIBUTE_TYPE a2) {
	if (a1 == ATTRIBUTE_METAL) {
		if (a2 == ATTRIBUTE_FIRE) return EFFECT_BURN; // 金火 → 灼烧
		if (a2 == ATTRIBUTE_WATER) return EFFECT_RUST; // 金水 → 锈蚀
	}
	if (a1 == ATTRIBUTE_EARTH) {
		if (a2 == ATTRIBUTE_FIRE) return EFFECT_DUST; // 土火 → 扬尘
		if (a2 == ATTRIBUTE_WATER) return EFFECT_MUD; // 土水 → 泥沼
	}
	if (a1 == ATTRIBUTE_FIRE) {
		if (a2 == ATTRIBUTE_METAL) return EFFECT_BURN; // 火金 → 灼烧
		if (a2 == ATTRIBUTE_EARTH) return EFFECT_DUST; // 火土 → 扬尘
	}
	if (a1 == ATTRIBUTE_WATER) {
		if (a2 == ATTRIBUTE_METAL) return EFFECT_RUST; // 水金 → 锈蚀
		if (a2 == ATTRIBUTE_EARTH) return EFFECT_MUD; // 水土 → 泥沼
	}
	return EFFECT_NONE;
}

bool Controller::FindReaction(EFFECT_TYPE reaction) {
	auto it = find_if(environment.begin(), environment.end(),
		[reaction](shared_ptr<Effect> e) { return e->GetType() == reaction; });
	return it != environment.end();
}

void Controller::PutoutReaction(ATTRIBUTE_TYPE attribute) {
	switch (attribute) {
	case ATTRIBUTE_METAL:
		for (auto it = environment.begin(); it != environment.end(); ) {
			if ((*it)->GetType() == EFFECT_DUST || (*it)->GetType() == EFFECT_MUD)
				it = environment.erase(it);
			else ++it;
		}
		break;
	case ATTRIBUTE_WATER:
		for (auto it = environment.begin(); it != environment.end(); ) {
			if ((*it)->GetType() == EFFECT_BURN || (*it)->GetType() == EFFECT_DUST)
				it = environment.erase(it);
			else ++it;
		}
		break;
	case ATTRIBUTE_FIRE:
		for (auto it = environment.begin(); it != environment.end(); ) {
			if ((*it)->GetType() == EFFECT_RUST || (*it)->GetType() == EFFECT_MUD)
				it = environment.erase(it);
			else ++it;
		}
		break;
	case ATTRIBUTE_EARTH:
		for (auto it = environment.begin(); it != environment.end(); ) {
			if ((*it)->GetType() == EFFECT_BURN || (*it)->GetType() == EFFECT_RUST)
				it = environment.erase(it);
			else ++it;
		}
		break;
	default:
		break;
	}
}

}
