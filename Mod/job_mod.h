#pragma once

#include "job_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModJob : public Job {
public:
    std::string GetName() const override { return "模组职业"; }
};

