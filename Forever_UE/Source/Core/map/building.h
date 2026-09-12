#pragma once

#include "map/building_mod.h"
#include "map/building_factory.h"
#include "map/geometry.h"

#include <string>
#include <unordered_map>

class Zone;

// Building：持有一个具体BuildingMod实例，代表一栋已经落地的Building占位（继承Quad表示自己
// 占据的矩形）。这次不实现Room/Component布局，只是一个footprint+类型的占位对象，详见
// map.md"InitBuildings"一节。落地那一刻采样出来的面积直接体现在继承来的矩形尺寸上，不用
// 单独存一份。
//
// 仿照老工程、和Zone同一次迁移改成的模式（第十六轮迁移）：Building**不拥有**自己持有的这个
// mod实例的生命周期——`BuildingMod`的`candidateWeights`/`RandomAcreage`机制天然需要一个
// "按类型共享"的实例活过整个`Map::InitBuildings()`（同一个类型可能同时走显式占位、
// FillRemainder、Zone内部建筑三条路径，产出好几个Building，全部指向同一个mod实例），不是
// Zone那种"一个mod实例从一开始就只服务一个即将落地的对象"的独占关系，所以不能照抄Zone在
// 析构时`DestroyBuilding(mod)`——那样会在同类型的第二个Building析构时对同一个指针重复销毁。
// 这个mod实例的生命周期由`Map::InitBuildings()`自己的`scanners`表统一持有/销毁，`Building`
// 只是拿着一个不持有生命周期的观察指针，和`boundaryRoads`里的`Road*`是同一个道理。
// 不自己存一份rotation，直接转发parentLot->GetRotation()再叠加relativeRotation——普通(非
// 园区内部)building的relativeRotation恒为0，等价于原来纯转发的行为；园区内部building由
// Map::PlaceZoneInternalBuilding用SetParentLot(zone->GetParentLot(), spec.relativeRotation)
// 设置，parentLot直接复用zone自己的parentLot(所以转发基准天然和zone一致)，relativeRotation
// 是相对zone自身旋转的附加偏移。
class Building : public Quad {
public:
	Building() = delete;

	// @mod: 这个Building要挂靠的BuildingMod实例——不持有生命周期，由调用方(Map::InitBuildings()
	// 自己的scanners表)保证在这个Building存活期间一直有效、并负责销毁。
	explicit Building(BuildingMod* mod);
	~Building();

	std::string GetType() const;
	std::string GetName() const;

	// 持有(观察，不持有生命周期)的mod实例——以后需要读mod内部数据的调用方直接用，不需要
	// 另外拷贝，和Zone::GetMod()同一个用途，只是这里的mod是按类型共享的(见上)。
	BuildingMod* GetMod() const;

	// 转发parentLot->GetRotation()+relativeRotation；parentLot为空时按0.f+relativeRotation算。
	float GetRotation() const;

	Lot* GetParentLot() const;
	void SetParentLot(Lot* lot, float relativeRotation = 0.f);

	// 归属哪个Zone——只是纯粹的反向查询登记(以后"这个building在哪个园区里"之类的功能用)，
	// 不参与GetRotation()计算(旋转转发走的是parentLot那条链路，见上)。和parentLot同时设置，
	// 不是二选一：园区内部building两个都要设。
	Zone* GetParentZone() const;
	void SetParentZone(Zone* zone);

	// 四周边界Road：下标按FACE_DIRECTION(0-3)，和Lot::boundaryRoads语义完全一致，不持有
	// 指针生命周期。
	void SetBoundaryRoad(int direction, Road* road);
	Road* GetBoundaryRoad(int direction) const;
	const std::unordered_map<int, Road*>& GetBoundaryRoads() const;

private:
	BuildingMod* mod;
	std::string type;
	std::string name;
	Lot* parentLot = nullptr;
	Zone* parentZone = nullptr;
	float relativeRotation = 0.f;
	std::unordered_map<int, Road*> boundaryRoads;
};
