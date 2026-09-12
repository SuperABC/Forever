#pragma once

#include "map/zone_mod.h"

// ZoneBasic：Zone域默认内容，只有一个通用占位类型，用于验证"显式指定矩形"这条分配链路
// （Zone这次没有权重/CDF方式，见zone_mod.h）。目标面积照抄老工程ResidentialZone::LayoutZone
// 的常量20000.f（Quad::acreage换算系数两边工程完全一致，不需要单位换算）；暂时不按
// lot->GetArea()筛选地块类型，任意有边界路的lot一视同仁，等以后设计具体建筑/园区类型时再
// 按类型区分（照抄老工程ResidentialZone::ZoneAssigner那套筛选逻辑），详见zone_basic.md。
// Basic现在编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由Config/ModLoader在
// 运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
class ZoneBasic : public ZoneMod {
public:
	static const char* GetId() { return "zone_basic"; }
	virtual const char* GetType() const override { return "zone_basic"; }
	virtual const char* GetName() override { return "ZoneBasic"; }

	virtual void Distribute(const std::vector<Lot*>& lots) override;
};
