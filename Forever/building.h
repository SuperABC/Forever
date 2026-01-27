#pragma once

#include "building_base.h"

#include "room.h"
#include "component.h"


// 子类注册函数
typedef void (*RegisterModBuildingsFunc)(BuildingFactory* factory);

// 默认住宅建筑
class DefaultResidentialBuilding : public Building {
public:
    DefaultResidentialBuilding();

    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

    static std::vector<float> GetPower();

    virtual float RandomAcreage() const override;
    virtual float GetAcreageMin() const override;
    virtual float GetAcreageMax() const override;

    virtual Rect LayoutConstruction() override;
    virtual void LayoutRooms(RoomFactory* factory, std::unique_ptr<Layout>& layout) override;

private:
    static int count;

    int name;
};

// 默认工作建筑
class DefaultWorkingBuilding : public Building {
public:
    DefaultWorkingBuilding();

    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

    static std::vector<float> GetPower();

    virtual float RandomAcreage() const override;
    virtual float GetAcreageMin() const override;
    virtual float GetAcreageMax() const override;

    virtual Rect LayoutConstruction() override;
    virtual void LayoutRooms(RoomFactory* factory, std::unique_ptr<Layout>& layout) override;

private:
    static int count;

    int name;
};
