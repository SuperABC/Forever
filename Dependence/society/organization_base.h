#pragma once

#include <string>
#include <functional>
#include <memory>
#include <unordered_map>


class Organization {
public:
    Organization() = default;
    virtual ~Organization() = default;

    // 动态返回园区名称
    virtual std::string GetName() const = 0;

private:
};

class OrganizationFactory {
public:
    void RegisterOrganization(const std::string& id, std::function<std::unique_ptr<Organization>()> creator);
    std::unique_ptr<Organization> CreateOrganization(const std::string& id);
    bool CheckRegistered(const std::string& id);

private:
    std::unordered_map<std::string, std::function<std::unique_ptr<Organization>()>> registry;
};