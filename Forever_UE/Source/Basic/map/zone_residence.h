#pragma once

#include "map/zone_mod.h"

// ResidenceZone：Zone域默认内容，只有一个通用占位类型，用于验证"显式指定矩形"这条分配链路
// （Zone这次没有权重/CDF方式，见zone_mod.h）。目标面积照抄老工程ResidentialZone::LayoutZone
// 的常量20000.f（Quad::acreage换算系数两边工程完全一致，不需要单位换算）；暂时不按
// lot->GetArea()筛选地块类型，任意有边界路的lot一视同仁，等以后设计具体建筑/园区类型时再
// 按类型区分（照抄老工程ResidentialZone::ZoneAssigner那套筛选逻辑），详见zone_residence.md。
// Basic现在编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由Config/ModLoader在
// 运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
//
// 这次会话改回"一个本体独占一个mod实例"模型：Distribute()/explicitPlacements改成static
// Assign()，围墙/大门/内部道路/内部建筑的实际布局从Distribute()里搬进非static的Layout()，
// 寻址用的唯一名字计数器直接写在构造函数里。
//
// 类名从ZoneBasic改成ResidenceZone(连同building_residence.h/room_residence.h/
// component_residence.h一起)，是这次会话进入populace域时顺手给map域占位类起的真名字——
// 这几个类的实际内容(纯住宅小区场景)从一开始就是"住宅"语义，只是阶段3/4早期还没有更多
// 类型可以对比，暂时叫"Basic"，纯改名不改行为，见Source/Core/populace/populace.md。
class ResidenceZone : public ZoneMod {
public:
	// 构造函数只定死lastName(用static计数器)，和老工程ResidentialZone::count同款。
	ResidenceZone();

	static const char* GetId() { return "zone_residence"; }
	virtual const char* GetType() const override { return "zone_residence"; }
	virtual const char* GetName() override { return lastName.c_str(); }

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	virtual void Layout(int direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

private:
	std::string lastName;
	static int count;
};
