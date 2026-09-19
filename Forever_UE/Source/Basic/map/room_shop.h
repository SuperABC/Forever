#pragma once

#include "map/room_mod.h"

// ShopRoom：商店营业room，照抄老工程(E:\Projects\Forever_UE\Source\Basic\map\room_basic.h/.cpp
// 的ShopRoom::ConfigRoom) isWorkspace=true, workspaceCapacity=100——这次只落地
// building/room/component这一层，workspaceCapacity这个"工位数"占位字段先带上真实数值，
// 真正被Job系统消费留到Society域迁移时再接，见building_shop.md。
class ShopRoom : public RoomMod {
public:
	ShopRoom();

	static const char* GetId() { return "room_shop"; }
	virtual const char* GetType() const override { return "room_shop"; }
	virtual const char* GetName() override { return "ShopRoom"; }
};

// WarehouseRoom：Shop布局里过道/地下室用的仓储room，照抄老工程WarehouseRoom::ConfigRoom
// (isStorage=true, storageConfig={{"shop",100.f}})。只有ShopBuilding用到，不单独开文件，
// 见building_shop.md。
class WarehouseRoom : public RoomMod {
public:
	WarehouseRoom();

	static const char* GetId() { return "room_warehouse"; }
	virtual const char* GetType() const override { return "room_warehouse"; }
	virtual const char* GetName() override { return "WarehouseRoom"; }
};

// ParkingRoom：Shop/Factory地下停车场room，纯占位——老工程isParking/parkingSpaces这套
// 车位数据这次不迁移(应用户要求，不处理车辆/车位概念)，保持RoomMod基类默认值全false/空，
// 只提供"room_parking"类型id给AssignRoom用，单纯让这块地下空间在几何上有个房间类型标签，
// 不参与isResidential/isWorkspace/isStorage/isManufacture任何一种容量占位。
class ParkingRoom : public RoomMod {
public:
	static const char* GetId() { return "room_parking"; }
	virtual const char* GetType() const override { return "room_parking"; }
	virtual const char* GetName() override { return "ParkingRoom"; }
};
