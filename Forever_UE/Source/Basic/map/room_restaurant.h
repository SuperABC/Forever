#pragma once

#include "map/room_mod.h"

#include <string>


// 餐厅room：CommerceHighBuilding一楼的联排。
class RestaurantRoom : public RoomMod {
public:
	RestaurantRoom();

	static const char* GetId() { return "room_restaurant"; }
	virtual const char* GetType() const override { return "room_restaurant"; }
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
