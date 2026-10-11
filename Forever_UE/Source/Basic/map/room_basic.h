#pragma once

#include "map/room_mod.h"

#include <string>


// ResidenceRoom：住宅Room，声明isResidential=true+residentialCapacity=1(每间住宅room=1个
// 名额，多人合住靠Map::Checkin()的名额池逻辑，不靠这个capacity强制)。workspace/storage/
// manufacture这3类占位属性保持RoomMod基类默认值(false/空)不动，另外3类由下面ShopRoom/
// WarehouseRoom/FactoryRoom各自启用。
// Basic编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由Config/ModLoader在
// 运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
//
// ResidenceRoom/ShopRoom/WarehouseRoom/ParkingRoom/FactoryRoom这五个具体类型合并进同一份
// room_basic.h/.cpp(不按residence/shop/plant各开一个文件)，和terrain_basic.h/.cpp里
// OceanTerrain/MountainTerrain合并的方式一样，详见room_basic.md。
//
// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+GetName里现拼，和
// Source/Basic/map/terrain_basic.h/.cpp的OceanTerrain同一个模式。
class ResidenceRoom : public RoomMod {
public:
	ResidenceRoom();

	static const char* GetId() { return "room_residence"; }
	virtual const char* GetType() const override { return "room_residence"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};

// ShopRoom：商店营业room，isWorkspace=true, workspaceCapacity=100。workspaceCapacity这个
// "工位数"占位字段带上真实数值，真正被Job系统消费留到Society域接入时再做，见room_basic.md。
class ShopRoom : public RoomMod {
public:
	ShopRoom();

	static const char* GetId() { return "room_shop"; }
	virtual const char* GetType() const override { return "room_shop"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};

// WarehouseRoom：Shop布局里过道/地下室用的仓储room(isStorage=true,
// storageConfig={{"shop",100.f}})。只有ShopBuilding用到，见room_basic.md。
class WarehouseRoom : public RoomMod {
public:
	WarehouseRoom();

	static const char* GetId() { return "room_warehouse"; }
	virtual const char* GetType() const override { return "room_warehouse"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};

// ParkingRoom：Shop/Factory地下停车场room，isParking=true+一个房间正中心的停车位，供
// Traffic::Init()在这里生成预置车辆，见Source/Core/traffic/traffic.md。ShopBuilding和
// FactoryBuilding共用同一个ParkingRoom类。
class ParkingRoom : public RoomMod {
public:
	ParkingRoom();

	static const char* GetId() { return "room_parking"; }
	virtual const char* GetType() const override { return "room_parking"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};

// FactoryRoom：isManufacture=true, manufactureTypes={"experience"}——"experience"这个
// 生产类型对应的ExperienceManufacture/ExperienceProduct属于Industry域，目前不接入，
// manufactureTypes这个占位字段先带上真实字符串，真正被消费留到Industry域接入时再做，见
// room_basic.md。
class FactoryRoom : public RoomMod {
public:
	FactoryRoom();

	static const char* GetId() { return "room_factory"; }
	virtual const char* GetType() const override { return "room_factory"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};

// TrainStationRoom/AirportRoom：公共交通站点建筑的候车厅/候机厅，不设任何
// isResidential/isWorkspace/isStorage/isManufacture/isParking标志——纯粹一个普通房间
// (公交站不挂建筑，不需要这个，见station_basic.h的BusStation)。
class TrainStationRoom : public RoomMod {
public:
	TrainStationRoom();

	static const char* GetId() { return "room_train_station"; }
	virtual const char* GetType() const override { return "room_train_station"; }
	virtual const char* GetName() override;

private:
	static int count;
	int id;
	std::string name;
};

class AirportRoom : public RoomMod {
public:
	AirportRoom();

	static const char* GetId() { return "room_airport"; }
	virtual const char* GetType() const override { return "room_airport"; }
	virtual const char* GetName() override;

private:
	static int count;
	int id;
	std::string name;
};
