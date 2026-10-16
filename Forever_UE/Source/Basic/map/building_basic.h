#pragma once

#include "map/building_mod.h"


// ResidenceBuilding：Building域默认内容，只有一个通用占位类型，只走权重CDF随机填充路径（不
// 使用显式占位）。暂时不按lot->GetArea()筛选/区分权重，任意住宅lot统一给权重1，等以后设计
// 具体建筑类型时再按类型区分。
// Basic编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由Config/ModLoader在
// 运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
//
// "一个本体独占一个mod实例"模型：RandomAcreage/GetAcreageMin/GetAcreageMax/GetPower/
// Assign全部是不需要实例的static方法(通过BuildingFactory::RegisterBuilding注册)。
//
// ResidenceBuilding/ShopBuilding/FactoryBuilding这三个具体类型合并进同一份
// building_basic.h/.cpp(不按residence/shop/plant各开一个文件)，和terrain_basic.h/.cpp
// 里OceanTerrain/MountainTerrain合并的方式一样——每个concept统一只对应一份`<concept>_basic.
// h/.cpp`，不按"这个类型是哪种业务场景"拆文件。详见building_basic.md。
class ResidenceBuilding : public BuildingMod {
public:
	// 构造函数只记下id(用static计数器)，不设footprint/楼层/lodMaterial，这些要等Layout()
	// 才知道落地上下文。
	ResidenceBuilding();

	static const char* GetId() { return "building_residence"; }
	virtual const char* GetType() const override { return "building_residence"; }
	// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id，和
	// Source/Basic/map/terrain_basic.h/.cpp的OceanTerrain同一个模式。
	virtual const char* GetName() override;

	// 这里才设footprint/basements/layers/floorHeights，以及AssignFloor/AssignRoom/
	// ArrangeRow声明楼层内部布局。direction==-1(FillRemainder落地)时从boundaryRoads里
	// 随机挑一个有真实边界路的方向回写，保证这栋building最终有确定方向可用(行人/车辆导航的
	// "outside"端点要靠它找到该连去哪条路，见building_mod.h的Layout()注释)。
	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

	// 不用显式占位，空实现。
	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area);

private:
	static int count;
	int id;
	std::string name;
};

// ShopBuilding：商店建筑。和ResidenceBuilding一样是"一个本体独占一个mod实例"模型：
// RandomAcreage/GetAcreageMin/GetAcreageMax/GetPower/Assign全部是不需要实例的static方法，
// 通过BuildingFactory::RegisterBuilding注册。楼层/房间布局详见building_basic.md。
class ShopBuilding : public BuildingMod {
public:
	ShopBuilding();

	static const char* GetId() { return "building_shop"; }
	virtual const char* GetType() const override { return "building_shop"; }
	// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id，和terrain_basic同一个
	// 模式。
	virtual const char* GetName() override;

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

	// 不用显式占位，走权重CDF随机填充路径(和ResidenceBuilding::Assign一样空实现)。
	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area);

private:
	static int count;
	int id;
	std::string name;
};

// FactoryBuilding：工厂建筑，和ResidenceBuilding/ShopBuilding一样是"一个本体独占一个mod
// 实例"模型。楼层/房间布局详见building_basic.md。
class FactoryBuilding : public BuildingMod {
public:
	FactoryBuilding();

	static const char* GetId() { return "building_factory"; }
	virtual const char* GetType() const override { return "building_factory"; }
	// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id，和terrain_basic同一个
	// 模式。
	virtual const char* GetName() override;

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area);

private:
	static int count;
	int id;
	std::string name;
};

// 火车站——全图唯一一个，用Lot::FindAdaptivePlacement自适应找一块空间，不挑剔具体是哪个
// 方向，和机场/围墙测试场景共存。承载StationMod"station_train"。公交站不走这条Building/Lot
// 路径了，直接贴着道路摆，见station_basic.h的BusStation::AssignRoads。
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

private:
	static int count;
	int id;
	std::string name;
};

// 机场——全图唯一一个，和TrainStationBuilding同样的"显式占第一块贴路lot、楼体偏向道路一侧"
// 做法。承载StationMod"station_air"。
class AirportBuilding : public BuildingMod {
public:
	AirportBuilding();

	static const char* GetId() { return "building_airport"; }
	virtual const char* GetType() const override { return "building_airport"; }
	virtual const char* GetName() override;

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area); // 恒0，不参与CDF

private:
	static int count;
	int id;
	std::string name;
};

// 公园：0层(不放楼体)，只在自己的quad范围内用BuildingMod::AddTreeArea种一片树，测试植被
// 系统的面状接口。走权重CDF随机填充(绿地分区权重1，住宅分区0.25)，不做显式占位。详见
// building_basic.md。
class ParkBuilding : public BuildingMod {
public:
	ParkBuilding();

	static const char* GetId() { return "building_park"; }
	virtual const char* GetType() const override { return "building_park"; }
	virtual const char* GetName() override;

	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;
	// 在整块quad里随机种树(地图全部生成完之后由Map::InitVegetation调用)。
	virtual void LayoutTrees(const Quad& quad, const std::unordered_map<int, Road*>& boundaryRoads) override;

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	static float RandomAcreage();
	static float GetAcreageMin();
	static float GetAcreageMax();
	static float GetPower(AREA_TYPE area);

private:
	static int count;
	int id;
	std::string name;
};
