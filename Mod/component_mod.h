#pragma once

#include "component_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModComponent : public Component {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetName() const override { return "模组组合"; }
};

