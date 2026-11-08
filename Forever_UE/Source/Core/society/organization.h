#pragma once

#include "class.h"

#include "society/organization_mod.h"
#include "society/organization_factory.h"
#include "society/job_factory.h"

#include "common/utility.h"

#include <climits>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>


// Organization：一份具体组织的实体（比如一家商店），独占持有一个OrganizationMod实例+一份Script，
// 占用若干Component，构造时按workspace room设计并new出Job，见organization.md。
class Organization {
	// 存档：SocietySaveCodec直接读写私有字段(society_save.md)。
	friend class SocietySaveCodec;
public:
	Organization(OrganizationFactory* factory, JobFactory* jobFactory, ScriptFactory* scriptFactory,
		const std::string& id, std::vector<Component*> components);
	~Organization(); // for (Job* j : jobs) delete j; factory->DestroyOrganization(mod); delete script;

	// 每个房间岗位计划的上限是房间容量的kMaxOvercrowd倍（容量=满编，超编会提高工坊产能）。
	static constexpr int kMaxOvercrowd = 2;
	// lastSettledBalance还没有记录（开局第一次月结）时的标记。
	static constexpr int64_t kUnsettled = INT64_MIN;

	std::string GetType() const;
	const std::vector<Component*>& GetComponents() const;
	const std::vector<Job*>& GetJobs() const;
	// 初始发行股数：每个组织开局（以及倒闭重启后）发行kTotalShares股，增发会抬高GetTotalShares()。
	static constexpr int64_t kTotalShares = 10000;
	// 当前总股数：市民持股数之和不超过它，余下的是组织自己的库存股。随社会段存档。
	int64_t GetTotalShares() const;
	void SetTotalShares(int64_t value);
	// 股东：持有的股数。开局由Society::AssignShareholders按mod占股比例随机选成年市民担任，之后随市场成交变化。
	struct Shareholder {
		Citizen* citizen = nullptr;
		int64_t shares = 0;
	};
	const std::vector<Shareholder>& GetShareholders() const;
	void SetShareholders(const std::vector<Shareholder>& value);
	// 按权重把kTotalShares分给这些市民（人数不够时权重重新归一化），最后一位补齐余数，总和恰为kTotalShares。
	static std::vector<Shareholder> SplitShares(const std::vector<Citizen*>& citizens, const std::vector<float>& weights);
	int64_t SharesOf(const Citizen* citizen) const;
	// 市民持股总数的差额(可正可负)，归零时移出股东表；不检查总量，调用方（Market）保证不超发。
	void AdjustShares(Citizen* citizen, int64_t delta);
	// 库存股：当前总股数减去全部市民持股。
	int64_t TreasuryShares() const;

	// 岗位管理。mod->vacancies是岗位计划（房间名+岗位类型，同一个(房间,类型)出现几次就计划几个），
	// Job对象按它创建/重开/关闭，Job只增不删。
	struct JobChange {
		int opened = 0;    // 新开或重开的岗位数
		int closed = 0;    // 关闭的岗位数
		int dismissed = 0; // 其中被裁掉的在岗人数
	};
	std::vector<Room*> WorkspaceRooms() const; // 已认领Component里的全部工位房间
	void DesignJobs(bool reset);               // 对每个工位房间调mod->DesignJobsForRoom，reset只对第一个房间传
	JobChange ReconcileJobs();                 // 按mod->vacancies对账：计划多了开岗，少了关岗（先关空缺，再随机裁在岗的）
	int LayOffAll();                           // 倒闭：全部岗位关闭、在岗者变失业，返回被裁人数
	JobChange ReopenInitial();                 // 重启：重新设计满编岗位计划（reset）并对账
	int EmployeeCount() const;                 // 开放岗位上的在岗人数
	int OpenPositionCount() const;             // 开放岗位数（含空缺）
	// 上次月结完成后的账户余额，当月利润=月结时余额-它。状态，随社会段存档。
	int64_t GetLastSettledBalance() const;
	void SetLastSettledBalance(int64_t value);

	// 倒闭：资金为负的企业停业——不发工资、不提供服务、不付房租、仓库和工坊停止物流与生产，员工全部裁掉（LayOffAll），
	// 重启后重新招聘。股东凑齐初始资金后重新营业。状态，随社会段存档。
	bool IsBankrupt() const;
	void SetBankrupt(bool value);

	Script* GetScript() const;
	OrganizationMod* GetMod() const; // 经济参数/服务收集都从mod读

	void DailyPlan(const Time& currentTime, PostHandle* post); // 转调mod->DailyPlan(...)
	const std::unordered_map<std::string, Time>& GetPlans() const;

	// 和Job::ExecNode同一个"只读mod->changes、不delete"写法。
	std::vector<Change*> ExecNode(const std::string& node, PostHandle* post);

private:
	OrganizationFactory* factory;
	JobFactory* jobFactory;
	ScriptFactory* scriptFactory;
	OrganizationMod* mod;
	Script* script;
	std::vector<Component*> components; // 不持有
	std::vector<Job*> jobs;             // 持有所有权，Core侧new
	std::vector<Shareholder> shareholders;
	int64_t totalShares = kTotalShares;
	int64_t lastSettledBalance = kUnsettled;
	bool bankrupt = false;
};
