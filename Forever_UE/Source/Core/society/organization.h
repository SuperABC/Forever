#pragma once

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
class ScriptFactory;

// Organization：一份具体组织的实体（比如一家商店），独占持有一个OrganizationMod实例，
// 可以占多个Component（也可以要多种不同类型的Component，具体由mod->requirements决定，
// 见organization_mod.h）。构造时自己遍历claimed components的每个workspace room、调
// mod->DesignJobsForRoom(...)拿到要配的job类型字符串列表，逐条new Job(...)——Job的
// 分配/释放全程在Core侧（OrganizationMod所在的Dependence层看不到Job这个Core类型），
// 见organization.md"分层约束"一节。
class Organization {
public:
	Organization(OrganizationFactory* factory, JobFactory* jobFactory, ScriptFactory* scriptFactory,
		const std::string& id, std::vector<Component*> components);
	~Organization(); // for (Job* j : jobs) delete j; factory->DestroyOrganization(mod);

	std::string GetType() const;
	const std::vector<Component*>& GetComponents() const;
	const std::vector<Job*>& GetJobs() const;

	void DailyPlan(const Time& currentTime); // 转调mod->DailyPlan()
	const std::unordered_map<std::string, Time>& GetPlans() const;

	// 和Job::ExecNode同一个"读mod->GetChanges()、不delete"写法，纯C++通道。
	std::vector<Change*> ExecNode(const std::string& node);

private:
	OrganizationFactory* factory;
	OrganizationMod* mod;
	std::vector<Component*> components; // 不持有
	std::vector<Job*> jobs;             // 持有所有权，Core侧new
};
