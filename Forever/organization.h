#pragma once

#include "organization_base.h"


// 子类注册函数
typedef void (*RegisterModOrganizationsFunc)(OrganizationFactory* factory);

// 主程序检测子类
class TestOrganization : public Organization {
public:
    static std::string GetId() { return "test"; }
    virtual std::string GetName() const override { return "测试组织"; }
};
