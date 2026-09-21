#include "populace/scheduler.h"

#include "populace/citizen.h"
#include "story/script.h"
#include "story/script_factory.h"
#include "common/config.h"

using namespace std;

Scheduler::Scheduler(SchedulerFactory* factory, ScriptFactory* scriptFactory, const string& id, Citizen* citizen) :
	factory(factory), citizen(citizen) {
	mod = factory->CreateScheduler(id);
	// scriptModName/milestoneNames由mod自己的构造函数决定(见scheduler_mod.h"Script配置"
	// 一节)，mod为空是CreateScheduler本身失败的防御性兜底，仍然硬编码"empty"作为最后一道
	// 保险，和Job::Job同一个写法。
	script = new Script(scriptFactory, mod ? mod->scriptModName : "empty");
	if (mod) {
		for (const string& name : mod->milestoneNames) {
			script->ReadMilestones(Config::GetScriptPath(name));
		}
		// 供milestone脚本里$$self.name引用这个Scheduler绑定的市民姓名——和Job用
		// mod->GetName()不同，Scheduler从构造起就唯一绑定一个citizen，直接用citizen自己
		// 的名字更贴切。
		script->SetValue("name", ValueType(citizen ? citizen->GetName() : string()));
	}
}

Scheduler::~Scheduler() {
	delete script;
	if (mod) factory->DestroyScheduler(mod);
}

string Scheduler::GetType() const {
	return mod ? mod->GetType() : string();
}

Script* Scheduler::GetScript() const { return script; }

void Scheduler::DailyPlan(const Time& currentTime, PostHandle* post) {
	if (mod) mod->DailyPlan(currentTime, post);
}

const unordered_map<string, Time>& Scheduler::GetPlans() const {
	static const unordered_map<string, Time> empty;
	return mod ? mod->plans : empty;
}

vector<Change*> Scheduler::ExecNode(const string& node, PostHandle* post) {
	if (!mod) return {};
	mod->occupantName = citizen ? citizen->GetName() : string();
	mod->ExecNode(node, post);
	return mod->changes; // 只复制指针值，所有权留在mod自己身上，见Job::ExecNode同款说明
}
