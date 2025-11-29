#pragma once

#include "roadnet_base.h"

#include <memory>
#include <string>


// 模组检测子类
class ModRoadnet : public Roadnet {
public:
    static std::string GetId() { return "mod"; }
    virtual std::string GetType() const override { return "mod"; }
    virtual std::string GetName() const override { return "模组路网"; }

    virtual void DistributeRoadnet(int width, int height,
        std::function<std::string(int, int)> get) override {

    }
};

