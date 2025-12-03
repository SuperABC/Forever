#pragma once

#include "asset_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModAsset : public Asset {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetType() const override { return "mod"; }
    virtual std::string GetName() const override { return "模组资产"; }

};

