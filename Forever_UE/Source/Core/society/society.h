#pragma once

#include "society/organization_factory.h"
#include "society/job_factory.h"
#include "story/script_factory.h"

#include "society/ledger.h"
#include "society/policy.h"

#include "common/utility.h"
#include "common/handle.h"

#include <functional>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

class Component;
class Room;
class Citizen;
class Organization;
class Change;
struct ScriptContext;

// Society：Job+Organization域的聚合入口（不引入Calendar——"哪几天上班/几点上下班"
// 直接写进具体JobMod子类的DailyPlan里）。见society.md。
class Society {
	// 存档：SocietySaveCodec直接读写私有字段(society_save.md)。
	friend class SocietySaveCodec;
public:
	Society(); // 绑定Registry::Get()对应的三个Factory引用成员，mod注册不在这里做
	~Society(); // delete organizations

	// 阶段1：按地图里所有Component加权随机分配Organization，见society.md"组织分配"
	// 一节的算法。components来自AForeverFrameworkActor::EnsureSocietyGenerated()
	// 调map->GetAllComponents()拍平的结果——Society不知道Map/Building的存在。
	void Init(const std::vector<Component*>& components);

	// 阶段2：把成年(GetAge(currentYear)>=18)且还没有job的市民随机匹配到还空缺的Job上。
	// 新开局为每个组织选股东：mod的shareholders占股比例归一化后，随机选对应数量个不同的成年市民。
	void AssignShareholders(const std::vector<Citizen*>& citizens, int currentYear);

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

	// 阶段占位：目前没有任何Change子类是Society域自己认识、需要处理的，恒返回false。
	// 返回这个Change是不是本域认识的类型(认识就返回true，不管目标对象找没找到——找不到目标静默
	// 忽略是既有的宽松约定)。AForeverFrameworkActor::ApplyChange汇总七个域的结果，全都不认识时
	// 才打"未实现"警告。
	bool ApplyChange(const Change* change, const ScriptContext& context);

	const std::vector<Organization*>& GetOrganizations() const;

	// 账本与财政参数，见ledger.h/policy.h。
	Ledger& GetLedger() { return ledger; }
	const Ledger& GetLedger() const { return ledger; }
	const GovernmentPolicy& GetPolicy() const { return policy; }
	GovernmentPolicy& GetPolicy() { return policy; } // 负反馈系数由Economy每天写入
	// 上一次月结（分红）所处的月份（年*12+月），0=还没有开始计月。状态，随社会段存档。
	int GetLastMonthKey() const { return lastMonthKey; }
	void SetLastMonthKey(int value) { lastMonthKey = value; }

	// 占用这个房间的组织（房间所在Component被哪个组织claim）；没有组织占用（或room为空）归政府。
	Party OwnerOf(const Room* room) const;

private:
	OrganizationFactory& organizationFactory;
	JobFactory& jobFactory;
	ScriptFactory& scriptFactory;

	std::vector<Organization*> organizations; // 持有所有权

	Ledger ledger;
	GovernmentPolicy policy;
	int lastMonthKey = 0;

	// 房间 -> 占用它的组织，按organizations惰性重建（Init/读档后置脏）。
	mutable std::unordered_map<const Room*, Organization*> roomOwners;
	mutable bool roomOwnersDirty = true;

	std::set<std::tuple<Time, Organization*, std::string>> organizationTimerSet;
	static constexpr int kMaxOrganizationTimersPerTick = 4;
};
