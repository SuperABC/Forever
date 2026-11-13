#pragma once

#include "map/building_mod.h"


// Mohar：用AddDecoration放一个成品建筑模型(3rdParty/Mohar)，并和楼层/房间系统融合，详见building_mohar.md。
class MoharBuilding : public BuildingMod {
public:
	MoharBuilding();

	static const char* GetId() { return "building_mohar"; }
	virtual const char* GetType() const override { return "building_mohar"; }
	virtual const char* GetName() override;

	// direction==-1时从boundaryRoads随机挑一个有路的方向回写(模型朝向靠它决定)。
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
