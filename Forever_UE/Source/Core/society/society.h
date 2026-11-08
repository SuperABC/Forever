#pragma once

#include "class.h"

#include "society/organization_factory.h"
#include "society/job_factory.h"
#include "story/script_factory.h"

#include "society/ledger.h"
#include "society/policy.h"
#include "society/treasury.h"
#include "society/market.h"

#include "common/utility.h"
#include "common/handle.h"

#include <functional>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>


// Society：Job+Organization域的聚合入口，同时持有账本和财政参数。见society.md。
class Society {
	// 存档：SocietySaveCodec直接读写私有字段(society_save.md)。
	friend class SocietySaveCodec;
public:
	Society(); // 绑定Registry::Get()对应的三个Factory引用成员，mod注册不在这里做
	~Society(); // delete organizations

	// 阶段1：按地图里所有Component加权随机分配Organization，算法见organization.md。
	void Init(const std::vector<Component*>& components);

	// 新开局为每个组织选股东：mod的shareholders占股比例归一化后，随机选对应数量个不同的成年市民。
	void AssignShareholders(const std::vector<Citizen*>& citizens, int currentYear);

	// 阶段2：把成年且未到退休年龄、还没有job的市民随机匹配到空缺的Job上。
	void RecruitCitizens(const std::vector<Citizen*>& citizens, int currentYear);
	// 再就业：和RecruitCitizens同一套随机配对，只招在场的人、只填开放的岗位；每天调一次，
	// 让被裁员/企业倒闭的人和扩招/重启空出的岗位尽快对上。返回新就业人数。
	int MatchUnemployed(const std::vector<Citizen*>& citizens, int currentYear);

	// 给一个开局之后才接入经济系统的市民(spawn_npc economy:true)单独求职：已有工作/不在工作年龄/没有
	// 空缺岗位都返回false，否则随机挑一个空缺岗位安排上并返回true。
	bool RecruitCitizen(Citizen* citizen, int currentYear);

	// 按Job脚本反查岗位及其所属组织(脚本里的change以context.self为Job脚本时用，如pay_wage/open_shop)。
	// 找不到返回nullptr。
	Job* FindJobByScript(const Container* script) const;
	Organization* FindOrganizationOfJob(const Job* job) const;

	// 阶段3：RecruitCitizens()之后调用，反推入职历史(JobExperience)并派生同事关系，见society.md。
	void GenerateEmploymentHistory(const std::vector<Citizen*>& citizens, int currentYear);

	// 每帧：Organization自己的DailyPlan/ExecNode调度(和Populace::Tick驱动Job那一份独立的timer)。
	// @post透传给Organization::DailyPlan/ExecNode。
	void Tick(const Time& currentTime, bool crossedDay,
		const std::function<void(Organization*, const std::vector<Change*>&)>& onActions, PostHandle* post);

	// 占位：Society域目前没有自己认识的Change子类，恒返回false。
	bool ApplyChange(const Change* change, const ScriptContext& context);

	const std::vector<Organization*>& GetOrganizations() const;

	// 账本与财政参数，见ledger.h/policy.h。
	Ledger& GetLedger() { return ledger; }
	const Ledger& GetLedger() const { return ledger; }
	const GovernmentPolicy& GetPolicy() const { return policy; }
	GovernmentPolicy& GetPolicy() { return policy; } // 负反馈系数由Economy每天写入
	// 政府的存款/国债负债簿和股市，见treasury.h/market.h。
	Treasury& GetTreasury() { return treasury; }
	const Treasury& GetTreasury() const { return treasury; }
	Market& GetMarket() { return market; }
	const Market& GetMarket() const { return market; }
	// 政府净头寸与开局注资之比，符号和净头寸相同；没记过开局注资时返回0。负反馈用它，借款不会抬高它，印钞会。
	double GovernmentNetRatio() const;
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
	Treasury treasury;
	Market market;
	int lastMonthKey = 0;

	// 房间 -> 占用它的组织，按organizations惰性重建（Init/读档后置脏）。
	mutable std::unordered_map<const Room*, Organization*> roomOwners;
	mutable bool roomOwnersDirty = true;

	std::set<std::tuple<Time, Organization*, std::string>> organizationTimerSet;
	static constexpr int kMaxOrganizationTimersPerTick = 16;
};
