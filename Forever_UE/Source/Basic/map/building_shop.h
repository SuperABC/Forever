#pragma once

#include "map/building_mod.h"

// ShopBuilding：商店建筑，照抄老工程(E:\Projects\Forever_UE\Source\Basic\map\
// building_basic.h/.cpp的ShopBuilding)。和ResidenceBuilding一样是"一个本体独占一个mod
// 实例"模型：RandomAcreage/GetAcreageMin/GetAcreageMax/GetPower/Assign全部是不需要实例
// 的static方法，通过BuildingFactory::RegisterBuilding注册。Layout()完整照抄老工程
// ShopBuilding::LayoutBuilding的楼层/房间数据，详见building_shop.md。
class ShopBuilding : public BuildingMod {
public:
	ShopBuilding();

	static const char* GetId() { return "building_shop"; }
	virtual const char* GetType() const override { return "building_shop"; }
	virtual const char* GetName() override { return lastName.c_str(); }

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

	// 不用显式占位，走权重CDF随机填充路径(和ResidenceBuilding::Assign一样空实现)。
	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area);

private:
	std::string lastName;
	static int count;
};
