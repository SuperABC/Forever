#pragma once

#include "map/room_mod.h"

#include <string>


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
