#include "society/society.h"

#include "society/organization.h"
#include "society/job.h"
#include "map/component.h"
#include "populace/citizen.h"
#include "common/registry.h"

#include <algorithm>
#include <unordered_map>

using namespace std;

namespace {
	constexpr int kMinAdultAge = 18;
}

Society::Society() :
	organizationFactory(Registry::Get().GetOrganizationFactory()),
	jobFactory(Registry::Get().GetJobFactory()),
	scriptFactory(Registry::Get().GetScriptFactory()) {
}

Society::~Society() {
	for (Organization* organization : organizations) {
		delete organization;
	}
}

void Society::Init(const vector<Component*>& components) {
	// 第一步：对每个已注册组织类型各构造一个临时mod实例查requirements，读完立刻销毁。
	unordered_map<string, unordered_map<string, pair<int, int>>> requirementsByType;
	for (const string& id : organizationFactory.GetRegisteredIds()) {
		OrganizationMod* temp = organizationFactory.CreateOrganization(id);
		if (!temp) continue;
		temp->ComponentRequirements();
		requirementsByType[id] = temp->requirements;
		organizationFactory.DestroyOrganization(temp);
	}

	// 按类型分组，池子会在下面的循环里被逐步摘取。
	unordered_map<string, vector<Component*>> componentsByType;
	for (Component* component : components) {
		if (component) componentsByType[component->GetType()].push_back(component);
	}

	while (true) {
		// 筛出每种要求的类型在componentsByType里剩余数量都>=min的候选类型。
		vector<string> candidates;
		vector<float> weights;
		for (const auto& [id, requirements] : requirementsByType) {
			// 没有任何Component类型需求的组织类型(比如Forever_Mod/Empty里纯占位的
			// EmptyOrganization)永远"满足"、永远不消耗任何Component，会让下面的循环死循环
			// (每次都能成功分配、但componentsByType的剩余数量永不减少)——跳过这种类型，
			// 不当作候选。
			if (requirements.empty()) continue;
			bool satisfied = true;
			for (const auto& [type, range] : requirements) {
				auto it = componentsByType.find(type);
				int available = it != componentsByType.end() ? static_cast<int>(it->second.size()) : 0;
				if (available < range.first) {
					satisfied = false;
					break;
				}
			}
			if (satisfied) {
				candidates.push_back(id);
				weights.push_back(organizationFactory.GetPower(id));
			}
		}
		if (candidates.empty()) break;

		// 加权CDF随机选一个候选类型。
		float total = 0.f;
		for (float w : weights) total += w;
		int chosenIdx = 0;
		if (total > 0.f) {
			float roll = (GetRandom(10000) / 10000.f) * total;
			float acc = 0.f;
			for (size_t i = 0; i < weights.size(); i++) {
				acc += weights[i];
				if (roll <= acc) {
					chosenIdx = static_cast<int>(i);
					break;
				}
			}
		}
		const string& chosenId = candidates[chosenIdx];
		const auto& requirements = requirementsByType[chosenId];

		vector<Component*> claimed;
		for (const auto& [type, range] : requirements) {
			auto& pool = componentsByType[type];
			int count = range.first + GetRandom(range.second - range.first + 1);
			count = min(count, static_cast<int>(pool.size()));
			for (int i = 0; i < count; i++) {
				claimed.push_back(pool.back());
				pool.pop_back();
			}
		}

		organizations.push_back(new Organization(&organizationFactory, &jobFactory, &scriptFactory,
			chosenId, move(claimed)));
	}
}

void Society::RecruitCitizens(const vector<Citizen*>& citizens, int currentYear) {
	vector<Job*> vacancies;
	for (Organization* organization : organizations) {
		for (Job* job : organization->GetJobs()) {
			if (job && !job->GetOccupant()) vacancies.push_back(job);
		}
	}

	vector<Citizen*> adults;
	for (Citizen* citizen : citizens) {
		if (citizen && !citizen->GetJob() && citizen->GetAge(currentYear) >= kMinAdultAge) {
			adults.push_back(citizen);
		}
	}

	// Fisher-Yates洗牌，用项目统一的GetRandom(n)（不引入std::shuffle额外的<random>引擎）。
	for (size_t i = vacancies.size(); i > 1; i--) {
		size_t j = static_cast<size_t>(GetRandom(static_cast<int>(i)));
		swap(vacancies[i - 1], vacancies[j]);
	}
	for (size_t i = adults.size(); i > 1; i--) {
		size_t j = static_cast<size_t>(GetRandom(static_cast<int>(i)));
		swap(adults[i - 1], adults[j]);
	}

	size_t n = min(vacancies.size(), adults.size());
	for (size_t i = 0; i < n; i++) {
		vacancies[i]->SetOccupant(adults[i]);
		adults[i]->SetJob(vacancies[i]);
	}
}

void Society::Tick(const Time& currentTime, bool crossedDay,
	const function<void(Organization*, const vector<Change*>&)>& onActions, PostHandle* post) {
	if (crossedDay) {
		for (Organization* organization : organizations) {
			organization->DailyPlan(currentTime, post);
			for (const auto& [node, time] : organization->GetPlans()) {
				organizationTimerSet.insert({ time, organization, node });
			}
		}
	}

	int count = 0;
	while (count < kMaxOrganizationTimersPerTick && !organizationTimerSet.empty()) {
		auto it = organizationTimerSet.begin();
		const auto& [target, organization, node] = *it;
		if (currentTime < target) break;
		vector<Change*> changes = organization->ExecNode(node, post);
		onActions(organization, changes);
		organizationTimerSet.erase(it);
		count++;
	}
}

void Society::ApplyChange(const Change* change, const ScriptContext& context) {
	// 占位，等Society域真的有需要处理的Change子类时再补，见society.h声明处注释。
}

const vector<Organization*>& Society::GetOrganizations() const { return organizations; }
