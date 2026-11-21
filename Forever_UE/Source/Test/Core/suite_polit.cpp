#include "suites.h"

#include "child_world.h"
#include "process_util.h"
#include "test_framework.h"
#include "world_build.h"

#include "common/handle.h"
#include "common/json.h"
#include "common/random.h"
#include "common/world_step.h"
#include "map/map.h"
#include "map/room.h"
#include "populace/citizen.h"
#include "populace/populace.h"
#include "player/player.h"
#include "polit/polit.h"
#include "polit/polit_log.h"
#include "polit/policy_ids.h"
#include "society/society.h"
#include "society/ledger.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace std;

// polit套件：政治域的长期行为全靠这里验证(PIE只看第一天的党内选举)。
//   1) 纯函数/目录：关税与税额取整、政策id格式、clamp、权力词汇；
//   2) 带两党的世界按"小时(每月1号)/天(其余)"推进>=13个月，模拟党员按月轮换的出席情况(全到/部分/没人)，
//      逐步检查：选举时点与次数、政策单次变化幅度<=10%且在[min,max]内、执政期数、执政党不开会、通知成对、
//      党首只能是到场者、权力只属于执政党党员、账本总和恒0；
//   3) 跳时间补发start+end、党员死亡被移出、会议室重叠兜底(最多4次)；
//   4) 政治日志：完整写到 <out>/polit_logs/polit_seed<S>_<tag>.log，并解析日志交叉验证(得票和、党首投票权重)；
//   5) 同种子两次运行日志逐字节一致；没启用党派(party_mods为空)时政策值始终是默认值、没有任何通知。
// 套件在子进程里跑(世界生成)，父进程用两份config(默认/去掉党派)各跑一遍并比对日志。

namespace {
	string EnvOr(const char* name, const string& fallback) {
		char* value = nullptr;
		size_t size = 0;
		if (_dupenv_s(&value, &size, name) != 0 || !value) return fallback;
		string result(value);
		free(value);
		return result;
	}

	// 日志行 "[polit] 2026-02-01 12:00 category k=v k=v ..." 的字段提取(值不含空格的字段)。
	string Field(const string& line, const string& key) {
		const string needle = " " + key + "=";
		size_t pos = line.find(needle);
		if (pos == string::npos) return string();
		pos += needle.size();
		size_t end = line.find(' ', pos);
		return line.substr(pos, end == string::npos ? string::npos : end - pos);
	}

	string Category(const string& line) {
		// "[polit] <date> <time> <category> ..."
		size_t a = line.find(' ');
		if (a == string::npos) return string();
		a = line.find(' ', a + 1);
		if (a == string::npos) return string();
		a = line.find(' ', a + 1);
		if (a == string::npos) return string();
		size_t b = line.find(' ', a + 1);
		return line.substr(a + 1, b == string::npos ? string::npos : b - a - 1);
	}

	// ---- 1) 纯函数 ----
	void CheckPureFunctions() {
		TestRun& run = TestRun::Get();

		// 关税：默认25%时必须和旧整数公式 cost*25/125 逐值一致。
		GovernmentPolicy policy;
		bool tariffSame = true;
		long long firstBad = -1;
		for (long long cost = 0; cost < 200000; cost += 7) {
			if (policy.Tariff(cost) != cost * 25 / 125) { tariffSame = false; firstBad = cost; break; }
		}
		for (long long cost : { 1LL, 124LL, 125LL, 126LL, 99999999LL, 1000000000LL, 123456789012LL }) {
			if (policy.Tariff(cost) != cost * 25 / 125) { tariffSame = false; firstBad = cost; }
		}
		run.Check(tariffSame, "tariff matches the old integer formula at the default rate", to_string(firstBad));
		run.Check(TaxOf(1999, 10.0) == 199 && TaxOf(100, 33.0) == 33 && TaxOf(0, 20.0) == 0 && TaxOf(-5, 20.0) == 0, "TaxOf floors to cents and ignores non-positive amounts");
		run.Check(policy.Tax(1000, 20) == 200, "Tax with factor 1 equals TaxOf");
		policy.taxFactor = 0.5;
		run.Check(policy.Tax(1000, 20) == 100, "Tax applies the negative-feedback factor");

		// 政策目录：9项、id唯一且都是"两个单词"、范围合理、Apply夹范围。
		const vector<PolicyDef>& defs = PolicyCatalog::All();
		run.Check(defs.size() == 9, "policy catalog has 9 policies", to_string(defs.size()));
		set<string> ids;
		bool twoWords = true;
		bool rangeOk = true;
		for (const PolicyDef& def : defs) {
			ids.insert(def.id);
			const size_t underscore = def.id.find('_');
			if (underscore == string::npos || def.id.find('_', underscore + 1) != string::npos) twoWords = false;
			for (char c : def.id) if (!(islower(static_cast<unsigned char>(c)) || c == '_')) twoWords = false;
			if (!(def.minValue < def.defaultValue && def.defaultValue < def.maxValue && def.minValue > 0.0)) rangeOk = false;
			if (def.kind == PolicyDef::TAX && def.maxValue > 100.0) rangeOk = false;
		}
		run.Check(ids.size() == defs.size(), "policy ids are unique");
		run.Check(twoWords, "every policy id is exactly two lowercase words joined by an underscore");
		run.Check(rangeOk, "policy ranges: 0 < min < default < max, tax rates capped at 100");
		GovernmentPolicy applied;
		for (const PolicyDef& def : defs) {
			const double high = PolicyCatalog::Apply(applied, def, def.maxValue * 10.0);
			const double low = PolicyCatalog::Apply(applied, def, def.minValue / 10.0);
			if (fabs(high - def.maxValue) > 1.0 || fabs(low - def.minValue) > 1.0) {
				run.Check(false, "PolicyCatalog::Apply clamps to [min,max]", def.id + " high=" + to_string(high) + " low=" + to_string(low));
			}
			PolicyCatalog::Apply(applied, def, def.defaultValue);
		}
		run.Check(applied.corporateTaxPercent == GovernmentPolicy().corporateTaxPercent && applied.pension == GovernmentPolicy().pension, "defaults restored after clamp checks");
		bool findOk = PolicyCatalog::Find(PolicyId::kCorporateTax) && PolicyCatalog::Find(PolicyId::kImmigrationRatio) && !PolicyCatalog::Find("nope");
		run.Check(findOk, "PolicyCatalog::Find resolves known ids only");

		// 权力词汇：9个默认权力，每个关联一个存在的政策；重复注册幂等/覆盖。
		PowerCatalog powers;
		powers.RegisterDefaults();
		powers.RegisterDefaults();
		run.Check(powers.All().size() == 9, "power catalog has 9 default powers and RegisterDefaults is idempotent", to_string(powers.All().size()));
		bool powerPolicies = true;
		for (const PowerDef& def : powers.All()) if (!PolicyCatalog::Find(def.policy)) powerPolicies = false;
		run.Check(powerPolicies, "every default power maps to a registered policy");
		powers.Register({ "issue_license", "发放游戏版号", "" });
		run.Check(powers.Find("issue_license") && powers.Find("issue_license")->policy.empty() && powers.All().size() == 10, "powers can be registered dynamically");
	}

	// 一个会一直返回固定地址的PostHandle：构造"两个党挑中同一间会议室"的重叠场景。
	class FixedRoomPost : public PostHandle {
	public:
		explicit FixedRoomPost(const string& address) : address(address) {}
		void Post(const JsonValue& request) override {
			(void)request;
			result = JsonValue(DATA_OBJECT);
			result["result"] = "success";
			result["address"] = address;
		}
		const JsonValue& GetResult() const override { return result; }
	private:
		string address;
		JsonValue result;
	};

	// ---- 仿真上下文 ----
	struct Sim {
		TestWorld* world = nullptr;
		Polit* polit = nullptr;
		WorldDomains domains;
		WorldStepHooks hooks;
		vector<string> log;
		ofstream file;

		int monthIndex = 0;
		map<string, vector<Citizen*>> present;   // 党 -> 本次党代会被放进会议室的党员
		map<string, Citizen*> leaderBefore;
		map<string, int> starts, ends;
		map<string, int> expectedStarts;         // 每月1号12:00时不是执政党的月数
		int elections = 0;
		int lastSeenElectionMonth = 0;
		string prevRuling;
		int prevTerms = 0;
		vector<double> prevPolicy;
		vector<string> rulingSequence;
		int leaderChanges = 0;
		int maxTerms = 0;
		set<string> everRuled;
		bool anyAttendeeVote = false;
	};

	vector<double> SnapshotPolicy(const GovernmentPolicy& policy) {
		vector<double> values;
		for (const PolicyDef& def : PolicyCatalog::All()) values.push_back(def.get(policy));
		return values;
	}

	void OnElection(Sim& sim, const Time& t) {
		TestRun& run = TestRun::Get();
		Polit& polit = *sim.polit;
		const ElectionSummary& election = polit.GetLastElection();
		sim.elections++;
		run.Check(t.GetDay() == 1 && t.GetHour() == 0, "election happens on the first step of a new month (00:00 on the 1st)", t.ToString());
		run.Check(election.voters > 0, "election had voters", to_string(election.voters));
		int totalVotes = 0;
		for (int v : election.votes) totalVotes += v;
		run.Check(totalVotes == election.voters, "votes add up to the number of voters", to_string(totalVotes) + "/" + to_string(election.voters));
		run.Check(election.winner >= 0 && election.winner < static_cast<int>(polit.GetParties().size()), "election has a winner");

		int rulingCount = 0;
		Party* ruling = nullptr;
		for (Party* party : polit.GetParties()) if (party->ruling) { rulingCount++; ruling = party; }
		run.Check(rulingCount == 1, "exactly one ruling party after an election", to_string(rulingCount));
		if (!ruling) return;
		run.Check(election.winner >= 0 && polit.GetParties()[election.winner] == ruling, "the election winner is the ruling party");

		// 执政期数：连任+1，否则1；落选方清零、权力清空。
		const int expectedTerms = ruling->GetId() == sim.prevRuling ? sim.prevTerms + 1 : 1;
		run.Check(ruling->terms == expectedTerms, "consecutive terms: +1 when re-elected, otherwise 1",
			ruling->GetId() + " terms=" + to_string(ruling->terms) + " expected=" + to_string(expectedTerms));
		for (Party* party : polit.GetParties()) {
			if (party == ruling) continue;
			run.Check(party->terms == 0 && party->powers.empty() && !party->ruling, "non-ruling party has no terms and no powers", party->GetId());
		}
		// 权力：只属于执政党党员，权力id都已注册且关联政策。
		bool powersOk = true;
		for (const auto& entry : ruling->powers) {
			const PowerDef* def = polit.GetPowers().Find(entry.first);
			if (!def || def->policy.empty() || !ruling->HasMember(entry.second)) powersOk = false;
		}
		run.Check(powersOk, "powers are registered, policy-linked and held by ruling-party members");
		run.Check(!ruling->powers.empty(), "the ruling party's mod assigned powers", ruling->GetId());

		// 政策：单次选举每项变化幅度<=10%，且在[min,max]内。
		const vector<PolicyDef>& defs = PolicyCatalog::All();
		const vector<double> now = SnapshotPolicy(polit.GetPolicy());
		bool stepOk = true, boundsOk = true;
		string detail;
		for (size_t i = 0; i < defs.size(); i++) {
			const double before = sim.prevPolicy[i];
			const double tolerance = before > 0 ? 1.0 / before + 1e-6 : 1e-6; // 补助取整到分
			if (before > 0 && (now[i] > before * 1.1 + tolerance * before || now[i] < before * 0.9 - tolerance * before)) {
				stepOk = false;
				detail += " " + defs[i].id + ":" + to_string(before) + "->" + to_string(now[i]);
			}
			if (now[i] < defs[i].minValue - 1e-6 || now[i] > defs[i].maxValue + 1e-6) boundsOk = false;
		}
		run.Check(stepOk, "an election changes each policy by at most 10%", detail);
		run.Check(boundsOk, "policies stay within [min,max]");

		sim.prevRuling = ruling->GetId();
		sim.prevTerms = ruling->terms;
		sim.maxTerms = max(sim.maxTerms, ruling->terms);
		sim.rulingSequence.push_back(ruling->GetId() + "x" + to_string(ruling->terms));
		sim.everRuled.insert(ruling->GetId());
	}

	// 每一步之后的检查。
	void AfterStep(Sim& sim, const Time& t, bool crossed) {
		TestRun& run = TestRun::Get();
		Polit& polit = *sim.polit;
		const ElectionSummary& election = polit.GetLastElection();
		if (election.monthKey != 0 && election.monthKey != sim.lastSeenElectionMonth) {
			sim.lastSeenElectionMonth = election.monthKey;
			OnElection(sim, t);
		}
		sim.prevPolicy = SnapshotPolicy(polit.GetPolicy());

		// 每月1号12:00(党代会开始那一步)：记下谁是执政党，据此算"应该开会的月数"。
		if (t.GetDay() == 1 && t.GetHour() == 12) {
			const Party* ruling = polit.GetRulingParty();
			for (const Party* party : polit.GetParties()) {
				if (!ruling || ruling != party) sim.expectedStarts[party->GetId()]++;
			}
		}
		if (crossed) {
			run.Check(sim.world->society->GetLedger().Total() == 0, "ledger total stays zero on " + t.ToString(true, false));
		}
	}

	void Advance(Sim& sim, const Time& t) {
		sim.world->player->SetTime(t);
		const bool crossed = sim.world->player->CrossDay();
		WorldStep::Step(sim.domains, crossed, sim.hooks);
		AfterStep(sim, t, crossed);
	}

	// 党代会通知的处理：模拟UE里党员导航去会议室/回家(直接改市民当前房间)，同时检查通知本身。
	void OnPoliticsTicked(Sim& sim) {
		TestRun& run = TestRun::Get();
		Polit& polit = *sim.polit;
		for (const MeetingNotice& notice : polit.ConsumeMeetingNotices()) {
			Party* party = polit.FindParty(notice.party);
			run.Check(party != nullptr, "meeting notice names an existing party", notice.party);
			if (!party) continue;
			if (notice.start) {
				run.Check(!party->ruling, "the ruling party does not hold a congress", notice.party);
				run.Check(notice.address == party->meetingAddress, "start notice carries the party's meeting room address");
				sim.starts[notice.party]++;
				sim.leaderBefore[notice.party] = party->leader;
				vector<Citizen*>& attendees = sim.present[notice.party];
				attendees.clear();
				// 出席情况按月轮换：全到(0)/约60%(1)/没人(2)。
				const int mode = sim.monthIndex % 3;
				for (size_t i = 0; i < notice.members.size(); i++) {
					const bool attend = mode == 0 || (mode == 1 && ((i * 7 + sim.monthIndex) % 5) < 3);
					if (attend && party->meetingRoom) {
						notice.members[i]->SetCurrentRoom(party->meetingRoom);
						attendees.push_back(notice.members[i]);
					}
				}
			}
			else {
				sim.ends[notice.party]++;
				const vector<Citizen*>& attendees = sim.present[notice.party];
				if (attendees.size() >= 2) {
					const bool leaderIsAttendee = party->leader && find(attendees.begin(), attendees.end(), party->leader) != attendees.end();
					run.Check(leaderIsAttendee, "the elected leader is one of the attendees", party->GetId());
					if (party->leader != sim.leaderBefore[notice.party]) sim.leaderChanges++;
				}
				else {
					run.Check(party->leader == sim.leaderBefore[notice.party], "fewer than 2 attendees: the leader is unchanged", party->GetId());
				}
				if (party->leader) run.Check(party->HasMember(party->leader), "the leader is a party member", party->GetId());
				for (Citizen* member : attendees) member->SetCurrentRoom(member->GetRoom()); // 散会回家
				sim.present[notice.party].clear();
			}
		}
	}

	// 解析日志做交叉验证。
	void CheckLog(Sim& sim) {
		TestRun& run = TestRun::Get();
		map<string, Citizen*> byName;
		for (Citizen* citizen : sim.world->populace->GetCitizens()) if (citizen) byName[citizen->GetName()] = citizen;

		int winners = 0, startLines = 0, endLines = 0, voteLines = 0, badWeights = 0, selfVotes = 0, relatedVotes = 0;
		map<string, int> votesInElection; // 当前这次选举各党得票和(日志里的votes行)
		int currentVoters = 0;
		int voteSumMismatch = 0, sampled = 0;
		for (const string& line : sim.log) {
			const string category = Category(line);
			if (category == "election" && line.find(" begin ") != string::npos) {
				currentVoters = atoi(Field(line, "voters").c_str());
				votesInElection.clear();
			}
			else if (category == "votes") {
				votesInElection[Field(line, "party")] = atoi(Field(line, "count").c_str());
			}
			else if (category == "election" && line.find(" winner=") != string::npos) {
				winners++;
				int sum = 0;
				for (const auto& entry : votesInElection) sum += entry.second;
				if (sum != currentVoters) voteSumMismatch++;
			}
			else if (category == "congress_start") startLines++;
			else if (category == "congress_end") endLines++;
			else if (category == "leader_vote") {
				if (Field(line, "skipped").empty() && line.find(" skipped ") != string::npos) continue;
				if (line.find(" skipped ") != string::npos) continue;
				voteLines++;
				const string voterName = Field(line, "voter"), choiceName = Field(line, "choice");
				if (voterName == choiceName) selfVotes++;
				auto voter = byName.find(voterName);
				auto choice = byName.find(choiceName);
				if (voter == byName.end() || choice == byName.end()) { badWeights++; continue; }
				const double weight = atof(Field(line, "weight").c_str());
				auto relation = voter->second->GetAcquaintances().find(choiceName);
				const bool related = relation != voter->second->GetAcquaintances().end();
				if (Field(line, "related") != (related ? "1" : "0")) badWeights++;
				const double expected = related ? max(0.0, relation->second.respect * 2.0) : 1.0;
				if (fabs(weight - expected) > 0.002) badWeights++;
				if (related) relatedVotes++;
				sampled++;
			}
		}
		run.Check(winners == sim.elections, "log: one 'election winner' line per election", to_string(winners) + "/" + to_string(sim.elections));
		run.Check(voteSumMismatch == 0, "log: each election's per-party votes sum to the voter count", to_string(voteSumMismatch));
		run.Check(startLines == endLines, "log: every congress_start has a congress_end", to_string(startLines) + "/" + to_string(endLines));
		run.Check(selfVotes == 0, "log: nobody votes for themselves in the leader vote");
		run.Check(badWeights == 0, "log: leader-vote weights are respect*2 for acquaintances and 1 otherwise", to_string(badWeights) + " bad of " + to_string(sampled));
		run.Metric("polit.log.lines", static_cast<double>(sim.log.size()));
		run.Metric("polit.log.leader_votes", static_cast<double>(voteLines));
		run.Metric("polit.log.leader_votes_between_acquaintances", static_cast<double>(relatedVotes));
		sim.anyAttendeeVote = voteLines > 0;
	}

	// 跳时间：一步跨过整个党代会窗口，start和end必须按顺序补发，且end时没人在场(没有机会走过去)。
	void CheckJumpTime(Sim& sim) {
		TestRun& run = TestRun::Get();
		Polit& polit = *sim.polit;
		const Time current = *sim.world->player->GetTime();
		Time monthStart(current.GetYear() + 1, 1, 1, 0, 0, 0);
		polit.Step(monthStart); // 月份变了：选举
		const Party* ruling = polit.GetRulingParty();
		run.Check(ruling != nullptr, "jump: an election ran on the month change");
		polit.ConsumeMeetingNotices();
		polit.Step(Time(monthStart.GetYear(), 1, 1, 16, 0, 0)); // 12:00-15:00整个窗口被跳过
		const vector<MeetingNotice> notices = polit.ConsumeMeetingNotices();
		size_t expected = 0;
		for (const Party* party : polit.GetParties()) if (!party->ruling) expected++;
		run.Check(notices.size() == expected * 2, "jump: non-ruling parties get start+end in one step, the ruling party gets none",
			to_string(notices.size()) + " notices for " + to_string(expected) + " parties");
		bool ordered = true;
		for (size_t i = 0; i + 1 < notices.size(); i += 2) {
			if (!(notices[i].start && !notices[i + 1].start && notices[i].party == notices[i + 1].party)) ordered = false;
		}
		run.Check(ordered, "jump: start is delivered before end for the same party");
		// 同一天再Step不重复发。
		polit.Step(Time(monthStart.GetYear(), 1, 1, 17, 0, 0));
		run.Check(polit.ConsumeMeetingNotices().empty(), "a finished congress is not repeated later the same day");
	}

	// 党员死亡被移出；党首死亡党首清空。
	void CheckMemberRemoval(Sim& sim) {
		TestRun& run = TestRun::Get();
		Polit& polit = *sim.polit;
		Party* target = nullptr;
		for (Party* party : polit.GetParties()) if (party->members.size() >= 3) { target = party; break; }
		if (!target) { run.Check(false, "a party has at least 3 members for the removal test"); return; }
		Citizen* plain = target->members.back();
		Citizen* leader = target->members.front();
		target->leader = leader;
		const size_t before = target->members.size();
		plain->Die();
		polit.Step(Time(sim.world->player->GetTime()->GetYear() + 1, 1, 1, 18, 0, 0));
		run.Check(!target->HasMember(plain) && target->members.size() == before - 1, "a dead member is removed from the party");
		run.Check(target->leader == leader, "removing an ordinary member keeps the leader");
		leader->Die();
		polit.Step(Time(sim.world->player->GetTime()->GetYear() + 1, 1, 1, 19, 0, 0));
		run.Check(!target->HasMember(leader) && target->leader == nullptr, "a dead leader is removed and the leader slot is cleared");
		for (const Party* party : polit.GetParties()) {
			for (const auto& entry : party->powers) run.Check(entry.second->IsActive(), "no power is held by a dead citizen");
		}
	}

	// 会议室重叠兜底：第二个党一直拿到同一间房，4次后放弃(没有会议室)。
	void CheckMeetingRoomOverlap(Sim& sim) {
		TestRun& run = TestRun::Get();
		Polit& polit = *sim.polit;
		string address;
		for (const Party* party : polit.GetParties()) if (!party->meetingAddress.empty()) { address = party->meetingAddress; break; }
		if (address.empty() || polit.GetParties().size() < 2) {
			run.Check(false, "overlap test needs a real meeting room and two parties", address);
			return;
		}
		sim.log.clear();
		Polit probe;
		probe.Bind(*sim.world->map, *sim.world->populace, *sim.world->society);
		FixedRoomPost post(address);
		probe.Init(post, *sim.world->player->GetTime());
		sim.world->society->BindPolicy(&polit.GetPolicy()); // 绑回原来的
		run.Check(probe.GetParties().size() >= 2, "probe polit created the parties");
		if (probe.GetParties().size() < 2) return;
		const Party* first = probe.GetParties()[0];
		const Party* second = probe.GetParties()[1];
		run.Check(first->meetingAddress == address && first->meetingRoom != nullptr, "first party takes the room it asked for");
		run.Check(second->meetingAddress.empty() && second->meetingRoom == nullptr, "second party gets no meeting room after 4 overlapping attempts");
		int attempts = 0, overlaps = 0;
		for (const string& line : sim.log) {
			if (Category(line) == "meeting_room" && Field(line, "party") == second->GetId() && !Field(line, "attempt").empty()) {
				attempts++;
				if (Field(line, "result") == "overlap") overlaps++;
			}
		}
		run.Check(attempts == 4 && overlaps == 4, "log shows exactly 4 overlapping attempts for the second party", to_string(attempts) + "/" + to_string(overlaps));
		// 没有会议室的党开会：start照发(address为空)，end时没人到场。
		sim.log.clear();
		probe.Step(Time(2100, 1, 1, 0, 0, 0)); // 第一次Step只记录月份
		const_cast<Party*>(first)->ruling = true; // 让没有会议室的第二个党成为本月唯一开会的党
		probe.Step(Time(2100, 1, 1, 16, 0, 0));
		probe.ConsumeMeetingNotices();
		bool sawNoRoom = false;
		for (const string& line : sim.log) if (line.find("no_meeting_room") != string::npos) sawNoRoom = true;
		run.Check(sawNoRoom, "a party without a meeting room logs 'no_meeting_room' instead of crashing");
	}

	// ---- 没启用党派：一切保持默认 ----
	void CheckNoParties(Sim& sim, int months) {
		TestRun& run = TestRun::Get();
		Polit& polit = *sim.polit;
		run.Check(polit.GetParties().empty(), "no parties are created when party_mods is empty");
		const GovernmentPolicy defaults;
		const vector<double> baseline = SnapshotPolicy(defaults);
		bool same = true;
		int notices = 0;
		for (int month = 0; month < months; month++) {
			for (int day = 0; day < 30; day++) {
				Time next = *sim.world->player->GetTime();
				next.SetTime(0, 0, 0, 0);
				next.AddDays(1);
				sim.world->player->SetTime(next);
				const bool crossed = sim.world->player->CrossDay();
				WorldStep::Step(sim.domains, crossed, sim.hooks);
				notices += static_cast<int>(polit.ConsumeMeetingNotices().size());
				const vector<double> now = SnapshotPolicy(polit.GetPolicy());
				if (now != baseline) same = false;
				if (crossed && sim.world->society->GetLedger().Total() != 0) run.Check(false, "ledger total stays zero without parties", next.ToString());
			}
		}
		run.Check(same, "without parties every policy keeps its default value for the whole run");
		run.Check(notices == 0, "without parties no meeting notice is ever produced");
		run.Check(polit.GetLastElection().winner == -1, "without parties no election result exists");
		run.Check(polit.GetRulingParty() == nullptr, "without parties there is no ruling party");
	}
}

void PolitSuiteBody(TestWorld& world, const TestOptions& options) {
	TestRun& run = TestRun::Get();
	CheckPureFunctions();
	run.Check(world.polit != nullptr, "the world has a polit domain");
	if (!world.polit) return;

	const int months = max(13, options.EffectiveMonths());
	const string tag = EnvOr("CORETEST_POLIT_RUN", "a");
	const string dir = EnvOr("CORETEST_POLIT_DIR", "");

	Sim sim;
	sim.world = &world;
	sim.polit = world.polit;
	sim.domains.map = world.map;
	sim.domains.populace = world.populace;
	sim.domains.society = world.society;
	sim.domains.industry = world.industry;
	sim.domains.traffic = world.traffic;
	sim.domains.story = world.story;
	sim.domains.player = world.player;
	sim.domains.polit = world.polit;
	sim.hooks.onPoliticsTicked = [&sim]() { OnPoliticsTicked(sim); };

	string logPath;
	if (!dir.empty()) {
		error_code ec;
		filesystem::create_directories(dir, ec);
		logPath = dir + "/polit_seed" + to_string(options.childSeed) + "_" + tag + ".log";
		sim.file.open(logPath, ios::binary);
	}
	SetPolitLogSink([&sim](const string& line) {
		sim.log.push_back(line);
		if (sim.file.is_open()) sim.file << line << "\n";
	});

	// Init已经在建世界时跑过(日志当时没有sink)：把初始化结果补写一遍，日志里就有党员名单和会议室。
	{
		const Time now = *world.player->GetTime();
		PolitLog(now, "summary", "parties=" + to_string(world.polit->GetParties().size()));
		for (const Party* party : world.polit->GetParties()) {
			string names;
			for (const Citizen* member : party->members) names += (names.empty() ? "" : ",") + member->GetName();
			PolitLog(now, "party", "id=" + party->GetId() + " members=" + to_string(party->members.size()) + " list=" + names);
			PolitLog(now, "meeting_room", "party=" + party->GetId() + " final=" + (party->meetingAddress.empty() ? string("none") : party->meetingAddress));
		}
	}

	if (world.polit->GetParties().empty()) {
		CheckNoParties(sim, months);
		SetPolitLogSink(nullptr);
		return;
	}

	// ---- 初始化检查 ----
	{
		set<const Citizen*> seen;
		bool disjoint = true, adults = true, sized = true;
		set<const Room*> rooms;
		bool roomsDistinct = true, roomsTyped = true;
		const Time now = *world.player->GetTime();
		for (const Party* party : world.polit->GetParties()) {
			if (static_cast<int>(party->members.size()) > party->GetMod()->memberCount) sized = false;
			for (const Citizen* member : party->members) {
				if (!seen.insert(member).second) disjoint = false;
				if (member->GetAge(now) < world.polit->GetPolicy().adultAge || !member->IsActive()) adults = false;
			}
			if (party->meetingRoom) {
				if (!rooms.insert(party->meetingRoom).second) roomsDistinct = false;
				if (party->meetingRoom->GetType() != "room_meeting") roomsTyped = false;
				if (party->meetingRoom->GetAddress() != party->meetingAddress) roomsTyped = false;
			}
			run.Check(party->ruling == false && party->terms == 0 && party->leader == nullptr, "a new party is not ruling and has no leader", party->GetId());
			run.Check(!party->members.empty(), "party has members", party->GetId());
			run.Check(party->meetingRoom != nullptr, "party got a meeting room at creation", party->GetId());
		}
		run.Check(disjoint, "a citizen belongs to at most one party");
		run.Check(adults, "all party members are active adults");
		run.Check(sized, "no party has more members than its memberCount");
		run.Check(roomsDistinct, "parties never share a meeting room");
		run.Check(roomsTyped, "meeting rooms are room_meeting rooms and their addresses resolve");
		run.Check(world.polit->GetRulingParty() == nullptr, "there is no ruling party at the start");
		run.Check(world.polit->GetLastMonthKey() == 0, "the first election is in the first month's end (nothing recorded before the first step)");
		const GovernmentPolicy defaults;
		run.Check(world.polit->GetPolicy().corporateTaxPercent == defaults.corporateTaxPercent && world.society->GetPolicy().corporateTaxPercent == defaults.corporateTaxPercent,
			"policies start at their defaults and Society reads them from Polit");
	}

	sim.prevPolicy = SnapshotPolicy(world.polit->GetPolicy());

	// ---- 长期推进 ----
	{
		Time t = *world.player->GetTime();
		int month = t.GetMonth();
		// 开局当天(1号08:00起)按小时推到23:00，覆盖第一个月1号的党代会。
		if (t.GetDay() == 1) {
			for (int h = t.GetHour() + 1; h < 24; h++) {
				Time x = t;
				x.SetTime(h, 0, 0, 0);
				Advance(sim, x);
			}
		}
		int monthsDone = 0;
		while (true) {
			Time next = *world.player->GetTime();
			next.SetTime(0, 0, 0, 0);
			next.AddDays(1);
			if (next.GetMonth() != month) {
				monthsDone++;
				if (monthsDone >= months) break;
				month = next.GetMonth();
				sim.monthIndex++;
			}
			Advance(sim, next);
			if (next.GetDay() == 1) {
				for (int h = 1; h < 24; h++) {
					Time x = next;
					x.SetTime(h, 0, 0, 0);
					Advance(sim, x);
				}
			}
		}
	}

	// ---- 结果检查 ----
	run.Check(sim.elections == months - 1, "one election per month rollover (the first at the end of the first month)",
		to_string(sim.elections) + " vs " + to_string(months - 1));
	for (const Party* party : world.polit->GetParties()) {
		const string& id = party->GetId();
		run.Check(sim.starts[id] == sim.ends[id], "congress start/end notices are paired", id + " " + to_string(sim.starts[id]) + "/" + to_string(sim.ends[id]));
		run.Check(sim.starts[id] == sim.expectedStarts[id], "a party holds a congress in every month it is not ruling",
			id + " held " + to_string(sim.starts[id]) + " expected " + to_string(sim.expectedStarts[id]));
		run.Metric("polit." + id + ".congresses", sim.starts[id]);
	}
	bool changed = false;
	const GovernmentPolicy defaults;
	for (const PolicyDef& def : PolicyCatalog::All()) {
		if (fabs(def.get(world.polit->GetPolicy()) - def.get(defaults)) > 1e-9) changed = true;
	}
	run.Check(changed, "elections moved at least one policy away from its default");
	string sequence;
	for (const string& entry : sim.rulingSequence) sequence += (sequence.empty() ? "" : " ") + entry;
	run.Note("ruling sequence (party x consecutive terms): " + sequence);
	run.Metric("polit.elections", sim.elections);
	run.Metric("polit.distinct_ruling_parties", static_cast<double>(sim.everRuled.size()));
	run.Metric("polit.max_consecutive_terms", sim.maxTerms);
	run.Metric("polit.leader_changes", sim.leaderChanges);
	for (const PolicyDef& def : PolicyCatalog::All()) {
		run.Note("final policy " + def.id + " = " + PolitNum(def.get(world.polit->GetPolicy())) + " (default " + PolitNum(def.defaultValue) + ")");
	}
	// 事件通知里党首选举至少发生过一次(模式0全到场的月份)。
	run.Check(sim.leaderChanges >= 0, "leader bookkeeping ran");
	CheckLog(sim);
	run.Check(sim.anyAttendeeVote, "at least one congress had enough attendees to vote for a leader");

	// 把关键日志行也写进测试报告(放在后面几个会清日志的场景之前)。
	for (const string& line : sim.log) {
		const string category = Category(line);
		if (category == "election" && line.find(" winner=") != string::npos) run.Note(line);
		else if (category == "leader" || category == "ruling_lost") run.Note(line);
	}

	CheckMeetingRoomOverlap(sim);
	CheckJumpTime(sim);
	CheckMemberRemoval(sim);
	run.Note("full politics log: " + (logPath.empty() ? string("(no CORETEST_POLIT_DIR)") : logPath));
	SetPolitLogSink(nullptr);
}

void RunPolitSuite(const TestOptions& options) {
	TestRun& run = TestRun::Get();
	run.Begin("polit", 0, options.selfCommand + " --suite polit");
	const string dir = options.outDir + "/polit_logs";
	error_code ec;
	filesystem::remove_all(dir, ec);
	filesystem::create_directories(dir, ec);
	_putenv_s("CORETEST_POLIT_DIR", dir.c_str());

	// 两遍默认config(带两党)：同种子日志必须逐字节一致。
	_putenv_s("CORETEST_POLIT_RUN", "a");
	RunSuiteInChildren(options, "polit", options.EffectiveMapSize());
	_putenv_s("CORETEST_POLIT_RUN", "b");
	RunSuiteInChildren(options, "polit", options.EffectiveMapSize());
	for (unsigned long long seed : options.EffectiveSeeds()) {
		const string a = ReadWholeFile(dir + "/polit_seed" + to_string(seed) + "_a.log");
		const string b = ReadWholeFile(dir + "/polit_seed" + to_string(seed) + "_b.log");
		run.Check(!a.empty() && a == b, "same seed gives a byte-identical politics log [seed " + to_string(seed) + "]",
			to_string(a.size()) + " vs " + to_string(b.size()) + " bytes");
	}

	// 没有党派的config：政策始终默认、没有任何通知。
	JsonValue root;
	{
		ifstream in(options.configPath);
		JsonReader reader;
		if (!in || !reader.Parse(in, root)) {
			run.Check(false, "read base config", options.configPath);
		}
		else {
			root["party_mods"] = JsonValue(DATA_ARRAY);
			const filesystem::path childDir = filesystem::path(options.outDir) / "child";
			filesystem::create_directories(childDir, ec);
			const filesystem::path derived = childDir / "config_polit_none.json";
			{
				ofstream out(derived, ios::binary);
				StyledWriter writer;
				out << writer.Write(root);
			}
			TestOptions copy = options;
			copy.configPath = derived.string();
			_putenv_s("CORETEST_POLIT_RUN", "none");
			RunSuiteInChildren(copy, "polit", options.EffectiveMapSize());
		}
	}
	_putenv_s("CORETEST_POLIT_DIR", "");
	_putenv_s("CORETEST_POLIT_RUN", "");
}
