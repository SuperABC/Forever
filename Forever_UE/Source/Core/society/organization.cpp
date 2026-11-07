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

	// scriptModName/milestoneNames由mod决定，都空则没有脚本，和Job同一套机制。
	script = MakeScript(scriptFactory, mod->scriptModName, mod->milestoneNames);
	if (script) {
		// 供milestone脚本里$$self.name引用这个Organization的唯一名字。
		script->SetValue("name", ValueType(string(mod->GetName())));
	}

	for (Component* component : this->components) {
		if (!component) continue;
		for (Room* room : component->GetRooms()) {
			if (!room || !room->IsWorkspace()) continue;

			// 不能在这里clear：vacancies里的string在mod堆上分配，Core(UE模块重载了operator delete)
			// 释放会崩溃；由mod在DesignJobsForRoom开头自己清空。
			mod->DesignJobsForRoom(component->GetType(), component->GetName(),
				room->GetType(), room->GetName(), room->WorkspaceCapacity());
			for (const string& jobType : mod->vacancies) {
				// jobType对应的JobMod未启用时Job::GetType()为空，这种哑岗位会白占citizen名额，丢弃。
				Job* job = new Job(jobFactory, scriptFactory, jobType, room);
				if (!job->GetType().empty()) {
					jobs.push_back(job);
				}
				else {
					delete job;
				}
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
bool Organization::IsBankrupt() const { return bankrupt; }
void Organization::SetBankrupt(bool value) { bankrupt = value; }

const vector<Organization::Shareholder>& Organization::GetShareholders() const { return shareholders; }
void Organization::SetShareholders(const vector<Shareholder>& value) { shareholders = value; }

Script* Organization::GetScript() const { return script; }
OrganizationMod* Organization::GetMod() const { return mod; }

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
