#pragma once

#include "map/building_mod.h"


// 办公楼(中密度)：只在办公楼中密度分区(AREA_OFFICE_MIDDLE)上有权重。
class OfficeMiddleBuilding : public BuildingMod {
public:
	OfficeMiddleBuilding();

	static const char* GetId() { return "building_office_middle"; }
	virtual const char* GetType() const override { return "building_office_middle"; }
	virtual const char* GetName() override;

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area);

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
