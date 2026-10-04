#pragma once

#include "traffic/route_mod.h"

#include <string>


// BusRoute：公交线路——stationType="bus"、useRoadnet=true(接入现有车道路网)、
// drawPath=false。LayoutRoute按就近顺序把所有bus站点分成若干条环线，每条N站(kStationsPerLine)。
class BusRoute : public RouteMod {
public:
	BusRoute();

	static const char* GetId() { return "route_bus"; }
	virtual const char* GetType() const override { return "route_bus"; }
	virtual const char* GetName() override;

	virtual void LayoutRoute(const std::vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) override;

private:
	static int count;
	int id;
	std::string name;
};

// TrainRoute：火车线路——stationType="train"、useRoadnet=false(走2.3节贝塞尔曲线)、
// drawPath=true、trackMesh留空(这次不画铁轨)。假定全图只有一个train站点(2个接口/两条站台)，
// 两个接口各自沿departDir射线求交地图边界，各生成一个边缘站点，线路是
// "边缘1->接口0->边缘2->接口1->边缘1"的环。
class TrainRoute : public RouteMod {
public:
	TrainRoute();

	static const char* GetId() { return "route_train"; }
	virtual const char* GetType() const override { return "route_train"; }
	virtual const char* GetName() override;

	virtual void LayoutRoute(const std::vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) override;

private:
	static int count;
	int id;
	std::string name;
};

// AirRoute：航线——拓扑和TrainRoute完全相同(假定全图只有一个plane站点/两个跑道接口)，区别是
// stationType="plane"、drawPath=false、边缘站点z取cruiseHeight(飞行高度，老工程用100)。
class AirRoute : public RouteMod {
public:
	AirRoute();

	static const char* GetId() { return "route_air"; }
	virtual const char* GetType() const override { return "route_air"; }
	virtual const char* GetName() override;

	virtual void LayoutRoute(const std::vector<RouteStationInfo>& interfaces, int sizeX, int sizeY) override;

private:
	static int count;
	int id;
	std::string name;
};
