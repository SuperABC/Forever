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

	archive.SetBinary(SAVE_SECTION_SOCIETY, VersionOf(SAVE_SECTION_SOCIETY), w.Take());
}

Society* SocietySaveCodec::Load(const SaveArchive& archive, Map& map, Populace& populace,
	const vector<pair<JobExperience*, int>>& pendingOrganizations, string& error) {
	const SaveSection* section = archive.Find(SAVE_SECTION_SOCIETY);
	if (!section) return nullptr;

	try {
		ByteReader r(section->data);
		Society* society = new Society();
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

		if (r.Failed()) {
			error = "society section is truncated";
			return nullptr;
		}

		for (auto& [experience, index] : pendingOrganizations) {
			if (experience) experience->organization = organizationAt(index);
		}
		return society;
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
