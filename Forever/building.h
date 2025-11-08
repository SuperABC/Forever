#pragma once

#include "building_base.h"


// 子类注册函数
typedef void (*RegisterModBuildingsFunc)(BuildingFactory* factory);

// 主程序检测子类
class TestBuilding : public Building {
public:
    static std::string GetId() { return "test"; }
    virtual std::string GetName() const override { return "测试建筑"; }
};
