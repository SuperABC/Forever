#pragma once

#include "map/room_mod.h"

#include <string>


// Room域默认内容：各类型的属性和设计见room_basic.md。
class ResidenceRoom : public RoomMod {
public:
	ResidenceRoom();

	static const char* GetId() { return "room_residence"; }
	virtual const char* GetType() const override { return "room_residence"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// 商店营业room。
class ShopRoom : public RoomMod {
public:
	ShopRoom();

	static const char* GetId() { return "room_shop"; }
	virtual const char* GetType() const override { return "room_shop"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// Shop布局里过道/地下室用的仓储room，只有ShopBuilding用到。
class WarehouseRoom : public RoomMod {
public:
	WarehouseRoom();

	static const char* GetId() { return "room_warehouse"; }
	virtual const char* GetType() const override { return "room_warehouse"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// Shop/Factory地下停车场room，ShopBuilding和FactoryBuilding共用。
class ParkingRoom : public RoomMod {
public:
	ParkingRoom();

	static const char* GetId() { return "room_parking"; }
	virtual const char* GetType() const override { return "room_parking"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// TrainStationRoom/AirportRoom：火车站候车厅/机场候机厅。TrainStationRoom是纯普通房间，
// AirportRoom声明了停机位。
class TrainStationRoom : public RoomMod {
public:
	TrainStationRoom();

	static const char* GetId() { return "room_train_station"; }
	virtual const char* GetType() const override { return "room_train_station"; }
	virtual const char* GetName() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
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
	virtual void SetProperty() override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// 三种工坊房间，由FactoryBuilding随机选其一布局。
class FarmRoom : public RoomMod {
public:
	FarmRoom();

	static const char* GetId() { return "room_farm"; }
	virtual const char* GetType() const override { return "room_farm"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class RanchRoom : public RoomMod {
public:
	RanchRoom();

	static const char* GetId() { return "room_ranch"; }
	virtual const char* GetType() const override { return "room_ranch"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

class FoodFactoryRoom : public RoomMod {
public:
	FoodFactoryRoom();

	static const char* GetId() { return "room_food_factory"; }
	virtual const char* GetType() const override { return "room_food_factory"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

public:
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

