#pragma once

#include "class.h"

#include "society/organization_mod.h"
#include "society/organization_factory.h"
#include "society/job_factory.h"

#include "common/utility.h"

#include <string>
#include <unordered_map>
#include <vector>

class Component;
class Job;
class Change;
class Script;
class ScriptFactory;

// Organization：一份具体组织的实体（比如一家商店），独占持有一个OrganizationMod实例+
// 一份自己的Script（构造时按mod->scriptModName创建、按mod->milestoneNames加载
// milestone，和Job同一套机制，见job_mod.h/organization_mod.h"Script配置"一节），
// 可以占多个Component（也可以要多种不同类型的Component，具体由mod的static GetRequirements()决定，
// 见organization_mod.h）。构造时自己遍历claimed components的每个workspace room、调
// mod->DesignJobsForRoom(...)拿到要配的job类型字符串列表，逐条new Job(...)——Job的
// 分配/释放全程在Core侧（OrganizationMod所在的Dependence层看不到Job这个Core类型），
// 见organization.md"分层约束"一节。
class Organization {
	// 存档：SocietySaveCodec直接读写私有字段(society_save.md)。
	friend class SocietySaveCodec;
public:
	Organization(OrganizationFactory* factory, JobFactory* jobFactory, ScriptFactory* scriptFactory,
		const std::string& id, std::vector<Component*> components);
	~Organization(); // for (Job* j : jobs) delete j; factory->DestroyOrganization(mod); delete script;

	std::string GetType() const;
	const std::vector<Component*>& GetComponents() const;
	const std::vector<Job*>& GetJobs() const;
	// 股东：占股比例已归一化（和为1），由Society::AssignShareholders随机选成年市民担任。
	struct Shareholder {
		Citizen* citizen = nullptr;
		float ratio = 0.f;
	};
	const std::vector<Shareholder>& GetShareholders() const;
	void SetShareholders(const std::vector<Shareholder>& value);

	// 倒闭：资金为负的企业停业——不发工资、不提供服务、不付房租、仓库和工坊停止物流与生产，员工不离职。
	// 股东凑齐初始资金后重新营业。状态，随社会段存档。
	bool IsBankrupt() const;
	void SetBankrupt(bool value);

	Script* GetScript() const;
	OrganizationMod* GetMod() const; // 经济参数/服务收集都从mod读

	void DailyPlan(const Time& currentTime, PostHandle* post); // 转调mod->DailyPlan(...)
	const std::unordered_map<std::string, Time>& GetPlans() const;

	// 和Job::ExecNode同一个"读mod->GetChanges()、不delete"写法，纯C++通道。@post透传给
	// mod，见job.h的同名参数说明。
	std::vector<Change*> ExecNode(const std::string& node, PostHandle* post);

private:
	OrganizationFactory* factory;
	OrganizationMod* mod;
	Script* script;
	std::vector<Component*> components; // 不持有
	std::vector<Job*> jobs;             // 持有所有权，Core侧new
	std::vector<Shareholder> shareholders;
	bool bankrupt = false;
};
