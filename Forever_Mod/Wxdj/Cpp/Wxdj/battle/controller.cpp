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
	mark(0),
	logs(),
	rng(std::random_device{}()) {

}

Controller::Controller(std::vector<std::shared_ptr<Cultivator>> cultivators) :
	cultivators(cultivators),
	effects(this->cultivators.size()),
	environment(),
	status(this->cultivators.size()),
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
}

shared_ptr<Cultivator> Controller::GetCultivator(int idx) const {
	if (idx < 0 || idx >= (int)cultivators.size())
		throw runtime_error("Cultivator index out of range.");
	return cultivators[idx];
}

void Controller::ResetRound() {
	for (auto& s : status) s = 0;
}

void Controller::StartTurn() {
	ResetRound();
	ExecEffect(STAGE_START);
	for (int i = 0; i < (int)cultivators.size(); ++i) {
		RecoverMP(i, -1);
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
		}
		else if (act->GetType() == ACTION_ACCUMULATE) {
			MakeMove(i, target, act->GetPoint(), act->GetText(), act->GetPower());
			cultivators[i]->AccumulatePower();
		}
		else if (status[i] & 0x01) {
			SkipMove(i);
		}
		else if (act->GetType() == ACTION_BOMB) {
			MakeMove(i, target, act->GetPoint(), act->GetText(), act->GetPower());
			dynamic_pointer_cast<BombAction>(act)->SetPower(cultivators[i]->BombPower());
		}
		else {
			MakeMove(i, target, act->GetPoint(), act->GetText(), act->GetPower());

			auto dot = act->GetEffect(EFFECT_DOT);
			if (dot && !dynamic_pointer_cast<DotEffect>(dot)->NeedHit()) {
				int amount = dynamic_pointer_cast<DotEffect>(dot)->DotInstant();
				logs.emplace_back(i, target, amount, EFFECT_DOT);
				TakeDamage(i, target, act->GetEffects(), actions[target].first->GetEffects(),
					act->GetAttribute(), amount);
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
				float powerEnemy = act_i->GetPower() * (FindReaction(EFFECT_RUST) ? 1.0f :
					((1.f / (cultivators[j]->GetAGI() * cultivators[j]->GetAGI() + 1.0f) + 1.f) * 0.8f));
				float powerSelf = act_i->GetPower() * (FindReaction(EFFECT_RUST) ? 1.0f :
					((1.f / (cultivators[i]->GetAGI() * cultivators[j]->GetAGI() + 1.0f) + 1.f) * 0.4f));
				TakeDamage(i, j, act_i->GetEffects(), act_j->GetEffects(),
					act_i->GetAttribute(), (int)powerEnemy);
				TakeDamage(i, i, act_i->GetEffects(), act_i->GetEffects(),
					act_i->GetAttribute(), (int)powerSelf);
			}
			else if (act_j->GetType() == ACTION_BOMB && actions[j].second == (int)i) {
				// 双方互扔真气炸弹，目标 j 受到合成伤害
				float power = sqrtf(powf((float)act_i->GetPower(), 2.f) + powf((float)act_j->GetPower(), 2.f));
				float powerEnemy = power * (FindReaction(EFFECT_RUST) ? 1.0f :
					((1.f / (cultivators[j]->GetAGI() * cultivators[j]->GetAGI() + 1.0f) + 1.f) * 0.8f));
				TakeDamage(j, j, act_j->GetEffects(), act_j->GetEffects(),
					act_j->GetAttribute(), (int)powerEnemy);
			}
			else {
				// 目标 j 施放普通招式，若真气炸弹威力更大，则目标 j 受到差额伤害
				if (act_j->GetPower() < act_i->GetPower()) {
					float powerEnemy = (float)(act_i->GetPower() - act_j->GetPower());
					powerEnemy *= (FindReaction(EFFECT_RUST) ? 1.0f :
						((1.f / (cultivators[j]->GetAGI() * cultivators[j]->GetAGI() + 1.0f) + 1.f) * 0.8f));
					TakeDamage(i, j, act_i->GetEffects(), act_j->GetEffects(),
						act_i->GetAttribute(), (int)powerEnemy);
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
				double dodgeChance = FindReaction(EFFECT_RUST) ? 0 : DodgeChance(cultivators[i]->GetAGI(), cultivators[j]->GetAGI());
				if (!(status[j] & 0x01) && dist(rng) < dodgeChance) {
					DodgeSuccess(i, j);
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
							act_i->GetAttribute(), act_i->GetPower());
					}
					else {
						HitFail(i);
					}
				}
			}
			else if (act_j->GetType() == ACTION_BOMB && actions[j].second == (int)i) {
				// 目标 j 施放真气炸弹且指向玩家 i，只计算目标 j 受到的伤害
				float powerEnemy = 0.f;
				if (act_i->GetPower() < act_j->GetPower()) {
					powerEnemy = (float)act_j->GetPower();
					powerEnemy *= (FindReaction(EFFECT_RUST) ? 1.0f :
						((1.f / (cultivators[j]->GetAGI() * cultivators[j]->GetAGI() + 1.0f) + 1.f) * 0.4f));
					TakeDamage(j, j, act_j->GetEffects(), act_j->GetEffects(),
						act_j->GetAttribute(), (int)powerEnemy);
				}
				else if (act_i->GetPower() > 2 * act_j->GetPower()) {
					powerEnemy = (float)(act_i->GetPower() - 2 * act_j->GetPower());
					TakeDamage(i, j, act_i->GetEffects(), act_j->GetEffects(),
						act_i->GetAttribute(), (int)powerEnemy);
				}
				else {
					powerEnemy = (float)(2 * act_j->GetPower() - act_i->GetPower());
					powerEnemy *= (FindReaction(EFFECT_RUST) ? 1.0f :
						((1.f / (cultivators[j]->GetAGI() * cultivators[j]->GetAGI() + 1.0f) + 1.f) * 0.4f));
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

				switch (CheckReaction(attr1, attr2)) {
				case EFFECT_BURN:
					AddEnvironment(make_shared<BurnEffect>());
					break;
				case EFFECT_RUST:
					AddEnvironment(make_shared<RustEffect>());
					break;
				case EFFECT_DUST:
					AddEnvironment(make_shared<DustEffect>());
					break;
				case EFFECT_MUD:
					AddEnvironment(make_shared<MudEffect>());
					break;
				default: break;
				}

				bool dust = FindReaction(EFFECT_DUST);
				if (point1 > point2) {
					bool hit = dust ? (dist(rng) < 0.5f) : true;
					if (hit) {
						HitSuccess(i, j, act_i, act_j);
						HitSuccess(j, i, act_j, act_i);
						TakeDamage(i, j, act_i->GetEffects(), act_j->GetEffects(),
							attr1, point1 - point2);
						if (auto effect = act_i->GetEffect(EFFECT_PENETRATE)) {
							int pen = (int)(point2 * (dynamic_pointer_cast<PenetrateEffect>(effect)->GetRatio(attr2)));
							logs.emplace_back(i, j, attr1, pen, 0, EFFECT_PENETRATE);
							TakeDamage(i, j, act_i->GetEffects(), act_j->GetEffects(),
								attr1, pen, false);
						}
						if (auto effect = act_j->GetEffect(EFFECT_PENETRATE)) {
							int pen = (int)(point2 * (dynamic_pointer_cast<PenetrateEffect>(effect)->GetRatio(attr1)));
							logs.emplace_back(j, i, attr2, pen, 0, EFFECT_PENETRATE);
							TakeDamage(j, i, act_j->GetEffects(), act_i->GetEffects(),
								attr2, pen, false);
						}
					}
					else {
						HitFail(i);
					}
				}
				else if (point2 > point1) {
					bool hit = dust ? (dist(rng) < 0.5f) : true;
					if (hit) {
						HitSuccess(i, j, act_i, act_j);
						HitSuccess(j, i, act_j, act_i);
						TakeDamage(j, i, act_j->GetEffects(), act_i->GetEffects(),
							attr2, point2 - point1);
						if (auto effect = act_i->GetEffect(EFFECT_PENETRATE)) {
							int pen = (int)(point1 * (dynamic_pointer_cast<PenetrateEffect>(effect)->GetRatio(attr2)));
							logs.emplace_back(i, j, attr1, pen, 0, EFFECT_PENETRATE);
							TakeDamage(i, j, act_i->GetEffects(), act_j->GetEffects(),
								attr1, pen, false);
						}
						if (auto effect = act_j->GetEffect(EFFECT_PENETRATE)) {
							int pen = (int)(point1 * (dynamic_pointer_cast<PenetrateEffect>(effect)->GetRatio(attr1)));
							logs.emplace_back(j, i, attr2, pen, 0, EFFECT_PENETRATE);
							TakeDamage(j, i, act_j->GetEffects(), act_i->GetEffects(),
								attr2, pen, false);
						}
					}
					else {
						HitFail(i);
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
								attr1, pen, false);
						}
						if (auto effect = act_j->GetEffect(EFFECT_PENETRATE)) {
							int pen = (int)(point1 * (dynamic_pointer_cast<PenetrateEffect>(effect)->GetRatio(attr1)));
							logs.emplace_back(j, i, attr2, pen, 0, EFFECT_PENETRATE);
							TakeDamage(j, i, act_j->GetEffects(), act_i->GetEffects(),
								attr2, pen, false);
						}
					}
					else {
						HitFail(i);
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
			offend->GetAttribute(), amount);
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
	ATTRIBUTE_TYPE attribute, int amount, bool log) {
	if (FindReaction(EFFECT_BURN)) amount = static_cast<int>(amount * 1.5f);

	auto reduce = find_if(defend.begin(), defend.end(),
		[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_REDUCE; });
	if (reduce != defend.end()) {
		amount = static_cast<int>(amount * (1.0f - dynamic_pointer_cast<ReduceEffect>(*reduce)->GetRatio(attribute)));
	}

	bool physical = (find_if(offend.begin(), offend.end(),
		[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_PHYSICAL; }) != offend.end());
	if (log) {
		logs.emplace_back(subject, object, attribute,
			physical ? 0 : amount,
			physical ? amount : 0);
	}
	cultivators[object]->TakeDamage(amount, physical);

	if (subject == object) return;

	auto rebound = find_if(defend.begin(), defend.end(),
		[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_REBOUND; });
	if (rebound != defend.end()) {
		int dmg = static_cast<int>(amount * dynamic_pointer_cast<ReboundEffect>(*rebound)->GetRatio(attribute));
		if (dmg > 0) {
			logs.emplace_back(object, subject, attribute,
				physical ? 0 : dmg,
				physical ? dmg : 0, EFFECT_REBOUND);
			TakeDamage(object, subject, defend, offend, attribute,
				physical ? 0 : dmg, physical ? dmg : 0, false);
		}
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
	int amount = (atk > 0) ? atk : hp;
	bool isPhysical = (hp > 0);

	if (FindReaction(EFFECT_BURN)) amount = static_cast<int>(amount * 1.5f);

	if (log) logs.emplace_back(subject, object, attribute, atk, hp);
	cultivators[object]->TakeDamage(amount, isPhysical);

	auto reduce = find_if(defend.begin(), defend.end(),
		[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_REDUCE; });
	if (reduce != defend.end()) {
		amount = static_cast<int>(amount * (1.0f - dynamic_pointer_cast<ReduceEffect>(*reduce)->GetRatio(attribute)));
	}

	auto rebound = find_if(defend.begin(), defend.end(),
		[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_REBOUND; });
	if (rebound != defend.end()) {
		int dmg = static_cast<int>(amount * dynamic_pointer_cast<ReboundEffect>(*rebound)->GetRatio(attribute));
		if (dmg > 0) {
			logs.emplace_back(object, subject, attribute,
				isPhysical ? 0 : dmg,
				isPhysical ? dmg : 0, EFFECT_REBOUND);
			TakeDamage(object, subject, defend, offend, attribute,
				isPhysical ? 0 : dmg, isPhysical ? dmg : 0, false);
		}
	}

	if (subject == object) return;

	auto absorb = find_if(defend.begin(), defend.end(),
		[](shared_ptr<Effect> e) { return e->GetType() == EFFECT_ABSORB; });
	if (absorb != defend.end()) {
		int mp = static_cast<int>(amount * dynamic_pointer_cast<AbsorbEffect>(*absorb)->GetRatio(attribute));
		logs.emplace_back(object, subject, mp, EFFECT_ABSORB);
		RecoverMP(object, mp);
	}
}

void Controller::DodgeSuccess(int subject, int object, bool log) {
	if (log) logs.emplace_back(subject, object);
	cultivators[object]->DodgeSuccess();
}

bool Controller::CheckCounter(ATTRIBUTE_TYPE a1, ATTRIBUTE_TYPE a2) {
	return ATTRIBUTE_COUNTERS.find({ a1, a2 }) != ATTRIBUTE_COUNTERS.end();
}

double Controller::DodgeChance(int attackerAGI, int defenderAGI) {
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
