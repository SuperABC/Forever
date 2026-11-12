#pragma once

#include "map/zone_mod.h"


// 带围墙/大门/内部道路的住宅园区，高/中/低三个密度各一个独立的mod，详见zone_basic.md。
// 三个类完全独立实现、没有共用基类，逻辑目前是同一份的拷贝，以后各自改。

// high密度住宅园区：只在high密度住宅区(AREA_RESIDENCE_HIGH)的lot里放，园区里放两栋high密度住宅楼。
class ResidenceHighZone : public ZoneMod {
public:
	ResidenceHighZone();

	static const char* GetId() { return "zone_residence_high"; }
	virtual const char* GetType() const override { return "zone_residence_high"; }
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

// middle密度住宅园区：只在middle密度住宅区(AREA_RESIDENCE_MIDDLE)的lot里放，园区里放两栋middle密度住宅楼。
class ResidenceMiddleZone : public ZoneMod {
public:
	ResidenceMiddleZone();

	static const char* GetId() { return "zone_residence_middle"; }
	virtual const char* GetType() const override { return "zone_residence_middle"; }
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

// low密度住宅园区：只在low密度住宅区(AREA_RESIDENCE_LOW)的lot里放，园区里放两栋low密度住宅楼。
class ResidenceLowZone : public ZoneMod {
public:
	ResidenceLowZone();

	static const char* GetId() { return "zone_residence_low"; }
	virtual const char* GetType() const override { return "zone_residence_low"; }
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
