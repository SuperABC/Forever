#pragma once

#include "roadnet_base.h"


// 子类注册函数
typedef void (*RegisterModRoadnetsFunc)(RoadnetFactory* factory);

// 主程序检测子类
class TestRoadnet : public Roadnet {
public:
    virtual std::string GetName() const override { return "测试路网"; }
};
