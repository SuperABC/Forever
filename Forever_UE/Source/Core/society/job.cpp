#include "society/job.h"

#include "story/script.h"
#include "story/script_factory.h"
#include "common/config.h"
#include "populace/citizen.h"

using namespace std;

Job::Job(JobFactory* factory, ScriptFactory* scriptFactory, const string& id, Room* position) :
	factory(factory), position(position) {
	mod = factory->CreateJob(id);
	// scriptModName/milestoneNames由mod自己的构造函数决定(见job_mod.h"Script配置"
	// 一节)，这里不替mod做任何选择——mod为空是CreateJob本身失败的防御性兜底，不是
	// "决定用哪个ScriptMod"这个设计选择，因此仍然硬编码"empty"作为最后一道保险。
	script = new Script(scriptFactory, mod ? mod->scriptModName : "empty");
	if (mod) {
		for (const string& name : mod->milestoneNames) {
			script->ReadMilestones(Config::GetScriptPath(name));
		}
		// 供milestone脚本里$$self.name引用这个Job的唯一名字（不是Script自己的
		// mod->GetName()，是JobMod自己的GetName()，见job_basic.cpp"八"的唯一性修复），
		// 见job_mod.h"Script配置"一节。
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
void Job::SetOccupant(Citizen* citizen) { occupant = citizen; }

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
	return mod->changes; // 只复制指针值，所有权留在mod自己身上，见job.h的说明
}
