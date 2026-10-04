#pragma once

#include "traffic/station_mod.h"

#include <string>
#include <vector>


// BusStation：公交站，不挂建筑、不占用任何Lot面积，直接贴着道路摆(见StationMod::AssignRoads
// 的说明)。测试布局：只贴井字路网中心正方形lot的四条边道路(中山西/东/北/南路)，每条路两侧
// 各摆一个(4条路x2侧=8个)，验证BusRoute::LayoutRoute的环线分组。
class BusStation : public StationMod {
public:
	BusStation();

	static const char* GetId() { return "station_bus"; }
	virtual const char* GetType() const override { return "station_bus"; }
	virtual const char* GetName() override;

	virtual void AssignRoads(const std::vector<Road*>& roads, RoadStationEmitFunc emit, void* context) override;

private:
	static int count;
	int id;
	std::string name;
};

// TrainStation：火车站，4个接口(两条轨道)，放在远离道路一侧(楼体贴道路，见
// TrainStationBuilding::Layout的NearRoadFootprint)。[leftIn,rightOut]同向构成轨道1，
// [rightIn,leftOut]反向构成轨道2，详见Layout()实现和route_basic.cpp
// LayoutDualTrackStationLoop的拓扑说明。挂在TrainStationBuilding(building_train_station)上。
class TrainStation : public StationMod {
public:
	TrainStation();

	static const char* GetId() { return "station_train"; }
	virtual const char* GetType() const override { return "station_train"; }
	virtual const char* GetName() override;

	virtual void Layout(int direction, float sizeX, float sizeY) override;

private:
	static int count;
	int id;
	std::string name;
};

// AirStation：机场，4个接口(两条跑道)，放在远离道路一侧(楼体贴道路，见
// AirportBuilding::Layout的NearRoadFootprint)。[leftIn,rightOut]同向构成跑道1，
// [rightIn,leftOut]反向构成跑道2，详见Layout()实现和route_basic.cpp
// LayoutDualTrackStationLoop的拓扑说明。挂在AirportBuilding(building_airport)上。
class AirStation : public StationMod {
public:
	AirStation();

	static const char* GetId() { return "station_air"; }
	virtual const char* GetType() const override { return "station_air"; }
	virtual const char* GetName() override;

	virtual void Layout(int direction, float sizeX, float sizeY) override;

private:
	static int count;
	int id;
	std::string name;
};
