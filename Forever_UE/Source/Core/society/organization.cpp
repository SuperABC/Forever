#include "society/organization.h"

#include "society/job.h"
#include "story/script.h"
#include "story/script_factory.h"
#include "common/config.h"
#include "map/component.h"
#include "map/room.h"
#include "populace/citizen.h"

#include <map>
#include <set>
#include <utility>

using namespace std;

Organization::Organization(OrganizationFactory* factory, JobFactory* jobFactory, ScriptFactory* scriptFactory,
	const string& id, vector<Component*> components) :
	factory(factory), jobFactory(jobFactory), scriptFactory(scriptFactory), script(nullptr), components(move(components)) {
	mod = factory->CreateOrganization(id);
	if (!mod) return;

	// scriptModName/milestoneNames由mod决定，都空则没有脚本，和Job同一套机制。
	script = MakeScript(scriptFactory, mod->scriptModName, mod->milestoneNames);
	if (script) {
		// 供milestone脚本里$$self.name引用这个Organization的唯一名字。
		script->SetValue("name", ValueType(string(mod->GetName())));
	}

	// 岗位计划由mod写进vacancies（Core不能清它：里面的string在mod堆上分配，Core侧释放会跨DLL堆崩溃），
	// 再按计划创建Job。读档时components为空，这里什么也不做，岗位和计划都从存档恢复。
	DesignJobs(false);
	ReconcileJobs();
}

vector<Room*> Organization::WorkspaceRooms() const {
	vector<Room*> result;
	for (Component* component : components) {
		if (!component) continue;
		for (Room* room : component->GetRooms()) {
			if (room && room->IsWorkspace()) result.push_back(room);
		}
	}
	return result;
}

void Organization::DesignJobs(bool reset) {
	if (!mod) return;
	bool first = true;
	for (Component* component : components) {
		if (!component) continue;
		for (Room* room : component->GetRooms()) {
			if (!room || !room->IsWorkspace()) continue;
			mod->DesignJobsForRoom(component->GetType(), component->GetName(),
				room->GetType(), room->GetName(), room->WorkspaceCapacity(), reset && first);
			first = false;
		}
	}
}

Organization::JobChange Organization::ReconcileJobs() {
	JobChange change;
	if (!mod) return change;

	unordered_map<string, Room*> roomByName;
	for (Room* room : WorkspaceRooms()) roomByName[room->GetName()] = room;

	// 计划：(房间, 岗位类型) -> 条数；每个房间总计划数不超过容量的kMaxOvercrowd倍，超出的条目忽略。
	using Key = pair<Room*, string>;
	map<Key, int> want;
	map<Room*, int> roomTotal;
	for (const Vacancy& v : mod->vacancies) {
		auto it = roomByName.find(v.room);
		if (it == roomByName.end()) continue;
		Room* room = it->second;
		if (roomTotal[room] >= max(1, room->WorkspaceCapacity()) * kMaxOvercrowd) continue;
		roomTotal[room]++;
		want[{ room, v.type }]++;
	}

	// 现状：开放的和已关闭的Job按(房间, 类型)分组。
	map<Key, vector<Job*>> openJobs, closedJobs;
	for (Job* job : jobs) {
		if (!job) continue;
		Key key{ job->GetPosition(), job->GetType() };
		(job->IsOpen() ? openJobs : closedJobs)[key].push_back(job);
	}

	set<Key> keys;
	for (const auto& entry : want) keys.insert(entry.first);
	for (const auto& entry : openJobs) keys.insert(entry.first);

	for (const Key& key : keys) {
		auto wantIt = want.find(key);
		int target = wantIt != want.end() ? wantIt->second : 0;
		vector<Job*>& open = openJobs[key];
		int have = static_cast<int>(open.size());

		if (target > have) {
			// 扩招：先重开这个房间这个类型已关闭的岗位，不够再新建（追加到jobs末尾，已有下标不变）。
			vector<Job*>& closedList = closedJobs[key];
			for (int need = target - have; need > 0; need--) {
				if (!closedList.empty()) {
					closedList.back()->SetClosed(false);
					closedList.pop_back();
					change.opened++;
					continue;
				}
				Job* job = new Job(jobFactory, scriptFactory, key.second, key.first);
				// 类型对应的JobMod未启用时Job::GetType()为空，这种哑岗位会白占citizen名额，丢弃。
				if (job->GetType().empty()) {
					delete job;
					break;
				}
				jobs.push_back(job);
				change.opened++;
			}
		}
		else if (target < have) {
			// 裁员：先关空缺岗位，不够再随机选在岗的，被裁的人变成失业者。
			vector<Job*> vacant, staffed;
			for (Job* job : open) (job->GetOccupant() ? staffed : vacant).push_back(job);
			for (int extra = have - target; extra > 0; extra--) {
				Job* job = nullptr;
				if (!vacant.empty()) {
					job = vacant.back();
					vacant.pop_back();
				}
				else if (!staffed.empty()) {
					size_t index = static_cast<size_t>(GetRandom(static_cast<int>(staffed.size())));
					job = staffed[index];
					staffed[index] = staffed.back();
					staffed.pop_back();
					if (Citizen* worker = job->GetOccupant()) worker->SetJob(nullptr);
					job->SetOccupant(nullptr);
					change.dismissed++;
				}
				else {
					break;
				}
				job->SetClosed(true);
				change.closed++;
			}
		}
	}
	return change;
}

int Organization::LayOffAll() {
	int dismissed = 0;
	for (Job* job : jobs) {
		if (!job) continue;
		if (Citizen* worker = job->GetOccupant()) {
			worker->SetJob(nullptr);
			job->SetOccupant(nullptr);
			dismissed++;
		}
		job->SetClosed(true);
	}
	return dismissed;
}

Organization::JobChange Organization::ReopenInitial() {
	DesignJobs(true);
	return ReconcileJobs();
}

int Organization::EmployeeCount() const {
	int count = 0;
	for (const Job* job : jobs) {
		if (job && job->IsOpen() && job->GetOccupant()) count++;
	}
	return count;
}

int Organization::OpenPositionCount() const {
	int count = 0;
	for (const Job* job : jobs) {
		if (job && job->IsOpen()) count++;
	}
	return count;
}

int64_t Organization::GetLastSettledBalance() const { return lastSettledBalance; }
void Organization::SetLastSettledBalance(int64_t value) { lastSettledBalance = value; }

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

int64_t Organization::GetTotalShares() const { return totalShares; }
void Organization::SetTotalShares(int64_t value) { totalShares = value; }

const vector<Organization::Shareholder>& Organization::GetShareholders() const { return shareholders; }
void Organization::SetShareholders(const vector<Shareholder>& value) { shareholders = value; }

vector<Organization::Shareholder> Organization::SplitShares(const vector<Citizen*>& citizens, const vector<float>& weights) {
	vector<Shareholder> result;
	float sum = 0.f;
	for (float weight : weights) sum += weight;
	if (citizens.empty() || sum <= 0.f) return result;
	int64_t assigned = 0;
	for (size_t i = 0; i < citizens.size(); i++) {
		int64_t shares = i + 1 == citizens.size() ? kTotalShares - assigned
			: static_cast<int64_t>(static_cast<double>(kTotalShares) * weights[i] / sum);
		if (shares > 0) result.push_back({ citizens[i], shares });
		assigned += shares;
	}
	return result;
}

int64_t Organization::SharesOf(const Citizen* citizen) const {
	for (const Shareholder& s : shareholders) {
		if (s.citizen == citizen) return s.shares;
	}
	return 0;
}

void Organization::AdjustShares(Citizen* citizen, int64_t delta) {
	if (!citizen || delta == 0) return;
	for (size_t i = 0; i < shareholders.size(); i++) {
		if (shareholders[i].citizen != citizen) continue;
		shareholders[i].shares += delta;
		if (shareholders[i].shares <= 0) shareholders.erase(shareholders.begin() + i);
		return;
	}
	if (delta > 0) shareholders.push_back({ citizen, delta });
}

int64_t Organization::TreasuryShares() const {
	int64_t held = 0;
	for (const Shareholder& s : shareholders) held += s.shares;
	return totalShares - held;
}

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
