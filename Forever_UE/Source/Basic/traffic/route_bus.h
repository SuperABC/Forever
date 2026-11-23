#pragma once

#include "traffic/route_mod.h"

#include <string>


class BusRoute : public RouteMod {
public:
	FOREVER_MOD_GENERIC_UNIT_TEST("route")
	// args：mod参数串，--skip=跳过自动布局、改用编辑器"路网路线"的手动公交线，见route_bus.md"手动线路"。
	explicit BusRoute(const std::string& args = std::string());

	static const char* GetId() { return "route_bus"; }
	virtual const char* GetType() const override { return "route_bus"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

	virtual void LayoutRoute(const std::vector<RouteStationInfo>& stations, int sizeX, int sizeY) override;
private:
	void LayoutManual();
public:

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
