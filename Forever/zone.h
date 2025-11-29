#pragma once

#include "zone_base.h"


// 子类注册函数
typedef void (*RegisterModZonesFunc)(ZoneFactory* factory);

// 主程序检测子类
class TestZone : public Zone {
public:
    TestZone() {
        name = count++;
    }

    static std::string GetId() { return "test"; }
    virtual std::string GetType() const override { return "test"; }
    virtual std::string GetName() const override { return "测试园区" + std::to_string(name); }

    static std::function<void(ZoneFactory*, BuildingFactory*, const std::vector<std::shared_ptr<Plot>>&)> ZoneGenerator;
    
private:
    static int count;

    int name;
};

