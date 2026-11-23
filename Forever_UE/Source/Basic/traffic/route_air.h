#pragma once

#include "traffic/route_mod.h"

#include <string>


class AirRoute : public RouteMod {
public:
	FOREVER_MOD_GENERIC_UNIT_TEST("route")
	// args：mod参数串，--skip=跳过自动布局、改用编辑器的手动线路，见route_mod.md"手动线路"。
	explicit AirRoute(const std::string& args = std::string());

	static const char* GetId() { return "route_air"; }
	virtual const char* GetType() const override { return "route_air"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

	virtual void LayoutRoute(const std::vector<RouteStationInfo>& stations, int sizeX, int sizeY) override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	void LayoutManual(const std::vector<RouteStationInfo>& stationInfos);
	static int count;
	int id;
	std::string name;
};
