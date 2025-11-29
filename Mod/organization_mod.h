#pragma once

#include "organization_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModOrganization : public Organization {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetType() const override { return "mod"; }
    virtual std::string GetName() const override { return "模组组织"; }

    virtual std::vector<std::pair<std::string, int>> ComponentRequirements() const {
        return std::vector<std::pair<std::string, int>>();
    }
};

