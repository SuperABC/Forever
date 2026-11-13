#pragma once

#include "map/room_mod.h"

#include <string>


// 办公室room：OfficeBuilding(三档密度)的房间。
class OfficeRoom : public RoomMod {
public:
	OfficeRoom();

	static const char* GetId() { return "room_office"; }
	virtual const char* GetType() const override { return "room_office"; }
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
