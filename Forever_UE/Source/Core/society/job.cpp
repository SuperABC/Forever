#include "society/job.h"

#include "story/script.h"
#include "story/script_factory.h"
#include "common/config.h"
#include "populace/citizen.h"
#include "map/room.h"

using namespace std;

Job::Job(JobFactory* factory, ScriptFactory* scriptFactory, const string& id, Room* position) :
	factory(factory), position(position) {
	mod = factory->CreateJob(id);
	// scriptModName/milestoneNames由mod的SetProperty()填好，都空则没有脚本，见MakeScript。
	if (mod) mod->SetProperty();
	script = mod ? MakeScript(scriptFactory, mod->scriptModName, mod->milestoneNames) : nullptr;
	if (script) {
		// 供milestone脚本里$$self.name引用这个Job的唯一名字（JobMod的GetName()）。
		script->SetValue("name", ValueType(string(mod->GetName())));
	}
}

Job::~Job() {
	delete script;
	if (mod) factory->DestroyJob(mod);
}

string Job::GetType() const {
	return mod ? mod->GetType() : string();
}

Room* Job::GetPosition() const { return position; }
Script* Job::GetScript() const { return script; }

Citizen* Job::GetOccupant() const { return occupant; }
void Job::SetOccupant(Citizen* citizen) {
	occupant = citizen;
	// 给Job脚本暴露上班的人和通勤地址，脚本里用$$self.occupant/home/workplace
	// 驱动上下班(npc_navigate + create_timer)，见job.md"脚本驱动通勤"。空岗时occupant/地址为空串。
	if (script) {
		Room* home = citizen ? citizen->GetRoom() : nullptr;
		script->SetValue("occupant", ValueType(citizen ? citizen->GetName() : string()));
		script->SetValue("home", ValueType(home ? home->GetAddress() : string()));
		script->SetValue("workplace", ValueType(position ? position->GetAddress() : string()));
	}
}

void Job::DailyPlan(const Time& currentTime, PostHandle* post) {
	if (mod) mod->DailyPlan(currentTime, post);
}

const unordered_map<string, Time>& Job::GetPlans() const {
	static const unordered_map<string, Time> empty;
	return mod ? mod->plans : empty;
}

vector<Change*> Job::ExecNode(const string& node, PostHandle* post) {
	if (!mod) return {};
	mod->occupantName = occupant ? occupant->GetName() : string();
	mod->ExecNode(node, post);
	return mod->changes; // 只复制指针值，所有权留在mod自己身上
}
