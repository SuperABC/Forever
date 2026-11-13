#pragma once

#include "traffic/route_mod.h"

#include <string>


class AirRoute : public RouteMod {
public:
	AirRoute();

	static const char* GetId() { return "route_air"; }
	virtual const char* GetType() const override { return "route_air"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

	virtual void LayoutRoute(const std::vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
