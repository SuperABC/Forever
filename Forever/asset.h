#pragma once

#include "asset_base.h"
#include "zone_base.h"


// 子类注册函数
typedef void (*RegisterModAssetsFunc)(AssetFactory* factory);

// 主程序检测子类
class TestAsset : public Asset {
public:
    TestAsset();
    ~TestAsset();

    static std::string GetId() { return "test"; }
    virtual std::string GetType() const override { return "test"; }
    virtual std::string GetName() const override { return "测试资产"; }

private:

};

// 园区资产子类
class ZoneAsset : public Asset {
public:
    ZoneAsset(std::shared_ptr<Zone> zone);
    ~ZoneAsset();

    static std::string GetId() { return "zoneAsset"; }
    virtual std::string GetType() const override { return "zoneAsset"; }
    virtual std::string GetName() const override { return "园区资产"; }

    std::shared_ptr<Zone> GetZone();
    void SetZone(std::shared_ptr<Zone> zone);

private:
    std::shared_ptr<Zone> zone;
};

// 建筑资产子类
class BuildingAsset : public Asset {
public:
    BuildingAsset(std::shared_ptr<Building> building);
    ~BuildingAsset();

    static std::string GetId() { return "buildingAsset"; }
    virtual std::string GetType() const override { return "buildingAsset"; }
    virtual std::string GetName() const override { return "园区资产"; }

    std::shared_ptr<Building> GetBuilding();
    void SetBuilding(std::shared_ptr<Building> building);

private:
    std::shared_ptr<Building> building;
};

// 房间资产子类
class RoomAsset : public Asset {
public:
    RoomAsset(std::shared_ptr<Room> room);
    ~RoomAsset();

    static std::string GetId() { return "roomAsset"; }
    virtual std::string GetType() const override { return "roomAsset"; }
    virtual std::string GetName() const override { return "园区资产"; }

    std::shared_ptr<Room> GetRoom();
    void SetRoom(std::shared_ptr<Room> room);

private:
    std::shared_ptr<Room> room;
};
