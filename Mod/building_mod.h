#pragma once

#include "building_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModBuilding : public Building {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetName() const override { return "模组建筑"; }
};

