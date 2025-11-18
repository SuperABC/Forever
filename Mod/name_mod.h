#pragma once

#include "name_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModName : public Name {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetName() const override { return "模组姓名"; }

    virtual std::string GenerateName(bool male = true, bool female = true, bool neutral = true) {
        return "";
    }
};

