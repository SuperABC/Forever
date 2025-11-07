#pragma once

#include "organization_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModOrganization : public Organization {
public:
    std::string GetName() const override { return "模组组织"; }
};

