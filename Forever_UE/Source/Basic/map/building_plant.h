#pragma once

#include "map/building_mod.h"

// FactoryBuilding：工厂建筑，照抄老工程(E:\Projects\Forever_UE\Source\Basic\map\
// building_basic.h/.cpp的FactoryBuilding)。和ResidenceBuilding/ShopBuilding一样是"一个
// 本体独占一个mod实例"模型。Layout()完整照抄老工程FactoryBuilding::LayoutBuilding的楼层/
// 房间数据，详见building_factory.md。
//
// 文件名是building_plant.h而不是building_factory.h：Source/Dependence/map/
// building_factory.h已经是BuildingFactory注册表类的头文件，避免和它在Basic/Dependence两个
// include目录下同名冲突(同一相对路径"map/building_factory.h"，basic.cpp里
// #include "map/building_factory.h"本意引用的是BuildingFactory注册表，不能被这个新文件
// 遮蔽)。类名/GetId()仍然是FactoryBuilding/"building_factory"，只有文件名避开了冲突，见
// room_plant.h/component_plant.h同样的处理。
class FactoryBuilding : public BuildingMod {
public:
	FactoryBuilding();

	static const char* GetId() { return "building_factory"; }
	virtual const char* GetType() const override { return "building_factory"; }
	virtual const char* GetName() override { return lastName.c_str(); }

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area);

private:
	std::string lastName;
	static int count;
};
