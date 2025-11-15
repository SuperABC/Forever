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
    TestBuilding() {
        name = count++;
        layers = 2;
        basements = 1;
    }

    static std::string GetId() { return "test"; }
    virtual std::string GetName() const override { return std::string("测试建筑") + std::to_string(name); }

    static std::vector<float> GetPower() { return std::vector<float>(13, 1.f); }

    virtual float RandomAcreage() const { return 10000.f; }
    virtual float GetAcreageMin() const { return 2000.f; }
    virtual float GetAcreageMax() const { return 20000.f; }

    virtual void LayoutRooms() {
        auto component = CreateComponent<TestComponent>();
        ReadFloor(-1, GetSizeX() / 2.f, GetSizeY() / 2.f, "single_room");
        ReadFloor(0, GetSizeX() / 2.f, GetSizeY() / 2.f, "straight_linear");
        ReadFloor(1, GetSizeX() / 2.f, GetSizeY() / 2.f, "straight_linear");
        AssignRoom(-1, 0, "test", component);
        ArrangeRow(0, 0, "test", 100.f, component);
        ArrangeRow(0, 1, "test", 100.f, component);
        ArrangeRow(1, 0, "test", 100.f, component);
        ArrangeRow(1, 1, "test", 100.f, component);
    }

private:
    static int count;

    int name;
};
