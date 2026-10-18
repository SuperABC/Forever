#include "script_save.h"

#include "story/script.h"
#include "story/story.h"
#include "map/map.h"
#include "map/door.h"
#include "populace/populace.h"
#include "populace/citizen.h"
#include "populace/scheduler.h"
#include "society/society.h"
#include "society/organization.h"
#include "society/job.h"
#include "traffic/traffic.h"

#include "common/save.h"
#include "common/save_refs.h"
#include "common/utility.h"

#include <algorithm>
#include <unordered_map>


using namespace std;

namespace {
	int VersionOf(const char* name) {
		for (const SaveSectionVersion& v : GetSaveSectionVersions()) {
			if (string(v.name) == name) return v.current;
		}
		return 1;
	}

	// ValueType = variant<int, double, bool, string>：类型下标 + 值。
	void WriteValue(const ValueType& value, ByteWriter& w) {
		w.U8(static_cast<uint8_t>(value.index()));
		switch (value.index()) {
		case 0: w.I32(get<int>(value)); break;
		case 1: w.F64(get<double>(value)); break;
		case 2: w.U8(get<bool>(value) ? 1 : 0); break;
		default: w.Str(get<string>(value)); break;
		}
	}
	ValueType ReadValue(ByteReader& r) {
		switch (r.U8()) {
		case 0: return ValueType(r.I32());
		case 1: return ValueType(r.F64());
		case 2: return ValueType(r.U8() != 0);
		default: return ValueType(r.Str());
		}
	}
}

vector<pair<string, Script*>> ScriptSaveCodec::CollectScripts(Map* map, Populace* populace,
	Society* society, Traffic* traffic, Story* story) {
	vector<pair<string, Script*>> scripts;
	if (story) {
		if (story->GetMainScript()) scripts.emplace_back("story.main", story->GetMainScript());
		if (story->GetSystemScript()) scripts.emplace_back("story.system", story->GetSystemScript());
	}
	if (map) {
		for (Door* door : map->GetAllDoors()) {
			if (door && door->GetScript()) scripts.emplace_back("door:" + door->GetId(), door->GetScript());
		}
	}
	if (society) {
		const vector<Organization*>& organizations = society->GetOrganizations();
		for (size_t i = 0; i < organizations.size(); i++) {
			Organization* o = organizations[i];
			if (!o) continue;
			if (o->GetScript()) scripts.emplace_back("org:" + to_string(i), o->GetScript());
			const vector<Job*>& jobs = o->GetJobs();
			for (size_t k = 0; k < jobs.size(); k++) {
				if (jobs[k] && jobs[k]->GetScript()) scripts.emplace_back("job:" + to_string(i) + ":" + to_string(k), jobs[k]->GetScript());
			}
		}
	}
	if (populace) {
		const vector<Citizen*>& citizens = populace->GetCitizens();
		for (size_t i = 0; i < citizens.size(); i++) {
			Scheduler* scheduler = citizens[i] ? citizens[i]->GetScheduler() : nullptr;
			if (scheduler && scheduler->GetScript()) scripts.emplace_back("scheduler:" + to_string(i), scheduler->GetScript());
		}
	}
	if (traffic) {
		for (auto& [name, vehicle] : traffic->GetVehicles()) {
			if (vehicle && vehicle->GetScript()) scripts.emplace_back("vehicle:" + name, vehicle->GetScript());
		}
	}
	sort(scripts.begin(), scripts.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
	return scripts;
}

void ScriptSaveCodec::Save(const SaveWorld& world, SaveArchive& archive) {
	vector<pair<string, Script*>> scripts = CollectScripts(const_cast<Map*>(world.map), const_cast<Populace*>(world.populace),
		const_cast<Society*>(world.society), const_cast<Traffic*>(world.traffic), const_cast<Story*>(world.story));

	ByteWriter w;
	w.U32(static_cast<uint32_t>(scripts.size()));
	for (auto& [key, script] : scripts) {
		w.Str(key);
		w.Str(script->type);
		SaveRefs::WriteModState(script->mod, w);

		vector<string> milestoneNames;
		unordered_map<const MilestoneNode*, string> nodeNames;
		for (auto& [name, node] : script->milestones) {
			milestoneNames.push_back(name);
			nodeNames[&node] = name;
		}
		sort(milestoneNames.begin(), milestoneNames.end());
		w.U32(static_cast<uint32_t>(milestoneNames.size()));
		for (const string& name : milestoneNames) {
			w.Str(name);
			w.I32(script->milestones.at(name).premise);
		}

		w.U32(static_cast<uint32_t>(script->actives.size()));
		for (const MilestoneNode* node : script->actives) {
			auto it = nodeNames.find(node);
			w.Str(it != nodeNames.end() ? it->second : string());
		}

		vector<string> variableNames;
		for (auto& [name, value] : script->variables) variableNames.push_back(name);
		sort(variableNames.begin(), variableNames.end());
		w.U32(static_cast<uint32_t>(variableNames.size()));
		for (const string& name : variableNames) {
			w.Str(name);
			WriteValue(script->variables.at(name), w);
		}
	}

	archive.SetBinary(SAVE_SECTION_SCRIPTS, VersionOf(SAVE_SECTION_SCRIPTS), w.Take());
}

bool ScriptSaveCodec::Load(const SaveArchive& archive, Map* map, Populace* populace, Society* society,
	Traffic* traffic, Story* story, string& error) {
	const SaveSection* section = archive.Find(SAVE_SECTION_SCRIPTS);
	if (!section) return false;

	unordered_map<string, Script*> byKey;
	for (auto& [key, script] : CollectScripts(map, populace, society, traffic, story)) byKey[key] = script;

	ByteReader r(section->data);
	int missing = 0;
	uint32_t count = r.U32();
	for (uint32_t i = 0; i < count && !r.Failed(); i++) {
		string key = r.Str();
		string type = r.Str();
		int stateVersion = r.I32();
		string stateText = r.Str();
		vector<pair<string, int>> premises(r.U32());
		for (auto& [name, premise] : premises) { name = r.Str(); premise = r.I32(); }
		vector<string> actives(r.U32());
		for (string& name : actives) name = r.Str();
		vector<pair<string, ValueType>> variables;
		uint32_t variableCount = r.U32();
		for (uint32_t k = 0; k < variableCount && !r.Failed(); k++) {
			string name = r.Str();
			variables.emplace_back(name, ReadValue(r));
		}
		if (r.Failed()) break;

		auto it = byKey.find(key);
		Script* script = it != byKey.end() ? it->second : nullptr;
		// 持有者不在了，或者它现在用的ScriptMod类型和存档时不同(mod配置变了)，这条状态没法套用。
		if (!script || script->type != type) {
			missing++;
			continue;
		}

		if (script->mod && !stateText.empty()) {
			JsonValue root;
			JsonReader reader;
			if (reader.Parse(stateText, root, false)) {
				JsonStateReader stateReader(root, stateVersion);
				script->mod->LoadState(stateReader);
			}
		}
		for (auto& [name, premise] : premises) {
			auto node = script->milestones.find(name);
			if (node != script->milestones.end()) node->second.premise = premise;
		}
		script->actives.clear();
		for (const string& name : actives) {
			auto node = script->milestones.find(name);
			if (node != script->milestones.end()) script->actives.push_back(&node->second);
		}
		script->variables.clear();
		for (auto& [name, value] : variables) script->variables[name] = value;
	}

	if (r.Failed()) {
		error = "scripts section is truncated";
		return false;
	}
	if (missing > 0) debugf("Warning: %d saved script states had no matching script and were skipped.\n", missing);
	return true;
}
