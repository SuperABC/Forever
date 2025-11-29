#pragma once

#include "change_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModChange : public Change {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetType() const override { return "mod"; }
    virtual std::string GetName() const override { return "mod"; }

    virtual std::vector<std::shared_ptr<Change>> ApplyChange() {
        return std::vector<std::shared_ptr<Change>>();
    }
};

