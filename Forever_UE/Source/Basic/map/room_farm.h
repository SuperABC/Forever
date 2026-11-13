#pragma once

#include "map/room_mod.h"

#include <string>


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
