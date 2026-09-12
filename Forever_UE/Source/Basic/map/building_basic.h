#pragma once

#include "map/building_mod.h"

// BuildingBasic：Building域默认内容，只有一个通用占位类型，只走权重CDF随机填充路径（不使用
// explicitPlacements）。面积采样公式照抄老工程ResidentialBuilding原始数值；暂时不按
// lot->GetArea()筛选/区分权重，任意lot统一给权重1，等以后设计具体建筑类型时再按类型区分，
// 详见building_basic.md。Basic现在编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由
// Config/ModLoader在运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
class BuildingBasic : public BuildingMod {
public:
	static const char* GetId() { return "building_basic"; }
	virtual const char* GetType() const override { return "building_basic"; }
	virtual const char* GetName() override { return "BuildingBasic"; }

	virtual void Distribute(const std::vector<Lot*>& lots) override;
	virtual float RandomAcreage() override;
	virtual float GetAcreageMin() override;
	virtual float GetAcreageMax() override;
};
