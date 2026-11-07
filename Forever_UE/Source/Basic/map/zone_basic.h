#pragma once

#include "map/zone_mod.h"


// 带围墙/大门/内部道路的园区测试场景，详见zone_basic.md。
class ResidenceZone : public ZoneMod {
public:
	ResidenceZone();

	static const char* GetId() { return "zone_residence"; }
	virtual const char* GetType() const override { return "zone_residence"; }
	virtual const char* GetName() override;

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context);
	virtual void Layout(int direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;
	// 沿四面栅栏内侧每隔一段种一棵树，大门那一段让出来(按朝北坐标写，Core按大门方向旋转)。
	virtual void LayoutTrees(int direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) override;

public:
	// 存档保存/恢复下面这个命名计数器(count)，见Source/Dependence/common/mod_state.md。
	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }
private:
	static int count;
	int id;
	std::string name;
};
