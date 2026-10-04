#pragma once

#include "traffic/route_mod.h"

#include <string>


// BusRoute：公交线路——stationType="bus"、useRoadnet=true(接入现有车道路网)、
// drawPath=false。LayoutRoute按每个接口自己的departDir相对所有接口形心的绕行方向分两条
// 环线(一条顺时针一条逆时针)，具体算法见route_basic.md。
class BusRoute : public RouteMod {
public:
	BusRoute();

	static const char* GetId() { return "route_bus"; }
	virtual const char* GetType() const override { return "route_bus"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

	virtual void LayoutRoute(const std::vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) override;

private:
	static int count;
	int id;
	std::string name;
};

// TrainRoute：火车线路——stationType="train"、useRoadnet=false(走三次贝塞尔曲线)、
// drawPath=true、trackMesh留空(这次不画铁轨)。假定全图只有一个train站点(4个接口/两条
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

private:
	static int count;
	int id;
	std::string name;
};

// AirRoute：航线——拓扑和TrainRoute完全相同(假定全图只有一个plane站点/四个跑道接口)，区别是
// stationType="plane"、drawPath=false、地图边缘站点z取cruiseHeight(飞行高度，老工程用100，
// 跑道本身的接口/中点停靠点仍然贴地)。
class AirRoute : public RouteMod {
public:
	AirRoute();

	static const char* GetId() { return "route_air"; }
	virtual const char* GetType() const override { return "route_air"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

	virtual void LayoutRoute(const std::vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) override;

private:
	static int count;
	int id;
	std::string name;
};
