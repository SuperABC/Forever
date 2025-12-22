#pragma once

#include "asset_base.h"
#include "zone_base.h"


// 子类注册函数
typedef void (*RegisterModAssetsFunc)(AssetFactory* factory);

// 园区资产子类
class ZoneAsset : public Asset {
public:
    ZoneAsset(std::shared_ptr<Zone> zone);
    ~ZoneAsset();

    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

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

    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

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

    static std::string GetId();
    virtual std::string GetType() const override;
    virtual std::string GetName() const override;

    std::shared_ptr<Room> GetRoom();
    void SetRoom(std::shared_ptr<Room> room);

private:
    std::shared_ptr<Room> room;
};
