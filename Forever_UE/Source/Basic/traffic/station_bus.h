#pragma once

#include "traffic/station_mod.h"

#include <string>
#include <vector>


class BusStation : public StationMod {
public:
	BusStation();

	static const char* GetId() { return "station_bus"; }
	virtual const char* GetType() const override { return "station_bus"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

	static void AssignRoads(const std::vector<Road*>& roads, RoadStationEmitFunc emit, void* context);

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
