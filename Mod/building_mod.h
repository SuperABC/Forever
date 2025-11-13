#pragma once

#include "building_base.h"

#include "component_mod.h"
#include "room_mod.h"

#include <memory>
#include <string>


// 模组检测子类
class ModBuilding : public Building {
public:
    ModBuilding() { count++; }

    static std::string GetId() { return "mod"; }
    virtual std::string GetName() const override { return std::string("模组建筑") + std::to_string(count); }

    static std::vector<float> GetPower() { return std::vector<float>(13, 1.f); }

    virtual float RandomAcreage() const { return 10000.f; }
    virtual float GetAcreageMin() const { return 2000.f; }
    virtual float GetAcreageMax() const { return 20000.f; }

    virtual void LayoutRooms() {
        auto component = CreateComponent<ModComponent>();
        component->AddRoom(CreateRoom<ModRoom>(1, 100.f));
    }

private:
    static int count;
};

int ModBuilding::count = 0;
