#pragma once

#include "room.h"
#include "component.h"
#include "building_base.h"
#include "component_base.h"


// 子类注册函数
typedef void (*RegisterModBuildingsFunc)(BuildingFactory* factory);

// 主程序检测子类
class TestBuilding : public Building {
public:
    static std::string GetId() { return "test"; }
    virtual std::string GetName() const override { return "测试建筑"; }

    static std::vector<float> GetPower() { return std::vector<float>(13, 1.f); }

    virtual float RandomAcreage() const { return 10000.f; }
    virtual float GetAcreageMin() const { return 2000.f; }
    virtual float GetAcreageMax() const { return 20000.f; }

    virtual void LayoutRooms() {
        auto component = CreateComponent<TestComponent>();
        component->AddRoom(CreateRoom<TestRoom>(1, 100.f));
    }
};
