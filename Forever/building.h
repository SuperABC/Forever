#pragma once

#include "building_base.h"

#include "room.h"
#include "component.h"


// 子类注册函数
typedef void (*RegisterModBuildingsFunc)(BuildingFactory* factory);

// 主程序检测子类
class FlatBuilding : public Building {
public:
    FlatBuilding();

    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

    static std::vector<float> GetPower();

    virtual float RandomAcreage() const override;
    virtual float GetAcreageMin() const override;
    virtual float GetAcreageMax() const override;

    virtual Rect LayoutConstruction() const override;
    virtual void LayoutRooms(RoomFactory* factory, std::unique_ptr<Layout>& layout) override;

private:
    static int count;

    int name;
};
