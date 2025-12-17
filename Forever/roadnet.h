#pragma once

#include "roadnet_base.h"


// 子类注册函数
typedef void (*RegisterModRoadnetsFunc)(RoadnetFactory* factory);

// 主程序检测子类
class JingRoadnet : public Roadnet {
public:
    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

    virtual void DistributeRoadnet(int width, int height,
        std::function<std::string(int, int)> get) override;
};
