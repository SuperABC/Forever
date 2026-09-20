#include "society/organization.h"

#include "society/job.h"
#include "story/script.h"
#include "story/script_factory.h"
#include "common/config.h"
#include "map/component.h"
#include "map/room.h"

using namespace std;

Organization::Organization(OrganizationFactory* factory, JobFactory* jobFactory, ScriptFactory* scriptFactory,
	const string& id, vector<Component*> components) :
	factory(factory), script(nullptr), components(move(components)) {
	mod = factory->CreateOrganization(id);
	if (!mod) return;

	// scriptModName/milestoneNames由mod自己的构造函数决定，Core不替mod做任何选择，
	// 和Job::Job()同一套机制，见organization_mod.h"Script配置"一节。
	script = new Script(scriptFactory, mod->scriptModName);
	for (const string& name : mod->milestoneNames) {
		script->ReadMilestones(Config::GetScriptPath(name));
	}
	// 供milestone脚本里$$self.name引用这个Organization的唯一名字，见job.cpp同款注释。
	script->SetValue("name", ValueType(string(mod->GetName())));

	for (Component* component : this->components) {
		if (!component) continue;
		for (Room* room : component->GetRooms()) {
			if (!room || !room->IsWorkspace()) continue;

			mod->vacancies.clear();
			mod->DesignJobsForRoom(component->GetType(), component->GetName(),
				room->GetType(), room->GetName(), room->WorkspaceCapacity());
			for (const string& jobType : mod->vacancies) {
				jobs.push_back(new Job(jobFactory, scriptFactory, jobType, room));
			}
		}
	}
}

Organization::~Organization() {
	for (Job* job : jobs) {
		delete job;
	}
	delete script;
	if (mod) factory->DestroyOrganization(mod);
}

string Organization::GetType() const {
	return mod ? mod->GetType() : string();
}

const vector<Component*>& Organization::GetComponents() const { return components; }
const vector<Job*>& Organization::GetJobs() const { return jobs; }
Script* Organization::GetScript() const { return script; }

void Organization::DailyPlan(const Time& currentTime, PostHandle* post) {
	if (mod) mod->DailyPlan(currentTime, post);
}

const unordered_map<string, Time>& Organization::GetPlans() const {
	static const unordered_map<string, Time> empty;
	return mod ? mod->plans : empty;
}

vector<Change*> Organization::ExecNode(const string& node, PostHandle* post) {
	if (!mod) return {};
	mod->ExecNode(node, post);
	return mod->changes;
}
