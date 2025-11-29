#pragma once

#include "../map/component_base.h"
#include "../populace/job_base.h"

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Organization {
public:
    Organization() = default;
    virtual ~Organization() = default;

	// 子类实现方法

    // 动态返回组织静态信息
    static std::string GetId();
    virtual std::string GetType() const = 0;
    virtual std::string GetName() const = 0;

	// 所需组合及数量
	virtual std::vector<std::pair<std::string, int>> ComponentRequirements() const = 0;

	// 父类实现方法

protected:
	std::vector<std::shared_ptr<Component>> components;
    std::vector<std::shared_ptr<Job>> jobs;
};

class OrganizationFactory {
public:
    void RegisterOrganization(const std::string& id, std::function<std::unique_ptr<Organization>()> creator);
    std::unique_ptr<Organization> CreateOrganization(const std::string& id);
    bool CheckRegistered(const std::string& id);
    void SetConfig(std::string name, bool config);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Organization>()>> registries;
    std::unordered_map<std::string, bool> configs;
};