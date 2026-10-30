#pragma once

#include "traffic/route_mod.h"

#include <string>


// BusRoute：公交线路——stationType="bus"、useRoadnet=true(接入现有车道路网)、
// instantiate=false。LayoutRoute按每个接口自己的departDir相对所有接口形心的绕行方向分两条
// 环线(一条顺时针一条逆时针)，具体算法见route_basic.md。
class BusRoute : public RouteMod {
public:
	BusRoute();

	static const char* GetId() { return "route_bus"; }
	virtual const char* GetType() const override { return "route_bus"; }
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

// TrainRoute：火车线路——stationType="train"、useRoadnet=false(走三次贝塞尔曲线)、
// instantiate=true(沿线铺SM_ufekaevdw_tier_3轨道，过山爬坡/挖隧道，见BuildTrack)。假定全图只有一个train站点(4个接口/两条
// 轨道)，拓扑是LayoutDualTrackStationLoop(route_basic.md)的单站双轨道环，只在每条轨道
// 中点真正停靠。
class TrainRoute : public RouteMod {
public:
	TrainRoute();

	static const char* GetId() { return "route_train"; }
	virtual const char* GetType() const override { return "route_train"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

	virtual void LayoutRoute(const std::vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) override;

	// 铺轨：沿每条link的贝塞尔底线按1单位采样地形，过山时和JingRoadnet::buildArm同一套判定——
	// 坡度小(或山后是海)就抬升(AddTerrainFit)，陡就挖隧道(S形下坡+水平隧道段，隧道口AddTunnelHatch)，
	// 前后平地不够放坡道的退化为抬升。详见route_basic.md"TrainRoute::BuildTrack"。
	virtual void BuildTrack(const std::vector<RouteTrackLinkEnds>& linkEnds,
		const std::function<const std::string&(int, int)>& getTerrain,
		const std::function<std::pair<bool, float>(int, int)>& getWater,
		const std::function<float(float, float)>& getHeight) override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};

// AirRoute：航线——拓扑和TrainRoute完全相同(假定全图只有一个plane站点/四个跑道接口)，区别是
// stationType="plane"、instantiate=false、地图边缘站点z取cruiseHeight(飞行高度，100地图单位，
// 跑道本身的接口/中点停靠点仍然贴地)。
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
