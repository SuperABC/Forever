#pragma once

#include "zone_base.h"


// 子类注册函数
typedef void (*RegisterModZonesFunc)(ZoneFactory* factory);

// 主程序检测子类
class TestZone : public Zone {
public:
    static std::string GetId() { return "test"; }
    virtual std::string GetName() const override { return "测试园区"; }

};
