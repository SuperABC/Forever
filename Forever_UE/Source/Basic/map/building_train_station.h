#pragma once

#include "map/building_mod.h"


// 火车站，全图唯一一个，承载StationMod"station_train"。公交站不走Building/Lot路径，见
// station_*.h的BusStation::AssignRoads。
class TrainStationBuilding : public BuildingMod {
public:
	TrainStationBuilding();

	static const char* GetId() { return "building_train_station"; }
	virtual const char* GetType() const override { return "building_train_station"; }
	virtual const char* GetName() override;

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area); // 恒0，不参与CDF

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
