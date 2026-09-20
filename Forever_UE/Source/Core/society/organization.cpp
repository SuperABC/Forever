#include "society/organization.h"

#include "society/job.h"
#include "map/component.h"
#include "map/room.h"

using namespace std;

Organization::Organization(OrganizationFactory* factory, JobFactory* jobFactory, ScriptFactory* scriptFactory,
	const string& id, vector<Component*> components) :
	factory(factory), components(move(components)) {
	mod = factory->CreateOrganization(id);
	if (!mod) return;

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
	if (mod) factory->DestroyOrganization(mod);
}

string Organization::GetType() const {
	return mod ? mod->GetType() : string();
}

const vector<Component*>& Organization::GetComponents() const { return components; }
const vector<Job*>& Organization::GetJobs() const { return jobs; }

void Organization::DailyPlan(const Time& currentTime) {
	if (mod) mod->DailyPlan(currentTime);
}

const unordered_map<string, Time>& Organization::GetPlans() const {
	static const unordered_map<string, Time> empty;
	return mod ? mod->plans : empty;
}

vector<Change*> Organization::ExecNode(const string& node) {
	if (!mod) return {};
	mod->ExecNode(node);
	return mod->changes;
}
