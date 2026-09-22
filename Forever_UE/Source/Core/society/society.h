#pragma once

#include "society/organization_factory.h"
#include "society/job_factory.h"
#include "story/script_factory.h"

#include "common/utility.h"
#include "common/handle.h"

#include <functional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

class Component;
class Citizen;
class Organization;
class Change;
struct ScriptContext;

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

	// 阶段3：RecruitCitizens()之后调用——(1)给每个当前在职者逐年往回推、递归到底地反推
	// 入职历史(JobExperience，一对多里"一"的那一份，见experience.h)，从空闲成年人池里
	// 动态摘取前任，直到推到"满18岁/最后一次毕业年份"这个硬下限或空闲成年人池耗尽为止；
	// (2)从这份"一"派生出同事acquaintances(同组织在职者随机抽一部分互相认识)，见
	// society.md"四类人际关系生成"一节。
	void GenerateEmploymentHistory(const std::vector<Citizen*>& citizens, int currentYear);

	// 每帧：Organization自己的DailyPlan/ExecNode调度(和Populace::Tick驱动Job那一份
	// 完全独立的timer)。ExecNode是纯C++通道，直接给Change*。@post透传给
	// Organization::DailyPlan/ExecNode，见populace.h同名参数的说明。
	void Tick(const Time& currentTime, bool crossedDay,
		const std::function<void(Organization*, const std::vector<Change*>&)>& onActions, PostHandle* post);

	// 阶段占位：目前没有任何Change子类是Society域自己认识、需要处理的，空实现——
	// AForeverFrameworkActor::ApplyChange会把同一个Change转发给全部六个域，这里不打"未实现"
	// 警告（避免同一个Change被六个域各打一遍重复警告），唯一的兜底警告在Story::ApplyChange。
	void ApplyChange(const Change* change, const ScriptContext& context);

	const std::vector<Organization*>& GetOrganizations() const;

private:
	OrganizationFactory& organizationFactory;
	JobFactory& jobFactory;
	ScriptFactory& scriptFactory;

	std::vector<Organization*> organizations; // 持有所有权

	std::set<std::tuple<Time, Organization*, std::string>> organizationTimerSet;
	static constexpr int kMaxOrganizationTimersPerTick = 4;
};
