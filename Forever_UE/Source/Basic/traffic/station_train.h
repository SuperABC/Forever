#pragma once

#include "traffic/station_mod.h"

#include <string>
#include <vector>


// 火车轨道mesh：沿X轴延伸、轴心在起点边，长约250cm=0.25地图单位(1地图单位=10m)；没有隧道专用资产，隧道内复用。站内连线和站间连线(TrainRoute)共用。
inline constexpr const char* kTrainTrackMesh = "/Game/3rdParty/SM_ufekaevdw_tier_3/StaticMeshes/SM_ufekaevdw_tier_3.SM_ufekaevdw_tier_3";
inline constexpr float kTrainTrackUnit = 0.25f;

class TrainStation : public StationMod {
public:
	TrainStation();

	static const char* GetId() { return "station_train"; }
	virtual const char* GetType() const override { return "station_train"; }
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
