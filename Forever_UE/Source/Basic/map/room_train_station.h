#pragma once

#include "map/room_mod.h"

#include <string>


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
