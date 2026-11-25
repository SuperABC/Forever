#include "polit/polit_save.h"

#include "polit/polit.h"
#include "polit/party_factory.h"
#include "map/map.h"
#include "populace/populace.h"
#include "populace/citizen.h"

#include "common/save.h"
#include "common/save_refs.h"
#include "common/utility.h"

#include <algorithm>
#include <memory>
#include <unordered_map>


using namespace std;

namespace {
	int VersionOf(const char* name) {
		for (const SaveSectionVersion& v : GetSaveSectionVersions()) {
			if (string(v.name) == name) return v.current;
		}
		return 1;
	}

	void WriteClock(const Time& t, ByteWriter& w) {
		w.I32(t.GetHour());
		w.I32(t.GetMinute());
		w.I32(t.GetSecond());
	}

	Time ReadClock(ByteReader& r) {
		int h = r.I32(), m = r.I32(), s = r.I32();
		return Time(1, 1, 1, h, m, s);
	}
}

void PolitSaveCodec::Save(const Polit& polit, const Populace& populace, SaveArchive& archive) {
	ByteWriter w;

	unordered_map<const Citizen*, int> citizenIndex;
	const vector<Citizen*>& citizens = populace.GetCitizens();
	for (size_t i = 0; i < citizens.size(); i++) citizenIndex[citizens[i]] = static_cast<int>(i);
	auto citizenRef = [&](const Citizen* c) {
		if (!c) return -1;
		auto it = citizenIndex.find(c);
		return it != citizenIndex.end() ? it->second : -1;
	};

	w.I32(polit.lastMonthKey);

	// 政策值：按目录顺序(id+值)。
	const vector<PolicyDef>& defs = PolicyCatalog::All();
	w.U32(static_cast<uint32_t>(defs.size()));
	for (const PolicyDef& def : defs) {
		w.Str(def.id);
		w.F64(def.get(polit.policy));
	}

	auto partyIds = SaveRefs::LiveIds<PartyFactory, PartyMod>(polit.factory);
	w.U32(static_cast<uint32_t>(polit.parties.size()));
	for (const Party* party : polit.parties) {
		w.Str(SaveRefs::LookupId(partyIds, party->GetMod()));
		SaveRefs::WriteModState(party->GetMod(), w);
		w.Str(party->meetingAddress);
		w.I32(party->terms);
		w.U8(party->ruling ? 1 : 0);
		w.I32(citizenRef(party->leader));
		w.U32(static_cast<uint32_t>(party->members.size()));
		for (const Citizen* member : party->members) w.I32(citizenRef(member));

		const Party::Congress& congress = party->congress;
		w.I32(congress.monthKey);
		w.I32(congress.plan.day);
		WriteClock(congress.plan.start, w);
		WriteClock(congress.plan.end, w);
		w.U8(congress.started ? 1 : 0);
		w.U8(congress.ended ? 1 : 0);
		w.U8(congress.skipLogged ? 1 : 0);

		w.U32(static_cast<uint32_t>(party->powers.size()));
		for (const auto& entry : party->powers) {
			w.Str(entry.first);
			w.I32(citizenRef(entry.second));
		}
	}

	archive.SetBinary(SAVE_SECTION_POLIT, VersionOf(SAVE_SECTION_POLIT), w.Take());
}

Polit* PolitSaveCodec::Load(const SaveArchive& archive, Map& map, Populace& populace, Society& society, string& error) {
	const SaveSection* section = archive.Find(SAVE_SECTION_POLIT);
	if (!section) {
		error = "the save has no section '" SAVE_SECTION_POLIT "'";
		return nullptr;
	}

	try {
		ByteReader r(section->data);
		const vector<Citizen*>& citizens = populace.GetCitizens();
		auto citizenAt = [&](int index) -> Citizen* {
			return index >= 0 && index < static_cast<int>(citizens.size()) ? citizens[index] : nullptr;
		};

		unique_ptr<Polit> polit(new Polit());
		PartyFactory& factory = polit->factory;
		polit->lastMonthKey = r.I32();

		uint32_t policyCount = r.U32();
		for (uint32_t i = 0; i < policyCount && !r.Failed(); i++) {
			string id = r.Str();
			double value = r.F64();
			const PolicyDef* def = PolicyCatalog::Find(id);
			if (def) PolicyCatalog::Apply(polit->policy, *def, value);
		}

		uint32_t partyCount = r.U32();
		for (uint32_t i = 0; i < partyCount && !r.Failed(); i++) {
			string id = r.Str();
			PartyMod* mod = id.empty() ? nullptr : factory.CreateParty(id);
			if (!SaveRefs::ReadModState(mod, r)) {
				debugf(DebugLevel::Warning, "failed to parse saved state of party mod %s.\n", id.c_str());
			}
			Party scratchParty(nullptr, nullptr, id); // mod不可用时也要把数据读完
			Party* party = mod ? new Party(&factory, mod, id) : &scratchParty;
			unique_ptr<Party> owner(mod ? party : nullptr);

			party->meetingAddress = r.Str();
			party->terms = r.I32();
			party->ruling = r.U8() != 0;
			party->leader = citizenAt(r.I32());
			uint32_t memberCount = r.U32();
			for (uint32_t k = 0; k < memberCount && !r.Failed(); k++) {
				Citizen* member = citizenAt(r.I32());
				if (member) party->members.push_back(member);
			}

			party->congress.monthKey = r.I32();
			party->congress.plan.day = r.I32();
			party->congress.plan.start = ReadClock(r);
			party->congress.plan.end = ReadClock(r);
			party->congress.started = r.U8() != 0;
			party->congress.ended = r.U8() != 0;
			party->congress.skipLogged = r.U8() != 0;

			uint32_t powerCount = r.U32();
			for (uint32_t k = 0; k < powerCount && !r.Failed(); k++) {
				string powerId = r.Str();
				Citizen* holder = citizenAt(r.I32());
				if (holder) party->powers.push_back({ powerId, holder });
			}

			if (mod) {
				polit->parties.push_back(owner.release());
			}
			else {
				debugf(DebugLevel::Warning, "party mod %s is not available, party dropped.\n", id.c_str());
			}
		}

		if (r.Failed()) {
			error = "polit section is truncated";
			return nullptr;
		}
		polit->Bind(map, populace, society);
		return polit.release();
	}
	catch (const exception& e) {
		error = string("polit load failed: ") + e.what();
		return nullptr;
	}
	catch (...) {
		error = "polit load failed with an unknown error";
		return nullptr;
	}
}
