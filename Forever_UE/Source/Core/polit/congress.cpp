// 党代会：每月由党派mod给出日程，开始/结束时通知党员，结束时统计到场者并投票选党首。设计见polit.md"党代会"一节。
#include "polit/polit.h"

#include "common/error.h"

#include "common/random.h"
#include "map/room.h"
#include "populace/citizen.h"
#include "polit/polit_log.h"

#include <algorithm>


using namespace std;

namespace {
	int DaysInMonth(int year, int month) {
		static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
		if (month < 1 || month > 12) return 30;
		bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
		return days[month - 1] + (month == 2 && leap ? 1 : 0);
	}

	Time AtDay(int year, int month, int day, const Time& clock) {
		return Time(year, month, min(max(day, 1), DaysInMonth(year, month)), clock.GetHour(), clock.GetMinute(), clock.GetSecond());
	}
}

void Polit::PushNotice(const Party& party, bool start) {
	MeetingNotice notice;
	notice.party = party.GetId();
	notice.start = start;
	notice.address = party.meetingAddress;
	for (Citizen* member : party.members) {
		if (member && member->IsActive()) notice.members.push_back(member);
	}
	notices.push_back(std::move(notice));
}

void Polit::UpdateCongress(Party& party, const Time& now) {
	int monthKey = MonthKeyOf(now);
	PartyMod* mod = party.GetMod();
	if (!mod) return;

	// 月份变了：重新取本月日程，进度清零。
	if (party.congress.monthKey != monthKey) {
		party.congress = Party::Congress();
		party.congress.monthKey = monthKey;
		{
			RandomScope scope(RandomRegistry::GetInstanceStream(mod));
			party.congress.plan = MOD_CALL(mod, mod->GetCongress(now.GetYear(), now.GetMonth()));
		}
		const PartyCongress& plan = party.congress.plan;
		PolitLog(now, "congress_plan", "party=" + party.GetId() + " day=" + to_string(plan.day) +
			" start=" + plan.start.Format("HH:mm") + " end=" + plan.end.Format("HH:mm"));
	}

	const PartyCongress& plan = party.congress.plan;
	if (plan.day <= 0) return;
	if (party.ruling) {
		if (!party.congress.skipLogged) {
			party.congress.skipLogged = true;
			PolitLog(now, "congress_skipped", "party=" + party.GetId() + " reason=ruling");
		}
		return;
	}

	Time startTime = AtDay(now.GetYear(), now.GetMonth(), plan.day, plan.start);
	Time endTime = AtDay(now.GetYear(), now.GetMonth(), plan.day, plan.end);

	if (!party.congress.started && now >= startTime) {
		party.congress.started = true;
		PushNotice(party, true);
		size_t count = 0;
		for (Citizen* member : party.members) if (member && member->IsActive()) count++;
		PolitLog(now, "congress_start", "party=" + party.GetId() + " address=" + party.meetingAddress +
			" notified=" + to_string(count) + (party.meetingAddress.empty() ? " note=no_meeting_room" : ""));
	}
	// 跳时间时start和end可能在同一步都过了，先补start再补end。
	if (party.congress.started && !party.congress.ended && now >= endTime) {
		party.congress.ended = true;
		ElectLeader(party, now);
		PushNotice(party, false);
		PolitLog(now, "congress_end", "party=" + party.GetId() + " leader=" + (party.leader ? party.leader->GetName() : string()));
	}
}

void Polit::ElectLeader(Party& party, const Time& now) {
	// 到场者 = 会议室里的党员。
	vector<Citizen*> attendees;
	if (party.meetingRoom) {
		const vector<Citizen*>& occupants = party.meetingRoom->GetOccupants();
		for (Citizen* member : party.members) {
			if (member && member->IsActive() && find(occupants.begin(), occupants.end(), member) != occupants.end()) {
				attendees.push_back(member);
			}
		}
	}
	string names;
	for (Citizen* attendee : attendees) names += (names.empty() ? "" : ",") + attendee->GetName();
	PolitLog(now, "attendance", "party=" + party.GetId() + " present=" + to_string(attendees.size()) + "/" +
		to_string(party.members.size()) + " list=" + names + (party.meetingRoom ? "" : " note=no_meeting_room"));

	if (attendees.size() < 2) {
		PolitLog(now, "leader_vote", "party=" + party.GetId() + " skipped reason=fewer_than_2_attendees keep=" +
			(party.leader ? party.leader->GetName() : string()));
		return;
	}

	// 每个到场者给除自己以外的到场者按权重抽一票：对方在自己熟人表里权重=respect*2，没有关系权重=1。
	vector<int> votes(attendees.size(), 0);
	for (size_t v = 0; v < attendees.size(); v++) {
		const Citizen* voter = attendees[v];
		vector<size_t> candidates;
		vector<double> weights;
		double total = 0.0;
		for (size_t c = 0; c < attendees.size(); c++) {
			if (c == v) continue;
			double weight = 1.0;
			bool related = false;
			auto it = voter->GetAcquaintances().find(attendees[c]->GetName());
			if (it != voter->GetAcquaintances().end()) {
				weight = max(0.0, static_cast<double>(it->second.respect) * 2.0);
				related = true;
			}
			candidates.push_back(c);
			weights.push_back(weight);
			total += weight;
			(void)related;
		}
		size_t pick = candidates.back();
		if (total <= 0.0) {
			pick = candidates[static_cast<size_t>(GetRandom(static_cast<int>(candidates.size())))];
		}
		else {
			double roll = static_cast<double>(GetRandom(1000000)) / 1000000.0 * total;
			double acc = 0.0;
			for (size_t k = 0; k < candidates.size(); k++) {
				acc += weights[k];
				if (roll < acc) {
					pick = candidates[k];
					break;
				}
			}
		}
		votes[pick]++;
		auto relation = voter->GetAcquaintances().find(attendees[pick]->GetName());
		PolitLog(now, "leader_vote", "party=" + party.GetId() + " voter=" + voter->GetName() + " choice=" + attendees[pick]->GetName() +
			" weight=" + PolitNum(relation != voter->GetAcquaintances().end() ? max(0.0, relation->second.respect * 2.0) : 1.0) +
			" related=" + (relation != voter->GetAcquaintances().end() ? "1" : "0") + " total_weight=" + PolitNum(total));
	}

	int best = *max_element(votes.begin(), votes.end());
	vector<size_t> top;
	for (size_t i = 0; i < votes.size(); i++) {
		PolitLog(now, "leader_tally", "party=" + party.GetId() + " candidate=" + attendees[i]->GetName() + " votes=" + to_string(votes[i]));
		if (votes[i] == best) top.push_back(i);
	}
	size_t winner = top.size() == 1 ? top[0] : top[static_cast<size_t>(GetRandom(static_cast<int>(top.size())))];
	party.leader = attendees[winner];
	PolitLog(now, "leader", "party=" + party.GetId() + " leader=" + party.leader->GetName() + " votes=" + to_string(best) +
		" tied=" + (top.size() > 1 ? "1" : "0"));
}
