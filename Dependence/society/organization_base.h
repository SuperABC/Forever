#pragma once

#include "../map/component_base.h"
#include "../populace/job_base.h"
#include "../society/calendar_base.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>

#undef AddJob


class Organization {
public:
    Organization() = default;
    virtual ~Organization() = default;

	// 子类实现方法

    // 动态返回组织静态信息
    static std::string GetId();
    virtual std::string GetType() const = 0;
    virtual std::string GetName() const = 0;

    // 组织权重
    static float GetPower();

	// 所需组合及数量范围
	virtual std::vector<std::pair<std::string, std::pair<int, int>>> ComponentRequirements() const = 0;

	// 根据实际组合安排工作岗位
    virtual std::vector<std::pair<std::string, std::vector<std::vector<std::string>>>> ArrageVacancies(
        std::vector<std::pair<std::string, int>> components) const = 0;

    // 设定工作日历
    virtual void SetCalendar(CalendarFactory* factory) = 0;

    // 员工入职
    virtual std::vector<std::shared_ptr<Job>> EnrollEmployee(std::vector<int> ids);

	// 父类实现方法

	// 管理职位
    std::vector<std::pair<std::shared_ptr<Component>, std::vector<std::pair<std::shared_ptr<Job>, int>>>> GetJobs() const;
	void AddVacancy(std::shared_ptr<Component> component, std::vector<std::shared_ptr<Job>> vacancies);

protected:
	std::vector<std::pair<std::shared_ptr<Component>, std::vector<std::pair<std::shared_ptr<Job>, int>>>> jobs;
};

class OrganizationFactory {
public:
    void RegisterOrganization(const std::string& id, std::function<std::unique_ptr<Organization>()> creator, float power);
    std::unique_ptr<Organization> CreateOrganization(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);
    const std::unordered_map<std::string, float>& GetPowers() const;

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Organization>()>> registries;
    std::unordered_map<std::string, bool> configs;
    std::unordered_map<std::string, float> powers;
};