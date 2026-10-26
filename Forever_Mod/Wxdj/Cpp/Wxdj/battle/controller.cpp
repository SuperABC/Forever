#include "controller.h"

#include <stdexcept>
#include <algorithm>
#include <cmath>
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

	for (int i = 0; i < (int)cultivators.size(); ++i) {
		auto& act = actions[i].first;
		int target = actions[i].second;

		if (act->GetType() == ACTION_SKIP) {
			SkipMove(i);
			PaySkip(i);
		}
		else if (act->GetType() == ACTION_ACCUMULATE) {
			MakeMove(i, target, act->GetPoint(), act->GetText(), act->GetPower());
			cultivators[i]->AccumulatePower();
			cultivators[i]->ResetSkipStreak();
		}
		else if (status[i] & 0x01) {
			SkipMove(i);
			PaySkip(i);
		}
		else if (act->GetType() == ACTION_BOMB && cultivators[i]->GetBomb() <= 0) {
			// 空炸弹：没有积攒过就释放，等于一次"纯跳过"——不翻倍敏捷、不损失敏捷(闪避照常用自己的敏捷)，也不出招；
			// 代价是这个回合不回复真气(把回合开始自然回复的真气撤销)
			SkipMove(i);
			cultivators[i]->UndoMP(regenMP[i]);
		}
		else if (act->GetType() == ACTION_BOMB) {
			// 战报里写明这次释放的炸弹能量(出招时才取出，所以先取再记)
			int bombPower = cultivators[i]->BombPower();
			MakeMove(i, target, act->GetPoint(), act->GetText() + "(能量" + to_string(bombPower) + ")", act->GetPower());
			dynamic_pointer_cast<BombAction>(act)->SetPower(bombPower);
			cultivators[i]->ResetSkipStreak();
		}
		else {
			cultivators[i]->ResetSkipStreak();
			MakeMove(i, target, act->GetPoint(), act->GetText(), act->GetPower());

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
				double dodgeChance = FindReaction(EFFECT_RUST) ? 0 : DodgeChance(cultivators[i]->GetAGI(), defenderAGI);
				if (canDodge && dist(rng) < dodgeChance) {
					DodgeSuccess(i, j);
					BuildDualEnvironment(act_i);   // 双招被闪避=未命中，它的效果计入场地效果
					continue;
				}
				else {
					bool hit = true;
					if (FindReaction(EFFECT_DUST)) {
						hit = (dist(rng) < 0.5f);
					}
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

				bool dust = FindReaction(EFFECT_DUST);
				if (point1 > point2) {
					bool hit = dust ? (dist(rng) < 0.5f) : true;
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
					bool hit = dust ? (dist(rng) < 0.5f) : true;
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
					bool hit = dust ? (dist(rng) < 0.5f) : true;
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

	for (auto& c : cultivators) c->UpdateRealm();
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
	for (int i = 0; i < (int)effects.size(); ++i) {
		for (int j = 0; j < (int)effects[i].size(); ++j) {
			auto& eff = effects[i][j];
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
	if (FindReaction(EFFECT_BURN)) amount = static_cast<int>(amount * 1.5f);

	auto reduce = find_if(defend.begin(), defend.end(),
		[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_REDUCE; });
	if (reduce != defend.end()) {
		amount = static_cast<int>(amount * (1.0f - dynamic_pointer_cast<ReduceEffect>(*reduce)->GetRatio(attribute)));
	}

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
	if (subject != object) {
		auto rebound = find_if(defend.begin(), defend.end(),
			[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_REBOUND; });
		if (rebound != defend.end()) {
			float ratio = min(1.f, max(0.f, dynamic_pointer_cast<ReboundEffect>(*rebound)->GetRatio(attribute)));
			reboundAtk = static_cast<int>(atkPart * ratio);
			reboundHp = static_cast<int>(hpPart * ratio);
		}
	}
	int ownAtk = atkPart - reboundAtk, ownHp = hpPart - reboundHp;

	if (log) logs.emplace_back(subject, object, attribute, ownAtk, ownHp);
	if (ownAtk > 0) cultivators[object]->TakeDamage(ownAtk, false);
	if (ownHp > 0) cultivators[object]->TakeDamage(ownHp, true);

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
	if (reduce != defend.end()) {
		float keep = 1.0f - dynamic_pointer_cast<ReduceEffect>(*reduce)->GetRatio(attribute);
		atk = static_cast<int>(atk * keep);
		hp = static_cast<int>(hp * keep);
	}

	// 反弹是减伤的升级版：只承受反弹之后剩下的伤害
	int reboundAtk = 0, reboundHp = 0;
	if (subject != object) {
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
