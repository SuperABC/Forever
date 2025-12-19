#pragma once

#include "building_base.h"

#include "component_mod.h"
#include "room_mod.h"

#include <memory>
#include <string>


// 模组检测子类
class ModBuilding : public Building {
public:
    ModBuilding() {
        name = count++;
    }

    static std::string GetId() { return "mod"; }
    virtual std::string GetType() const override { return "mod"; }
    virtual std::string GetName() const override { return std::string("模组建筑") + std::to_string(name); }

    static std::vector<float> GetPower() { return std::vector<float>(13, 0.1f); }

    virtual float RandomAcreage() const override { return 10000.f; }
    virtual float GetAcreageMin() const override { return 2000.f; }
    virtual float GetAcreageMax() const override { return 20000.f; }

    virtual Rect LayoutConstruction() const override {
        return Rect(0.5f * GetSizeX(), 0.5f * GetSizeY(), 0.5f * GetSizeX(), 0.5f * GetSizeY());
    }
    virtual void LayoutRooms(RoomFactory* factory, std::unique_ptr<Layout>& layout) override {
        int direction = GetRandom(4);

        auto component = CreateComponent<ModComponent>();
        ReadFloor(0, direction, "single_room", layout);
        AssignRoom(0, 0, "mod", component, factory);
    }

private:
    static int count;

    int name;
};

int ModBuilding::count = 0;
