#pragma once

#include "society/organization_factory.h"
#include "society/job_factory.h"
#include "story/script_factory.h"

#include "common/utility.h"

#include <functional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

class Component;
class Citizen;
class Organization;
class Change;

// Society：Job+Organization域的聚合入口（这次不迁移Calendar——"哪几天上班/几点上下班"
// 直接写进具体JobMod子类的DailyPlan里）。见society.md。
class Society {
public:
	Society(); // 绑定Registry::Get()对应的三个Factory引用成员，mod注册不在这里做
	~Society(); // delete organizations

	// 阶段1：按地图里所有Component加权随机分配Organization，见society.md"组织分配"
	// 一节的算法。components来自AForeverFrameworkActor::EnsureSocietyGenerated()
	// 调map->GetAllComponents()拍平的结果——Society不知道Map/Building的存在。
	void Init(const std::vector<Component*>& components);

	// 阶段2：把成年(GetAge(currentYear)>=18)且还没有job的市民随机匹配到还空缺的Job上。
	void RecruitCitizens(const std::vector<Citizen*>& citizens, int currentYear);

	// 每帧：Organization自己的DailyPlan/ExecNode调度(和Populace::Tick驱动Job那一份
	// 完全独立的timer)。ExecNode是纯C++通道，直接给Change*。
	void Tick(const Time& currentTime, bool crossedDay,
		const std::function<void(Organization*, const std::vector<Change*>&)>& onActions);

	const std::vector<Organization*>& GetOrganizations() const;

private:
	OrganizationFactory& organizationFactory;
	JobFactory& jobFactory;
	ScriptFactory& scriptFactory;

	std::vector<Organization*> organizations; // 持有所有权

	std::set<std::tuple<Time, Organization*, std::string>> organizationTimerSet;
	static constexpr int kMaxOrganizationTimersPerTick = 4;
};
