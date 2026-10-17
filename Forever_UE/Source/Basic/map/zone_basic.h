#pragma once

#include "map/zone_mod.h"


// ResidenceZone：Zone域默认内容，只有一个通用占位类型，用于验证"显式指定矩形"这条分配链路
// （Zone没有权重/CDF方式，见zone_mod.h）。目标面积固定常量20000.f（Quad::acreage换算系数
// 不需要单位换算）；暂时不按lot->GetArea()筛选地块类型，任意有边界路的lot一视同仁，等以后
// 设计具体建筑/园区类型时再按类型区分。
// Basic编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由Config/ModLoader在
// 运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
//
// "一个本体独占一个mod实例"模型：Distribute()/explicitPlacements是static Assign()，
// 围墙/大门/内部道路/内部建筑的实际布局在非static的Layout()里。
//
// Zone这个concept目前只有这一个默认实现，不像Building/Room/Component那样要合并多个具体
// 类型，但文件名统一按`<concept>_basic.h/.cpp`命名（和terrain_basic/roadnet_basic/
// building_basic/room_basic/component_basic一致），不叫`zone_residence`，让Basic/map下
// 每个concept都对应唯一一份`xxx_basic.h/.cpp`，见Source/Basic/README.md。
class ResidenceZone : public ZoneMod {
public:
	// 构造函数只记下id(用static计数器)。
	ResidenceZone();

	static const char* GetId() { return "zone_residence"; }
	virtual const char* GetType() const override { return "zone_residence"; }
	// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id，和terrain_basic同一个
	// 模式。
	virtual const char* GetName() override;

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	virtual void Layout(int direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;
	// 沿四面栅栏内侧每隔一段种一棵树，大门那一段让出来(按朝北坐标写，Core按大门方向旋转)。
	virtual void LayoutTrees(int direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

private:
	static int count;
	int id;
	std::string name;
};
