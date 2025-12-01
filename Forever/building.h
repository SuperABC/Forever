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
    virtual std::string GetType() const override { return "test"; }
    virtual std::string GetName() const override { return std::string("测试建筑") + std::to_string(name); }

    static std::vector<float> GetPower() { return std::vector<float>(13, 1.f); }

    virtual float RandomAcreage() const override { return 10000.f; }
    virtual float GetAcreageMin() const override { return 2000.f; }
    virtual float GetAcreageMax() const override { return 20000.f; }

    virtual void LayoutRooms(RoomFactory* factory, std::unique_ptr<Layout>& layout) override {
        int direction = GetRandom(4);

        auto component = CreateComponent<TestComponent>();
        ReadFloor(-1, GetSizeX() / 2.f, GetSizeY() / 2.f, direction, "single_room", layout);
        ReadFloor(0, GetSizeX() / 2.f, GetSizeY() / 2.f, direction, "straight_linear", layout);
        ReadFloor(1, GetSizeX() / 2.f, GetSizeY() / 2.f, direction, "straight_linear", layout);
        AssignRoom(-1, 0, "test", component, factory);
        ArrangeRow(0, 0, "test", 120.f, component, factory);
        ArrangeRow(0, 1, "test", 120.f, component, factory);
        ArrangeRow(1, 0, "test", 120.f, component, factory);
        ArrangeRow(1, 1, "test", 120.f, component, factory);
    }

private:
    static int count;

    int name;
};
