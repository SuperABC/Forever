#pragma once

#include "map/building_mod.h"


// ResidenceBuilding：Building域默认内容，只有一个通用占位类型，只走权重CDF随机填充路径（不
// 使用显式占位）。面积采样公式照抄老工程ResidentialBuilding原始数值；暂时不按lot->GetArea()
// 筛选/区分权重，任意lot统一给权重1，等以后设计具体建筑类型时再按类型区分。
// Basic现在编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由Config/ModLoader在
// 运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
//
// 这次会话改回"一个本体独占一个mod实例"模型：RandomAcreage/GetAcreageMin/GetAcreageMax/
// GetPower/Assign全部改成不需要实例的static方法(通过BuildingFactory::RegisterBuilding注册)，
// 寻址用的唯一名字计数器可以像老工程一样直接写在构造函数里。
//
// ResidenceBuilding/ShopBuilding/FactoryBuilding这三个具体类型合并进同一份
// building_basic.h/.cpp(不再按residence/shop/plant各开一个文件)，和terrain_basic.h/.cpp
// 里OceanTerrain/MountainTerrain合并的方式一样——每个concept统一只对应一份`<concept>_basic.
// h/.cpp`，不再按"这个类型是哪种业务场景"拆文件。合并之后不再需要"文件名避开
// Dependence/map/building_factory.h同名冲突"这个历史包袱(原来的building_plant.h就是为了
// 避开这个冲突才起的名字)——`building_basic.h`本身和Dependence那边任何文件都不同名，见
// Source/Basic/README.md。详见building_basic.md。
class ResidenceBuilding : public BuildingMod {
public:
	// 构造函数只记下id(用static计数器)，不设footprint/楼层/lodMaterial，这些要等Layout()
	// 才知道落地上下文。
	ResidenceBuilding();

	static const char* GetId() { return "building_residence"; }
	virtual const char* GetType() const override { return "building_residence"; }
	// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id，和
	// Source/Basic/map/terrain_basic.h/.cpp的OceanTerrain同一个模式(之前这里是构造函数
	// 里一次性拼好存lastName、GetName()只返回lastName.c_str()的风格，这次统一改成和
	// terrain一致的"GetName()里现拼")。
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

// ShopBuilding：商店建筑，照抄老工程(E:\Projects\Forever_UE\Source\Basic\map\
// building_basic.h/.cpp的ShopBuilding)。和ResidenceBuilding一样是"一个本体独占一个mod
// 实例"模型：RandomAcreage/GetAcreageMin/GetAcreageMax/GetPower/Assign全部是不需要实例
// 的static方法，通过BuildingFactory::RegisterBuilding注册。Layout()完整照抄老工程
// ShopBuilding::LayoutBuilding的楼层/房间数据，详见building_basic.md。
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

// FactoryBuilding：工厂建筑，照抄老工程(E:\Projects\Forever_UE\Source\Basic\map\
// building_basic.h/.cpp的FactoryBuilding)。和ResidenceBuilding/ShopBuilding一样是"一个
// 本体独占一个mod实例"模型。Layout()完整照抄老工程FactoryBuilding::LayoutBuilding的楼层/
// 房间数据，详见building_basic.md。
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
