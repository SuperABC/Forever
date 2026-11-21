// 月末选举：全体在城成年人按"各党提议 + 自己的身份"投票，胜者成为下个月的执政党。设计见polit.md"选举"一节。
#include "polit/polit.h"

#include "common/random.h"
#include "populace/populace.h"
#include "populace/citizen.h"
#include "populace/household.h"
#include "society/society.h"
#include "society/emigration.h"
#include "polit/policy_ids.h"
#include "polit/polit_log.h"

#include <algorithm>
#include <cmath>


using namespace std;

// ---- 投票模型参数(占位初值，等观察后调) ----
// 每条提议先换算成相对当前值的比例r并夹到[RATIO_MIN, RATIO_MAX]；选民对一项政策的利益权重w按身份算：
//   税类    w = -(TAX_BASE + TAX_WEALTH*pct)                         越富越反对加税
//   福利类  w = (1-pct) * (BENEFIT_OTHER + (1-BENEFIT_OTHER)*受益)    越穷越支持，本项受益人(退休/失业/户里有未成年)更支持
//   移民    w = (pct-0.5)*2                                          富人欢迎，穷人排斥
// 效用 u = Σ w*(r-1)，支持度 S = exp(VOTE_K*u) * (当前执政党 ? 1/(1+FATIGUE*连任期数) : 1)，每人投S最大的党。
// pct=该市民净资产在全体选民里的百分位(0最穷~1最富)。
#define POLIT_RATIO_MIN 0.9
#define POLIT_RATIO_MAX 1.1
#define POLIT_TAX_BASE 0.1
#define POLIT_TAX_WEALTH 0.3
#define POLIT_BENEFIT_OTHER 0.5
#define POLIT_VOTE_K 4.0
#define POLIT_FATIGUE 0.1

namespace {
	struct Voter {
		Citizen* citizen = nullptr;
		double pct = 0.5;
		bool retired = false;
		bool unemployed = false;
		bool hasMinor = false;
	};

	// 福利类政策对这个选民是否"受益人"。
	bool Beneficiary(const string& policyId, const Voter& voter) {
		if (policyId == PolicyId::kRetirementPension) return voter.retired;
		if (policyId == PolicyId::kUnemploymentBenefit) return voter.unemployed;
		if (policyId == PolicyId::kMinorAllowance) return voter.hasMinor;
		return false;
	}

	double Weight(const PolicyDef& def, const Voter& voter) {
		switch (def.kind) {
		case PolicyDef::TAX:
			return -(POLIT_TAX_BASE + POLIT_TAX_WEALTH * voter.pct);
		case PolicyDef::BENEFIT:
			return (1.0 - voter.pct) * (POLIT_BENEFIT_OTHER + (1.0 - POLIT_BENEFIT_OTHER) * (Beneficiary(def.id, voter) ? 1.0 : 0.0));
		case PolicyDef::IMMIGRATION:
			return (voter.pct - 0.5) * 2.0;
		}
		return 0.0;
	}
}

vector<double> Polit::ComputeRatios(Party& party, const Time& now) {
	const vector<PolicyDef>& defs = PolicyCatalog::All();
	vector<PolicyValue> current;
	for (const PolicyDef& def : defs) current.push_back({ def.id, def.get(policy) });

	PartyMod* mod = party.GetMod();
	if (mod) {
		RandomScope scope(RandomRegistry::GetInstanceStream(mod));
		mod->Propose(current);
	}
	vector<PolicyTarget> targets = mod ? mod->targets : vector<PolicyTarget>();

	vector<double> ratios(defs.size(), 1.0);
	for (const PolicyTarget& target : targets) {
		size_t index = defs.size();
		for (size_t i = 0; i < defs.size(); i++) {
			if (defs[i].id == target.policy) index = i;
		}
		if (index == defs.size()) {
			PolitLog(now, "proposal_rejected", "party=" + party.GetId() + " policy=" + target.policy + " reason=unknown_policy");
			continue;
		}
		double base = current[index].value;
		double ratio = base > 0.0 ? target.target / base : 1.0;
		double clamped = max(POLIT_RATIO_MIN, min(POLIT_RATIO_MAX, ratio));
		ratios[index] = clamped;
		PolitLog(now, "proposal", "party=" + party.GetId() + " policy=" + target.policy + " current=" + PolitNum(base) +
			" target=" + PolitNum(target.target) + " ratio=" + PolitNum(ratio) + " clamped=" + PolitNum(clamped));
	}
	return ratios;
}

void Polit::ApplyRatios(const vector<double>& ratios, const Time& now) {
	const vector<PolicyDef>& defs = PolicyCatalog::All();
	for (size_t i = 0; i < defs.size(); i++) {
		const PolicyDef& def = defs[i];
		double oldValue = def.get(policy);
		double wanted = oldValue * ratios[i];
		double applied = PolicyCatalog::Apply(policy, def, wanted);
		string limit = "none";
		if (wanted < def.minValue - 1e-9) limit = "min";
		else if (wanted > def.maxValue + 1e-9) limit = "max";
		PolitLog(now, "policy", "id=" + def.id + " old=" + PolitNum(oldValue) + " ratio=" + PolitNum(ratios[i]) +
			" new=" + PolitNum(applied) + " limit=" + limit);
	}
}

void Polit::HoldElection(const Time& now) {
	CORE_RANDOM_SCOPE("core.polit");
	lastElection = ElectionSummary();
	lastElection.monthKey = MonthKeyOf(now);
	if (parties.empty() || !populace || !society) {
		PolitLog(now, "election", "skipped reason=no_parties");
		return;
	}
	for (const Party* party : parties) lastElection.partyIds.push_back(party->GetId());
	lastElection.votes.assign(parties.size(), 0);

	// 选民：在城成年人；按净资产排名算百分位。
	vector<Voter> voters;
	for (Citizen* citizen : populace->GetCitizens()) {
		if (!citizen || !citizen->IsActive() || citizen->GetAge(now) < policy.adultAge) continue;
		Voter voter;
		voter.citizen = citizen;
		int age = citizen->GetAge(now);
		voter.retired = age >= policy.retirementAge;
		voter.unemployed = !voter.retired && !citizen->GetJob();
		if (citizen->GetHousehold()) {
			for (const Citizen* member : citizen->GetHousehold()->GetMembers()) {
				if (member && member->IsActive() && member->GetAge(now) < policy.adultAge) voter.hasMinor = true;
			}
		}
		voters.push_back(voter);
	}
	lastElection.voters = static_cast<int>(voters.size());
	PolitLog(now, "election", "begin voters=" + to_string(voters.size()) + " parties=" + to_string(parties.size()));
	if (voters.empty()) {
		PolitLog(now, "election", "skipped reason=no_voters");
		return;
	}

	vector<pair<int64_t, size_t>> ranking;
	for (size_t i = 0; i < voters.size(); i++) ranking.push_back({ Emigration::NetWorth(*society, voters[i].citizen), i });
	stable_sort(ranking.begin(), ranking.end(), [](const pair<int64_t, size_t>& a, const pair<int64_t, size_t>& b) { return a.first < b.first; });
	for (size_t rank = 0; rank < ranking.size(); rank++) {
		voters[ranking[rank].second].pct = ranking.size() > 1 ? static_cast<double>(rank) / (ranking.size() - 1) : 0.5;
	}

	// 各党的提议比例。
	const vector<PolicyDef>& defs = PolicyCatalog::All();
	vector<vector<double>> ratios;
	for (Party* party : parties) ratios.push_back(ComputeRatios(*party, now));

	// 投票。
	for (const Voter& voter : voters) {
		vector<double> support(parties.size(), 0.0);
		for (size_t p = 0; p < parties.size(); p++) {
			double utility = 0.0;
			for (size_t i = 0; i < defs.size(); i++) {
				double delta = ratios[p][i] - 1.0;
				if (delta != 0.0) utility += Weight(defs[i], voter) * delta;
			}
			double s = exp(POLIT_VOTE_K * utility);
			if (parties[p]->ruling) s /= (1.0 + POLIT_FATIGUE * parties[p]->terms);
			support[p] = s;
		}
		double best = *max_element(support.begin(), support.end());
		vector<size_t> top;
		for (size_t p = 0; p < parties.size(); p++) {
			if (support[p] >= best - 1e-12) top.push_back(p);
		}
		size_t choice = top.size() == 1 ? top[0] : top[static_cast<size_t>(GetRandom(static_cast<int>(top.size())))];
		lastElection.votes[choice]++;
	}

	int mostVotes = *max_element(lastElection.votes.begin(), lastElection.votes.end());
	vector<size_t> leaders;
	for (size_t p = 0; p < parties.size(); p++) {
		PolitLog(now, "votes", "party=" + parties[p]->GetId() + " count=" + to_string(lastElection.votes[p]) +
			" ruling=" + (parties[p]->ruling ? "1" : "0") + " terms=" + to_string(parties[p]->terms));
		if (lastElection.votes[p] == mostVotes) leaders.push_back(p);
	}
	lastElection.tied = leaders.size() > 1;
	size_t winnerIndex = leaders.size() == 1 ? leaders[0] : leaders[static_cast<size_t>(GetRandom(static_cast<int>(leaders.size())))];
	lastElection.winner = static_cast<int>(winnerIndex);
	Party* winner = parties[winnerIndex];

	// 执政党更替：连任则期数+1，否则新党期数=1、旧执政党清空权力。
	for (Party* party : parties) {
		if (party != winner && party->ruling) {
			party->ruling = false;
			party->terms = 0;
			party->powers.clear();
			PolitLog(now, "ruling_lost", "party=" + party->GetId());
		}
	}
	winner->terms = winner->ruling ? winner->terms + 1 : 1;
	winner->ruling = true;
	PolitLog(now, "election", "winner=" + winner->GetId() + " votes=" + to_string(mostVotes) + " tied=" + (lastElection.tied ? "1" : "0") +
		" terms=" + to_string(winner->terms));

	ApplyRatios(ratios[winnerIndex], now);
	AssignPowers(*winner, now);
}
