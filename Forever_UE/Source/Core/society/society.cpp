#include "society/society.h"

#include "society/organization.h"
#include "society/job.h"
#include "story/script.h"
#include "map/component.h"
#include "map/room.h"
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

double Society::GovernmentNetRatio() const {
	int64_t injection = ledger.GetInitialInjection();
	if (injection <= 0) return 0.0;
	return static_cast<double>(treasury.NetPosition(ledger)) / static_cast<double>(injection);
}

Party Society::OwnerOf(const Room* room) const {
	if (roomOwnersDirty) {
		roomOwners.clear();
		for (Organization* organization : organizations) {
			for (Component* component : organization->GetComponents()) {
				if (!component) continue;
				for (Room* owned : component->GetRooms()) roomOwners[owned] = organization;
			}
		}
		roomOwnersDirty = false;
	}
	if (!room) return Party::Government();
	auto it = roomOwners.find(room);
	return it != roomOwners.end() ? Party::Of(it->second) : Party::Government();
}

void Society::AssignShareholders(const vector<Citizen*>& citizens, const Time& now) {
	vector<Citizen*> adults;
	for (Citizen* citizen : citizens) {
		if (citizen && citizen->IsInEconomy() && citizen->GetAge(now) >= kMinAdultAge) adults.push_back(citizen);
	}

	for (Organization* organization : organizations) {
		OrganizationMod* mod = organization->GetMod();
		if (!mod || mod->shareholders.empty() || adults.empty()) continue;

		float sum = 0.f;
		for (float share : mod->shareholders) sum += share;
		if (sum <= 0.f) continue;

		// 不放回随机抽样：成年人不够时只选得出多少算多少。
		vector<Citizen*> pool = adults;
		vector<Citizen*> picked;
		vector<float> pickedShare;
		for (float share : mod->shareholders) {
			if (pool.empty()) break;
			int index = GetRandom(static_cast<int>(pool.size()));
			picked.push_back(pool[index]);
			pickedShare.push_back(share);
			pool[index] = pool.back();
			pool.pop_back();
		}
		organization->SetShareholders(Organization::SplitShares(picked, pickedShare));
	}
}

void Society::Init(const vector<Component*>& components) {
	roomOwnersDirty = true;
	// 第一步：直接读各已注册组织类型的static组件需求(不构造mod实例)，只读引用。
	unordered_map<string, const unordered_map<string, pair<int, int>>*> requirementsByType;
	for (const string& id : organizationFactory.GetRegisteredIds()) {
		requirementsByType[id] = &organizationFactory.GetRequirements(id);
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
		for (const auto& [id, requirementsPtr] : requirementsByType) {
			const auto& requirements = *requirementsPtr;
			// 没有Component需求的类型永远"满足"又不消耗Component，会让循环死循环，跳过。
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
		const auto& requirements = *requirementsByType[chosenId];

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

void Society::RecruitCitizens(const vector<Citizen*>& citizens, const Time& now) {
	MatchUnemployed(citizens, now);
}

int Society::MatchUnemployed(const vector<Citizen*>& citizens, const Time& now) {
	vector<Job*> vacancies;
	for (Organization* organization : organizations) {
		for (Job* job : organization->GetJobs()) {
			if (job && job->IsOpen() && !job->GetOccupant()) vacancies.push_back(job);
		}
	}
	if (vacancies.empty()) return 0;

	// 只招成年人：未成年人和老年人（>=退休年龄）不工作。
	vector<Citizen*> adults;
	for (Citizen* citizen : citizens) {
		if (!citizen || !citizen->IsActive() || !citizen->IsInEconomy() || citizen->GetJob()) continue;
		int age = citizen->GetAge(now);
		if (age >= kMinAdultAge && age < policy.retirementAge) adults.push_back(citizen);
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
	return static_cast<int>(n);
}

Job* Society::FindJobByScript(const Container* script) const {
	if (!script) return nullptr;
	for (Organization* organization : organizations) {
		for (Job* job : organization->GetJobs()) {
			if (job && static_cast<const Container*>(job->GetScript()) == script) return job;
		}
	}
	return nullptr;
}

Organization* Society::FindOrganizationOfJob(const Job* job) const {
	if (!job) return nullptr;
	for (Organization* organization : organizations) {
		for (const Job* candidate : organization->GetJobs()) {
			if (candidate == job) return organization;
		}
	}
	return nullptr;
}

bool Society::RecruitCitizen(Citizen* citizen, const Time& now) {
	if (!citizen || citizen->GetJob()) return false;
	int age = citizen->GetAge(now);
	if (age < kMinAdultAge || age >= policy.retirementAge) return false;

	vector<Job*> vacancies;
	for (Organization* organization : organizations) {
		for (Job* job : organization->GetJobs()) {
			if (job && job->IsOpen() && !job->GetOccupant()) vacancies.push_back(job);
		}
	}
	if (vacancies.empty()) return false;

	Job* job = vacancies[GetRandom(static_cast<int>(vacancies.size()))];
	job->SetOccupant(citizen);
	citizen->SetJob(job);
	return true;
}

bool Society::HasDueTimers(const Time& currentTime) const {
	return !organizationTimerSet.empty() && std::get<0>(*organizationTimerSet.begin()) <= currentTime;
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

bool Society::ApplyChange(const Change* change, const ScriptContext& context) {
	// 占位：Society域目前没有需要处理的Change子类。
	return false;
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
