#include "society_save.h"

#include "society/society.h"
#include "society/organization.h"
#include "society/job.h"
#include "populace/populace.h"
#include "populace/citizen.h"
#include "populace/experience.h"
#include "story/script.h"

#include "common/save.h"
#include "common/save_refs.h"
#include "common/utility.h"

#include <algorithm>
#include <memory>
#include <tuple>
#include <unordered_map>


using namespace std;

namespace {
	int VersionOf(const char* name) {
		for (const SaveSectionVersion& v : GetSaveSectionVersions()) {
			if (string(v.name) == name) return v.current;
		}
		return 1;
	}
}

void SocietySaveCodec::Save(const Society& society, const Map& map, const Populace& populace, SaveArchive& archive) {
	(void)map;
	ByteWriter w;

	unordered_map<const Citizen*, int> citizenIndex;
	const vector<Citizen*>& citizens = populace.GetCitizens();
	for (size_t i = 0; i < citizens.size(); i++) citizenIndex[citizens[i]] = static_cast<int>(i);
	auto citizenRef = [&](const Citizen* c) {
		if (!c) return -1;
		auto it = citizenIndex.find(c);
		return it != citizenIndex.end() ? it->second : -1;
		};

	auto organizationIds = SaveRefs::LiveIds<OrganizationFactory, OrganizationMod>(society.organizationFactory);
	auto jobIds = SaveRefs::LiveIds<JobFactory, JobMod>(society.jobFactory);

	unordered_map<const Organization*, int> organizationIndex;
	w.U32(static_cast<uint32_t>(society.organizations.size()));
	for (size_t i = 0; i < society.organizations.size(); i++) {
		const Organization* o = society.organizations[i];
		organizationIndex[o] = static_cast<int>(i);
		w.Str(SaveRefs::LookupId(organizationIds, static_cast<const OrganizationMod*>(o->mod)));
		SaveRefs::WriteModState(o->mod, w);
		// mod自己的名字(脚本里$$self.name用的就是它)：读档时重新创建的mod会按计数器拿到新名字，
		// 要把脚本变量改回存档时的名字。
		w.Str(o->mod ? string(o->mod->GetName()) : string());
		w.U32(static_cast<uint32_t>(o->components.size()));
		for (const Component* component : o->components) SaveRefs::WriteComponent(component, w);
		w.U32(static_cast<uint32_t>(o->jobs.size()));
		for (const Job* job : o->jobs) {
			w.Str(SaveRefs::LookupId(jobIds, static_cast<const JobMod*>(job->mod)));
			SaveRefs::WriteModState(job->mod, w);
			w.Str(job->mod ? string(job->mod->GetName()) : string());
			SaveRefs::WriteRoom(job->position, w);
			w.I32(citizenRef(job->occupant));
		}
		w.U32(static_cast<uint32_t>(o->shareholders.size()));
		for (const Organization::Shareholder& s : o->shareholders) {
			w.I32(citizenRef(s.citizen));
			w.F32(s.ratio);
		}
		w.U8(o->bankrupt ? 1 : 0);
	}

	// 同一时刻的条目std::set按Organization*地址排序，换成按(时间, 组织下标, 节点名)排序后写出。
	vector<tuple<Time, int, string>> timers;
	for (const auto& [time, organization, node] : society.organizationTimerSet) {
		auto it = organizationIndex.find(organization);
		timers.emplace_back(time, it != organizationIndex.end() ? it->second : -1, node);
	}
	sort(timers.begin(), timers.end());
	w.U32(static_cast<uint32_t>(timers.size()));
	for (const auto& [time, organization, node] : timers) {
		SaveRefs::WriteTime(time, w);
		w.I32(organization);
		w.Str(node);
	}

	// 账本：城市外、政府余额，再是市民和组织的余额（按下标引用，下标升序）。
	w.I64(society.ledger.outside);
	w.I64(society.ledger.government);
	vector<pair<int, int64_t>> citizenBalances;
	for (const auto& [citizen, balance] : society.ledger.citizens) {
		int index = citizenRef(citizen);
		if (index >= 0) citizenBalances.push_back({ index, balance });
	}
	sort(citizenBalances.begin(), citizenBalances.end());
	w.U32(static_cast<uint32_t>(citizenBalances.size()));
	for (const auto& [index, balance] : citizenBalances) { w.I32(index); w.I64(balance); }
	vector<pair<int, int64_t>> organizationBalances;
	for (const auto& [organization, balance] : society.ledger.organizations) {
		auto it = organizationIndex.find(organization);
		if (it != organizationIndex.end()) organizationBalances.push_back({ it->second, balance });
	}
	sort(organizationBalances.begin(), organizationBalances.end());
	w.U32(static_cast<uint32_t>(organizationBalances.size()));
	for (const auto& [index, balance] : organizationBalances) { w.I32(index); w.I64(balance); }

	w.I32(society.lastMonthKey);

	// 累计流水统计（报表用）。
	for (int64_t total : society.ledger.flowTotals) w.I64(total);
	w.I64(society.ledger.initialInjection);

	archive.SetBinary(SAVE_SECTION_SOCIETY, VersionOf(SAVE_SECTION_SOCIETY), w.Take());
}

Society* SocietySaveCodec::Load(const SaveArchive& archive, Map& map, Populace& populace,
	const vector<pair<JobExperience*, int>>& pendingOrganizations, string& error) {
	const SaveSection* section = archive.Find(SAVE_SECTION_SOCIETY);
	if (!section) {
		error = "the save has no section '" SAVE_SECTION_SOCIETY "'";
		return nullptr;
	}

	try {
		ByteReader r(section->data);
		unique_ptr<Society> guard(new Society());
		Society* society = guard.get();
		const vector<Citizen*>& citizens = populace.GetCitizens();
		auto citizenAt = [&](int index) -> Citizen* {
			return (index >= 0 && index < static_cast<int>(citizens.size())) ? citizens[index] : nullptr;
			};

		uint32_t organizationCount = r.U32();
		for (uint32_t i = 0; i < organizationCount && !r.Failed(); i++) {
			string id = r.Str();
			// 空Component列表构造：只创建mod和Script，不按Component重新设计岗位(岗位按存档恢复)。
			Organization* o = new Organization(&society->organizationFactory, &society->jobFactory,
				&society->scriptFactory, id, {});
			society->organizations.push_back(o);
			if (!SaveRefs::ReadModState(o->mod, r)) debugf("Warning: failed to parse saved state of organization mod %s.\n", id.c_str());
			string modName = r.Str();
			if (o->script && !modName.empty()) o->script->SetValue("name", ValueType(modName));
			uint32_t componentCount = r.U32();
			for (uint32_t k = 0; k < componentCount && !r.Failed(); k++) {
				if (Component* component = SaveRefs::ReadComponent(map, r)) o->components.push_back(component);
			}
			uint32_t jobCount = r.U32();
			for (uint32_t k = 0; k < jobCount && !r.Failed(); k++) {
				string jobId = r.Str();
				// Job的构造函数要先拿到工位房间，mod状态和名字写在房间引用前面，先暂存。
				int stateVersion = r.I32();
				string stateText = r.Str();
				string jobName = r.Str();
				Room* position = SaveRefs::ReadRoom(map, r);
				Citizen* occupant = citizenAt(r.I32());
				Job* job = new Job(&society->jobFactory, &society->scriptFactory, jobId, position);
				o->jobs.push_back(job);
				if (job->mod && !stateText.empty()) {
					JsonValue root;
					JsonReader reader;
					if (reader.Parse(stateText, root, false)) {
						JsonStateReader stateReader(root, stateVersion);
						job->mod->LoadState(stateReader);
					}
					else {
						debugf("Warning: failed to parse saved state of job mod %s.\n", jobId.c_str());
					}
				}
				if (job->script && !jobName.empty()) job->script->SetValue("name", ValueType(jobName));
				if (occupant) {
					job->SetOccupant(occupant);
					occupant->SetJob(job);
				}
			}
			uint32_t shareholderCount = r.U32();
			for (uint32_t k = 0; k < shareholderCount && !r.Failed(); k++) {
				Citizen* citizen = citizenAt(r.I32());
				float ratio = r.F32();
				if (citizen) o->shareholders.push_back({ citizen, ratio });
			}
			o->bankrupt = r.U8() != 0;
		}
		auto organizationAt = [&](int index) -> Organization* {
			return (index >= 0 && index < static_cast<int>(society->organizations.size())) ? society->organizations[index] : nullptr;
			};

		uint32_t timerCount = r.U32();
		for (uint32_t i = 0; i < timerCount && !r.Failed(); i++) {
			Time time = SaveRefs::ReadTime(r);
			Organization* organization = organizationAt(r.I32());
			string node = r.Str();
			if (organization) society->organizationTimerSet.insert({ time, organization, node });
		}

		society->ledger.outside = r.I64();
		society->ledger.government = r.I64();
		uint32_t citizenBalanceCount = r.U32();
		for (uint32_t i = 0; i < citizenBalanceCount && !r.Failed(); i++) {
			int index = r.I32();
			int64_t balance = r.I64();
			if (Citizen* citizen = citizenAt(index)) society->ledger.citizens[citizen] = balance;
		}
		uint32_t organizationBalanceCount = r.U32();
		for (uint32_t i = 0; i < organizationBalanceCount && !r.Failed(); i++) {
			int index = r.I32();
			int64_t balance = r.I64();
			if (Organization* organization = organizationAt(index)) society->ledger.organizations[organization] = balance;
		}

		society->lastMonthKey = r.I32();
		for (int64_t& total : society->ledger.flowTotals) total = r.I64();
		society->ledger.initialInjection = r.I64();

		if (r.Failed()) {
			error = "society section is truncated";
			return nullptr;
		}

		for (auto& [experience, index] : pendingOrganizations) {
			if (experience) experience->organization = organizationAt(index);
		}
		return guard.release();
	}
	catch (const exception& e) {
		error = string("society load failed: ") + e.what();
		return nullptr;
	}
	catch (...) {
		error = "society load failed with an unknown error";
		return nullptr;
	}
}
