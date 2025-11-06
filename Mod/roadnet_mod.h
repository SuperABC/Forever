#pragma once

#include "roadnet_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModRoadnet : public Roadnet {
public:
    std::string GetName() const override { return "模组职业"; }
};

