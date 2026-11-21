#pragma once

#include "map/room_mod.h"

#include <string>


// 党派会议室：不是住宅、没有容量/房租、不摆家具，只供党代会用，设计见room_meeting.md。
class MeetingRoom : public RoomMod {
public:
	FOREVER_MOD_GENERIC_UNIT_TEST("room")
	MeetingRoom();

	static const char* GetId() { return "room_meeting"; }
	virtual const char* GetType() const override { return "room_meeting"; }
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
