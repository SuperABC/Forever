#include "society/society.h"

#include "society/organization.h"
#include "society/job.h"
#include "map/component.h"
#include "populace/citizen.h"
#include "populace/experience.h"
#include "common/registry.h"

#include <algorithm>
#include <unordered_map>

using namespace std;

namespace {
	constexpr int kMinAdultAge = 18;

	// 同事关系Relation随机初始化——familiarity/trust中等，competing比同学/亲属更高一些，
	// 体现职场竞争，数值量级参考，实现后可再调。
	void RandomizeColleagueRelation(Citizen* citizen, const string& otherName) {
		citizen->SetAcquaintanceValue(otherName, RELATION_FAMILIARITY, clamp(GetRandomNormal(0.5f, 0.2f), 0.0f, 1.0f));
		citizen->SetAcquaintanceValue(otherName, RELATION_RESPECT, clamp(GetRandomNormal(0.5f, 0.2f), 0.0f, 1.0f));
		citizen->SetAcquaintanceValue(otherName, RELATION_FAVOUR, clamp(GetRandomNormal(0.45f, 0.2f), 0.0f, 1.0f));
		citizen->SetAcquaintanceValue(otherName, RELATION_TRUST, clamp(GetRandomNormal(0.5f, 0.2f), 0.0f, 1.0f));
		citizen->SetAcquaintanceValue(otherName, RELATION_COMPETING, clamp(GetRandomNormal(0.5f, 0.2f), 0.0f, 1.0f));
		citizen->SetAcquaintanceValue(otherName, RELATION_RELIABILITY, clamp(GetRandomNormal(0.5f, 0.2f), 0.0f, 1.0f));
	}
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

void Society::GenerateEmploymentHistory(const vector<Citizen*>& citizens, int currentYear) {
	constexpr int kHireEventChancePercent = 15; // 对应平均约6-7年一任
	constexpr int kColleagueChancePercent = 40;

	// 空闲成年人池：没有当前工作、年满18岁的citizen，随着链条往回推逐步摘取。
	vector<Citizen*> idlePool;
	for (Citizen* citizen : citizens) {
		if (citizen && !citizen->GetJob() && citizen->GetAge(currentYear) >= kMinAdultAge) {
			idlePool.push_back(citizen);
		}
	}

	auto EarliestHire = [](Citizen* citizen) {
		int graduation = citizen->GetLastGraduationYear();
		int adultYear = citizen->GetBirthYear() + kMinAdultAge;
		return graduation >= 0 ? max(adultYear, graduation) : adultYear;
	};

	// 每个当前有人在职的Job维护一条"待定链条"：链条头是目前认为占着这个职位的citizen，
	// knownSinceYear是"这个人干到哪一年"（-1表示至今仍在干，即当前在职者本人）。
	struct PendingChain {
		Organization* org;
		Citizen* occupant;
		int knownSinceYear;
	};
	vector<PendingChain> pending;
	int floorYear = currentYear;
	for (Organization* organization : organizations) {
		for (Job* job : organization->GetJobs()) {
			if (!job || !job->GetOccupant()) continue;
			Citizen* occupant = job->GetOccupant();
			pending.push_back({ organization, occupant, -1 });
			floorYear = min(floorYear, EarliestHire(occupant));
		}
	}

	// 逐年往回推，每年对每条链条做一次概率判定："这位是不是恰好这一年入职的"——命中就
	// 写一条JobExperience、从空闲成年人池里挑一个前任接上链条继续往回推；没命中就把年份
	// 继续往前推一年、下次再判定同一个人。链条终止于"推到硬下限"或"空闲成年人池耗尽"。
	for (int year = currentYear; !pending.empty() && year >= floorYear; year--) {
		for (size_t i = 0; i < pending.size(); ) {
			PendingChain& chain = pending[i];
			int earliestHire = EarliestHire(chain.occupant);
			bool finalize = (year <= earliestHire) || (GetRandom(100) < kHireEventChancePercent);

			if (!finalize) { i++; continue; }

			int hireYear = max(year, earliestHire);
			chain.occupant->AddExperience(new JobExperience(chain.org, hireYear, chain.knownSinceYear));

			if (hireYear <= earliestHire) {
				pending.erase(pending.begin() + i);
				continue;
			}

			// 从idlePool挑一个前任：要求在接任前一年就已年满18岁。
			int candidateIdx = -1;
			for (int attempt = 0; attempt < 10 && !idlePool.empty(); attempt++) {
				int idx = GetRandom(static_cast<int>(idlePool.size()));
				if (idlePool[idx]->GetAge(hireYear - 1) >= kMinAdultAge) { candidateIdx = idx; break; }
			}

			if (candidateIdx >= 0) {
				Citizen* predecessor = idlePool[candidateIdx];
				idlePool.erase(idlePool.begin() + candidateIdx);
				chain.occupant = predecessor;
				chain.knownSinceYear = hireYear;
				i++;
			}
			else {
				pending.erase(pending.begin() + i);
			}
		}
	}
	// 循环结束后pending里剩下的（因为floorYear下限，理论上不会剩太多）：兜底按各自当前
	// occupant的earliestHire收尾。
	for (PendingChain& chain : pending) {
		int earliestHire = EarliestHire(chain.occupant);
		chain.occupant->AddExperience(new JobExperience(chain.org, earliestHire, chain.knownSinceYear));
	}

	// 同事关系——从上面"一"的JobExperience派生出"多"：当前在职者两两随机互认，不是全组织
	// 都互相认识。
	for (Organization* organization : organizations) {
		vector<Citizen*> occupants;
		for (Job* job : organization->GetJobs()) {
			if (job && job->GetOccupant()) occupants.push_back(job->GetOccupant());
		}
		for (size_t i = 0; i < occupants.size(); i++) {
			for (size_t j = i + 1; j < occupants.size(); j++) {
				if (GetRandom(100) >= kColleagueChancePercent) continue;
				occupants[i]->AddAcquaintance(occupants[j]->GetName(), RELATIONSHIP_COLLEAGUE);
				RandomizeColleagueRelation(occupants[i], occupants[j]->GetName());
				occupants[j]->AddAcquaintance(occupants[i]->GetName(), RELATIONSHIP_COLLEAGUE);
				RandomizeColleagueRelation(occupants[j], occupants[i]->GetName());
			}
		}
	}
}
