#pragma once

#include "zone_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModZone : public Zone {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetName() const override { return "模组职业"; }

};

