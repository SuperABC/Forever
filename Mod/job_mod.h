#pragma once

#include "job_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModJob : public Job {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetType() const override { return "mod"; }
    virtual std::string GetName() const override { return "模组职业"; }

    virtual std::vector<std::string> GetScripts() const override {
        return { "../Resources/scripts/jobs/mod.json" };
    }
};

