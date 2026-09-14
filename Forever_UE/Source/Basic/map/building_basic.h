#pragma once

#include "map/building_mod.h"

// BuildingBasic：Building域默认内容，只有一个通用占位类型，只走权重CDF随机填充路径（不使用
// 显式占位）。面积采样公式照抄老工程ResidentialBuilding原始数值；暂时不按lot->GetArea()筛选/
// 区分权重，任意lot统一给权重1，等以后设计具体建筑类型时再按类型区分，详见building_basic.md。
// Basic现在编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由Config/ModLoader在
// 运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
//
// 这次会话改回"一个本体独占一个mod实例"模型：RandomAcreage/GetAcreageMin/GetAcreageMax/
// GetPower/Assign全部改成不需要实例的static方法(通过BuildingFactory::RegisterBuilding注册)，
// 寻址用的唯一名字计数器可以像老工程一样直接写在构造函数里。
class BuildingBasic : public BuildingMod {
public:
	// 构造函数只定死lastName(用static计数器)，和老工程ResidentialZone::count同款——不设
	// footprint/楼层/lodMaterial，这些要等Layout()才知道落地上下文。
	BuildingBasic();

	static const char* GetId() { return "building_basic"; }
	virtual const char* GetType() const override { return "building_basic"; }
	virtual const char* GetName() override { return lastName.c_str(); }

	// 这里才设footprint/basements/layers/floorHeights，以及AssignFloor/AssignRoom/
	// ArrangeRow声明楼层内部布局。direction==-1(FillRemainder落地)时从boundaryRoads里
	// 随机挑一个有真实边界路的方向回写，保证这栋building最终有确定方向可用(行人导航的
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
	std::string lastName;
	static int count;
};
