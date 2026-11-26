#include "polit/polit.h"

#include "common/error.h"

#include "common/random.h"
#include "common/registry.h"
#include "map/map.h"
#include "map/room.h"
#include "populace/populace.h"
#include "populace/citizen.h"
#include "society/society.h"
#include "polit/polit_log.h"

#include <algorithm>
#include <unordered_set>


using namespace std;

namespace {
	constexpr int kMeetingRoomAttempts = 4; // 会议室和别的党重叠时最多重选几次，全部重叠就当没有会议室
}

Polit::Polit() : factory(Registry::Get().GetPartyFactory()) {
	powers.RegisterDefaults();
}

Polit::~Polit() {
	for (Party* party : parties) delete party;
	parties.clear();
}

void Polit::Bind(Map& mapRef, Populace& populaceRef, Society& societyRef) {
	map = &mapRef;
	populace = &populaceRef;
	society = &societyRef;
	society->BindPolicy(&policy);
	// 读档后恢复会议室指针。
	for (Party* party : parties) {
		party->meetingRoom = party->meetingAddress.empty() ? nullptr : map->LocateRoom(party->meetingAddress);
	}
}

Party* Polit::FindParty(const string& id) const {
	for (Party* party : parties) {
		if (party->GetId() == id) return party;
	}
	return nullptr;
}

Party* Polit::GetRulingParty() const {
	for (Party* party : parties) {
		if (party->ruling) return party;
	}
	return nullptr;
}

void Polit::Init(PostHandle& post, const Time& now) {
	CORE_RANDOM_SCOPE("core.polit");
	if (!populace || !map) return;

	vector<string> ids = factory.GetRegisteredIds();
	PolitLog(now, "init", "parties=" + to_string(ids.size()));

	// 候选党员池：在城成年人(党员互斥：一个人最多入一个党)。
	vector<Citizen*> pool;
	for (Citizen* citizen : populace->GetCitizens()) {
		if (citizen && citizen->IsActive() && citizen->GetAge(now) >= policy.adultAge) pool.push_back(citizen);
	}

	unordered_set<const Room*> usedRooms;
	for (const string& id : ids) {
		PartyMod* mod = factory.CreateParty(id);
		if (!mod) continue;
		Party* party = new Party(&factory, mod, id);
		parties.push_back(party);

		// 按人数无放回随机抽(部分Fisher-Yates)，池不够就抽到没人。
		int want = max(0, mod->memberCount);
		int taken = 0;
		while (taken < want && !pool.empty()) {
			size_t pick = static_cast<size_t>(GetRandom(static_cast<int>(pool.size())));
			party->members.push_back(pool[pick]);
			pool[pick] = pool.back();
			pool.pop_back();
			taken++;
		}
		string names;
		for (Citizen* member : party->members) names += (names.empty() ? "" : ",") + member->GetName();
		PolitLog(now, "party", "id=" + id + " name=" + party->GetName() + " wanted=" + to_string(want) +
			" members=" + to_string(party->members.size()) + " list=" + names);

		// 会议室：mod自己挑(post随机房间)，Core校验有效且没被前面的党占用，最多重试4次。
		for (int attempt = 1; attempt <= kMeetingRoomAttempts; attempt++) {
			{
				RandomScope scope(RandomRegistry::GetInstanceStream(mod));
				MOD_CALL(mod, mod->ChooseMeetingRoom(&post));
			}
			string address = mod->meetingAddress;
			if (address.empty()) {
				PolitLog(now, "meeting_room", "party=" + id + " attempt=" + to_string(attempt) + " address= result=none");
				break; // mod不想要会议室，不用重试
			}
			Room* room = map->LocateRoom(address);
			string result;
			if (!room) result = "invalid";
			else if (usedRooms.count(room)) result = "overlap";
			else result = "ok";
			PolitLog(now, "meeting_room", "party=" + id + " attempt=" + to_string(attempt) + " address=" + address +
				" result=" + result);
			if (result == "ok") {
				party->meetingAddress = address;
				party->meetingRoom = room;
				usedRooms.insert(room);
				break;
			}
		}
		if (party->meetingAddress.empty()) PolitLog(now, "meeting_room", "party=" + id + " final=none");
	}
}

void Polit::RemoveGoneMembers(Party& party, const Time& now) {
	vector<Citizen*> kept;
	for (Citizen* member : party.members) {
		if (member && member->IsActive()) {
			kept.push_back(member);
			continue;
		}
		PolitLog(now, "member_removed", "party=" + party.GetId() + " citizen=" + (member ? member->GetName() : string("?")) +
			" reason=" + (member && member->IsDeparted() ? "departed" : "dead"));
		if (party.leader == member) {
			party.leader = nullptr;
			PolitLog(now, "leader_cleared", "party=" + party.GetId());
		}
		party.powers.erase(remove_if(party.powers.begin(), party.powers.end(),
			[member](const pair<string, Citizen*>& entry) { return entry.second == member; }), party.powers.end());
	}
	party.members.swap(kept);
}

void Polit::Step(const Time& now) {
	CORE_RANDOM_SCOPE("core.polit");
	int monthKey = MonthKeyOf(now);
	if (lastMonthKey == 0) {
		lastMonthKey = monthKey; // 开局第一次只记录：第一次选举在第一个月末
	}
	else if (monthKey != lastMonthKey) {
		lastMonthKey = monthKey;
		HoldElection(now);
	}

	for (Party* party : parties) {
		RemoveGoneMembers(*party, now);
		UpdateCongress(*party, now);
	}
}

vector<MeetingNotice> Polit::ConsumeMeetingNotices() {
	vector<MeetingNotice> out;
	out.swap(notices);
	return out;
}

void Polit::AssignPowers(Party& party, const Time& now) {
	party.powers.clear();
	PartyMod* mod = party.GetMod();
	if (!mod) return;

	PartyContext context;
	context.leader = party.leader ? party.leader->GetName() : string();
	for (Citizen* member : party.members) context.members.push_back(member->GetName());
	for (const PowerDef& def : powers.All()) context.powers.push_back({ def.id, def.name, def.policy });

	{
		RandomScope scope(RandomRegistry::GetInstanceStream(mod));
		MOD_CALL(mod, mod->AssignPowers(context));
	}

	// 复制mod的输出并校验：姓名必须是党员，权力id必须已注册，重复的(权力,人)忽略。
	vector<PowerAssignment> assignments = mod->assignments;
	for (const PowerAssignment& assignment : assignments) {
		Citizen* holder = nullptr;
		for (Citizen* member : party.members) {
			if (member->GetName() == assignment.member) {
				holder = member;
				break;
			}
		}
		if (!holder || !powers.Find(assignment.power)) {
			PolitLog(now, "power_rejected", "party=" + party.GetId() + " member=" + assignment.member + " power=" + assignment.power +
				" reason=" + (holder ? "unknown_power" : "not_member"));
			continue;
		}
		bool duplicate = false;
		for (const auto& entry : party.powers) {
			if (entry.first == assignment.power && entry.second == holder) duplicate = true;
		}
		if (duplicate) continue;
		party.powers.push_back({ assignment.power, holder });
		PolitLog(now, "power", "party=" + party.GetId() + " power=" + assignment.power + " holder=" + holder->GetName());
	}
}

vector<string> Polit::PowersOf(const Citizen* citizen) const {
	vector<string> result;
	Party* ruling = GetRulingParty();
	if (!ruling) return result;
	for (const auto& entry : ruling->powers) {
		if (entry.second == citizen) result.push_back(entry.first);
	}
	return result;
}

vector<Citizen*> Polit::HoldersOf(const string& powerId) const {
	vector<Citizen*> result;
	Party* ruling = GetRulingParty();
	if (!ruling) return result;
	for (const auto& entry : ruling->powers) {
		if (entry.first == powerId) result.push_back(entry.second);
	}
	return result;
}
