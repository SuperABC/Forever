#include "populace_save.h"

#include "populace/populace.h"
#include "populace/citizen.h"
#include "populace/experience.h"
#include "populace/scheduler.h"
#include "populace/school.h"
#include "populace/name.h"
#include "society/society.h"

#include "common/save.h"
#include "common/save_refs.h"
#include "common/utility.h"

#include <algorithm>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>


using namespace std;

namespace {
	enum ExperienceKind : uint8_t {
		EXPERIENCE_PLAIN = 0,
		EXPERIENCE_KINSHIP = 1,
		EXPERIENCE_EMOTION = 2,
		EXPERIENCE_EDUCATION = 3,
		EXPERIENCE_JOB = 4,
	};

	constexpr int kPersonalityCount = PERSONALITY_PERCEPTION + 1;
	constexpr int kRelationCount = RELATION_RELIABILITY + 1;

	vector<string> SortedNames(const unordered_map<string, Relation>& m) {
		vector<string> keys;
		keys.reserve(m.size());
		for (auto& [k, _] : m) keys.push_back(k);
		sort(keys.begin(), keys.end());
		return keys;
	}

	template <typename T>
	vector<T> SortedSet(const unordered_set<T>& s) {
		vector<T> v(s.begin(), s.end());
		sort(v.begin(), v.end());
		return v;
	}

	void WriteStrings(const vector<string>& v, ByteWriter& w) {
		w.U32(static_cast<uint32_t>(v.size()));
		for (const string& s : v) w.Str(s);
	}
	vector<string> ReadStrings(ByteReader& r) {
		vector<string> v(r.U32());
		for (string& s : v) s = r.Str();
		return v;
	}

	int VersionOf(const char* name) {
		for (const SaveSectionVersion& v : GetSaveSectionVersions()) {
			if (string(v.name) == name) return v.current;
		}
		return 1;
	}
}

void PopulaceSaveCodec::Save(const Populace& populace, const Map& map, const Society* society, SaveArchive& archive) {
	ByteWriter w;

	unordered_map<const Citizen*, int> citizenIndex;
	for (size_t i = 0; i < populace.citizens.size(); i++) citizenIndex[populace.citizens[i]] = static_cast<int>(i);
	auto citizenRef = [&](const Citizen* c) {
		if (!c) return -1;
		auto it = citizenIndex.find(c);
		return it != citizenIndex.end() ? it->second : -1;
		};
	unordered_map<const SchoolClass*, int> classIndex;
	for (size_t i = 0; i < populace.schoolClasses.size(); i++) classIndex[populace.schoolClasses[i]] = static_cast<int>(i);
	unordered_map<const Organization*, int> organizationIndex;
	if (society) {
		const vector<Organization*>& organizations = society->GetOrganizations();
		for (size_t i = 0; i < organizations.size(); i++) organizationIndex[organizations[i]] = static_cast<int>(i);
	}

	w.I32(populace.currentYear);

	// 取名器：注册id + 占位名(已经用掉/剧情保留的名字，之后新生成的市民不会重名)。
	const Name* name = populace.name;
	w.U8(name ? 1 : 0);
	if (name) {
		auto nameIds = SaveRefs::LiveIds<NameFactory, NameMod>(populace.nameFactory);
		w.Str(SaveRefs::LookupId(nameIds, static_cast<const NameMod*>(name->mod)));
		SaveRefs::WriteModState(name->mod, w);
		WriteStrings(SortedSet(name->reserve), w);
	}

	auto schedulerIds = SaveRefs::LiveIds<SchedulerFactory, SchedulerMod>(populace.schedulerFactory);

	// 第一遍：市民自身的字段(不含指向别的市民/班级/组织的引用)。
	w.U32(static_cast<uint32_t>(populace.citizens.size()));
	for (const Citizen* c : populace.citizens) {
		w.Str(c->name);
		w.I32(static_cast<int>(c->gender));
		w.I32(c->birthYear); w.I32(c->birthMonth); w.I32(c->birthDay);
		for (int i = 0; i < kPersonalityCount; i++) w.F32(c->personality[static_cast<PERSONALITY_TYPE>(i)]);
		w.U32(static_cast<uint32_t>(c->acquaintances.size()));
		for (const string& other : SortedNames(c->acquaintances)) {
			const Relation& relation = c->acquaintances.at(other);
			w.Str(other);
			w.I32(static_cast<int>(relation.category));
			for (int i = 0; i < kRelationCount; i++) w.F32(relation[static_cast<RELATION_TYPE>(i)]);
		}
		w.I32(c->lastGraduationYear);
		SaveRefs::WriteRoom(c->room, w);
		SaveRefs::WriteRoom(c->currentRoom, w);
		w.U8(c->hasPosition ? 1 : 0);
		w.F32(c->posX); w.F32(c->posY); w.F32(c->posZ);
		w.F32(c->health);
		w.F32(c->maxHealth);
		WriteStrings(c->options, w);
		const Scheduler* scheduler = c->scheduler;
		w.U8(scheduler ? 1 : 0);
		if (scheduler) {
			w.Str(SaveRefs::LookupId(schedulerIds, static_cast<const SchedulerMod*>(scheduler->mod)));
			SaveRefs::WriteModState(scheduler->mod, w);
		}
	}

	// 班级。
	w.U32(static_cast<uint32_t>(populace.schoolClasses.size()));
	for (const SchoolClass* schoolClass : populace.schoolClasses) {
		w.Str(schoolClass->GetSchoolName());
		w.I32(static_cast<int>(schoolClass->GetLevel()));
		w.I32(schoolClass->GetStartYear());
		const vector<Citizen*>& students = schoolClass->GetStudents();
		w.U32(static_cast<uint32_t>(students.size()));
		for (const Citizen* student : students) w.I32(citizenRef(student));
	}

	// 第二遍：市民之间/市民到班级、组织的引用。
	for (const Citizen* c : populace.citizens) {
		w.I32(citizenRef(c->spouse));
		w.U32(static_cast<uint32_t>(c->children.size()));
		for (const Citizen* child : c->children) w.I32(citizenRef(child));
		w.U32(static_cast<uint32_t>(c->experiences.size()));
		for (const Experience* e : c->experiences) {
			uint8_t kind = EXPERIENCE_PLAIN;
			if (dynamic_cast<const KinshipExperience*>(e)) kind = EXPERIENCE_KINSHIP;
			else if (dynamic_cast<const EmotionExperience*>(e)) kind = EXPERIENCE_EMOTION;
			else if (dynamic_cast<const EducationExperience*>(e)) kind = EXPERIENCE_EDUCATION;
			else if (dynamic_cast<const JobExperience*>(e)) kind = EXPERIENCE_JOB;
			w.U8(kind);
			w.I32(static_cast<int>(e->GetCategory()));
			w.I32(e->GetBeginYear());
			w.I32(e->GetEndYear());
			switch (kind) {
			case EXPERIENCE_KINSHIP: {
				auto* k = static_cast<const KinshipExperience*>(e);
				w.I32(citizenRef(k->GetOther()));
				w.I32(static_cast<int>(k->GetRelativeType()));
				break;
			}
			case EXPERIENCE_EMOTION:
				w.I32(citizenRef(static_cast<const EmotionExperience*>(e)->GetOther()));
				break;
			case EXPERIENCE_EDUCATION: {
				auto it = classIndex.find(static_cast<const EducationExperience*>(e)->GetSchoolClass());
				w.I32(it != classIndex.end() ? it->second : -1);
				break;
			}
			case EXPERIENCE_JOB: {
				auto it = organizationIndex.find(static_cast<const JobExperience*>(e)->GetOrganization());
				w.I32(it != organizationIndex.end() ? it->second : -1);
				break;
			}
			default:
				break;
			}
		}
	}

	// Job/Scheduler定时器。std::set同一时刻的条目按Citizen*地址排序，地址每次运行都不同，
	// 所以换成按(时间, 市民下标, 节点名)排序后写出，保证写出的字节稳定。
	auto writeTimers = [&](const set<tuple<Time, Citizen*, string>>& timers) {
		vector<tuple<Time, int, string>> sorted;
		sorted.reserve(timers.size());
		for (const auto& [time, citizen, node] : timers) sorted.emplace_back(time, citizenRef(citizen), node);
		sort(sorted.begin(), sorted.end());
		w.U32(static_cast<uint32_t>(sorted.size()));
		for (const auto& [time, citizen, node] : sorted) {
			SaveRefs::WriteTime(time, w);
			w.I32(citizen);
			w.Str(node);
		}
		};
	writeTimers(populace.jobTimerSet);
	writeTimers(populace.schedulerTimerSet);

	// 市民和地图之间的归属(地图段只存了stated，owner/tenants/occupants指向市民，放在人口段)。
	vector<string> zoneNames;
	for (auto& [zoneName, zone] : map.GetZones()) zoneNames.push_back(zoneName);
	sort(zoneNames.begin(), zoneNames.end());
	w.U32(static_cast<uint32_t>(zoneNames.size()));
	for (const string& zoneName : zoneNames) {
		w.Str(zoneName);
		w.I32(citizenRef(map.GetZone(zoneName)->GetOwner()));
	}
	vector<string> buildingNames;
	for (auto& [buildingName, building] : map.GetBuildings()) buildingNames.push_back(buildingName);
	sort(buildingNames.begin(), buildingNames.end());
	w.U32(static_cast<uint32_t>(buildingNames.size()));
	for (const string& buildingName : buildingNames) {
		const Building* building = map.GetBuilding(buildingName);
		w.Str(buildingName);
		w.I32(citizenRef(building->GetOwner()));
		const vector<Room*>& rooms = building->GetRooms();
		w.U32(static_cast<uint32_t>(rooms.size()));
		for (const Room* room : rooms) {
			w.I32(citizenRef(room->GetOwner()));
			w.U32(static_cast<uint32_t>(room->GetTenants().size()));
			for (const Citizen* tenant : room->GetTenants()) w.I32(citizenRef(tenant));
			w.U32(static_cast<uint32_t>(room->GetOccupants().size()));
			for (const Citizen* occupant : room->GetOccupants()) w.I32(citizenRef(occupant));
		}
	}

	archive.SetBinary(SAVE_SECTION_POPULACE, VersionOf(SAVE_SECTION_POPULACE), w.Take());
}

Populace* PopulaceSaveCodec::Load(const SaveArchive& archive, Map& map,
	vector<pair<JobExperience*, int>>& outPendingOrganizations, string& error) {
	outPendingOrganizations.clear();
	const SaveSection* section = archive.Find(SAVE_SECTION_POPULACE);
	if (!section) return nullptr;

	try {
		ByteReader r(section->data);
		bool restoreOptions = archive.Find(SAVE_SECTION_SCRIPTS) != nullptr;
		Populace* populace = new Populace();
		populace->currentYear = r.I32();

		if (r.U8() != 0) {
			string nameId = r.Str();
			if (!populace->nameFactory.CheckRegistered(nameId)) {
				error = "name mod '" + nameId + "' is not available";
				return nullptr;
			}
			populace->nameFactory.SetConfig(nameId, true);
			populace->name = new Name(&populace->nameFactory, nameId);
			if (!SaveRefs::ReadModState(populace->name->mod, r)) debugf("Warning: failed to parse saved state of name mod %s.\n", nameId.c_str());
			for (const string& reserved : ReadStrings(r)) populace->name->reserve.insert(reserved);
		}

		uint32_t citizenCount = r.U32();
		vector<Citizen*>& citizens = populace->citizens;
		for (uint32_t i = 0; i < citizenCount && !r.Failed(); i++) {
			string citizenName = r.Str();
			GENDER_TYPE gender = static_cast<GENDER_TYPE>(r.I32());
			int birthYear = r.I32(), birthMonth = r.I32(), birthDay = r.I32();
			Citizen* c = new Citizen(citizenName, gender, birthYear, birthMonth, birthDay);
			citizens.push_back(c);
			for (int k = 0; k < kPersonalityCount; k++) c->personality[static_cast<PERSONALITY_TYPE>(k)] = r.F32();
			uint32_t acquaintanceCount = r.U32();
			for (uint32_t k = 0; k < acquaintanceCount && !r.Failed(); k++) {
				string other = r.Str();
				RELATIONSHIP_CATEGORY category = static_cast<RELATIONSHIP_CATEGORY>(r.I32());
				Relation relation(category);
				for (int m = 0; m < kRelationCount; m++) relation[static_cast<RELATION_TYPE>(m)] = r.F32();
				c->acquaintances.insert_or_assign(other, relation);
			}
			c->lastGraduationYear = r.I32();
			// 住处：lot/zone/building按Map::Checkin同一套规则从房间推出来。
			c->room = SaveRefs::ReadRoom(map, r);
			c->currentRoom = SaveRefs::ReadRoom(map, r);
			if (c->room) {
				Building* building = c->room->GetParentBuilding();
				c->building = building;
				if (building) {
					c->zone = building->GetParentZone();
					c->lot = building->GetParentLot();
				}
			}
			c->hasPosition = r.U8() != 0;
			c->posX = r.F32(); c->posY = r.F32(); c->posZ = r.F32();
			c->health = r.F32();
			c->maxHealth = r.F32();
			// options(剧情脚本加的交互选项)：存档带脚本状态段时读档会跳过game_start广播，按存档恢复；
			// 没有(P4及更早的存档)时game_start会重新加一遍，这里不恢复，避免重复。
			vector<string> options = ReadStrings(r);
			if (restoreOptions) c->options = options;
			if (r.U8() != 0) {
				string schedulerId = r.Str();
				c->scheduler = new Scheduler(&populace->schedulerFactory, &populace->scriptFactory, schedulerId, c);
				if (!SaveRefs::ReadModState(c->scheduler->mod, r)) debugf("Warning: failed to parse saved state of scheduler mod %s.\n", schedulerId.c_str());
			}
		}
		auto citizenAt = [&](int index) -> Citizen* {
			return (index >= 0 && index < static_cast<int>(citizens.size())) ? citizens[index] : nullptr;
			};

		uint32_t classCount = r.U32();
		for (uint32_t i = 0; i < classCount && !r.Failed(); i++) {
			string schoolName = r.Str();
			EDUCATION_LEVEL level = static_cast<EDUCATION_LEVEL>(r.I32());
			int startYear = r.I32();
			SchoolClass* schoolClass = new SchoolClass(schoolName, level, startYear);
			populace->schoolClasses.push_back(schoolClass);
			uint32_t students = r.U32();
			for (uint32_t k = 0; k < students && !r.Failed(); k++) {
				if (Citizen* student = citizenAt(r.I32())) schoolClass->AddStudent(student);
			}
		}
		auto classAt = [&](int index) -> SchoolClass* {
			return (index >= 0 && index < static_cast<int>(populace->schoolClasses.size())) ? populace->schoolClasses[index] : nullptr;
			};

		for (Citizen* c : citizens) {
			if (r.Failed()) break;
			c->spouse = citizenAt(r.I32());
			uint32_t childCount = r.U32();
			for (uint32_t k = 0; k < childCount && !r.Failed(); k++) {
				if (Citizen* child = citizenAt(r.I32())) c->children.push_back(child);
			}
			uint32_t experienceCount = r.U32();
			for (uint32_t k = 0; k < experienceCount && !r.Failed(); k++) {
				uint8_t kind = r.U8();
				RELATIONSHIP_CATEGORY category = static_cast<RELATIONSHIP_CATEGORY>(r.I32());
				int beginYear = r.I32();
				int endYear = r.I32();
				Experience* e = nullptr;
				switch (kind) {
				case EXPERIENCE_KINSHIP: {
					Citizen* other = citizenAt(r.I32());
					RELATIVE_TYPE relativeType = static_cast<RELATIVE_TYPE>(r.I32());
					e = new KinshipExperience(other, relativeType, beginYear);
					break;
				}
				case EXPERIENCE_EMOTION:
					e = new EmotionExperience(citizenAt(r.I32()), beginYear, endYear);
					break;
				case EXPERIENCE_EDUCATION:
					e = new EducationExperience(classAt(r.I32()), beginYear, endYear);
					break;
				case EXPERIENCE_JOB: {
					int organizationIndex = r.I32();
					JobExperience* job = new JobExperience(nullptr, beginYear, endYear);
					outPendingOrganizations.emplace_back(job, organizationIndex);
					e = job;
					break;
				}
				default:
					e = new Experience(category, beginYear, endYear);
					break;
				}
				c->experiences.push_back(e);
			}
		}

		auto readTimers = [&](set<tuple<Time, Citizen*, string>>& timers) {
			uint32_t n = r.U32();
			for (uint32_t i = 0; i < n && !r.Failed(); i++) {
				Time time = SaveRefs::ReadTime(r);
				Citizen* citizen = citizenAt(r.I32());
				string node = r.Str();
				if (citizen) timers.insert({ time, citizen, node });
			}
			};
		readTimers(populace->jobTimerSet);
		readTimers(populace->schedulerTimerSet);

		uint32_t zoneCount = r.U32();
		for (uint32_t i = 0; i < zoneCount && !r.Failed(); i++) {
			string zoneName = r.Str();
			Citizen* owner = citizenAt(r.I32());
			if (Zone* zone = map.GetZone(zoneName)) zone->SetOwner(owner);
		}
		uint32_t buildingCount = r.U32();
		for (uint32_t i = 0; i < buildingCount && !r.Failed(); i++) {
			string buildingName = r.Str();
			Citizen* owner = citizenAt(r.I32());
			Building* building = map.GetBuilding(buildingName);
			if (building) building->SetOwner(owner);
			uint32_t roomCount = r.U32();
			for (uint32_t k = 0; k < roomCount && !r.Failed(); k++) {
				Room* room = (building && k < building->GetRooms().size()) ? building->GetRooms()[k] : nullptr;
				Citizen* roomOwner = citizenAt(r.I32());
				if (room) room->SetOwner(roomOwner);
				uint32_t tenants = r.U32();
				for (uint32_t m = 0; m < tenants && !r.Failed(); m++) {
					Citizen* tenant = citizenAt(r.I32());
					if (room && tenant) room->AddTenant(tenant);
				}
				uint32_t occupants = r.U32();
				for (uint32_t m = 0; m < occupants && !r.Failed(); m++) {
					Citizen* occupant = citizenAt(r.I32());
					if (room && occupant) room->AddOccupant(occupant);
				}
			}
		}

		if (r.Failed()) {
			error = "populace section is truncated";
			return nullptr;
		}
		return populace;
	}
	catch (const exception& e) {
		error = string("populace load failed: ") + e.what();
		return nullptr;
	}
	catch (...) {
		error = "populace load failed with an unknown error";
		return nullptr;
	}
}
