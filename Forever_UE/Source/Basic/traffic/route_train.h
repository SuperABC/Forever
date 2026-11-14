#pragma once

#include "traffic/route_mod.h"

#include "route_common.h"

#include <string>
#include <vector>


class TrainRoute : public RouteMod {
public:
	TrainRoute();

	static const char* GetId() { return "route_train"; }
	virtual const char* GetType() const override { return "route_train"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

	virtual void LayoutRoute(const std::vector<RouteStationInfo>& stations, int sizeX, int sizeY) override;

	// 铺轨：过山爬坡/挖隧道，见route_train.md"TrainRoute::BuildTrack"。
	virtual void BuildTrack(const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		const std::function<float(float, float)>& getHeight) override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	// 站间的一对铁轨(right沿中轴线方向，left倒着走)和它们共用的中轴线(按1单位弧长采样)；BuildTrack据此统一判定隧道、统一开洞。
	struct RailPair { int right, left; std::vector<CurveSample> midline; };
	std::vector<RailPair> railPairs;
	static int count;
	int id;
	std::string name;
};
