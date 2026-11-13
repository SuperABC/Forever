#pragma once

#include "traffic/station_mod.h"

#include <string>
#include <vector>


class AirStation : public StationMod {
public:
	AirStation();

	static const char* GetId() { return "station_air"; }
	virtual const char* GetType() const override { return "station_air"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

	virtual void Layout(int direction, float sizeX, float sizeY) override;

	// 挂建筑的站点类型，不贴路——空实现，见station_mod.h StationMod::AssignRoads的约定。
	static void AssignRoads(const std::vector<Road*>& roads, RoadStationEmitFunc emit, void* context) {}

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
