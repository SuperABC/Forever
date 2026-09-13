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
// 仿照老工程、和Zone同一个模式：Building独占持有一个mod实例的生命周期，构造时创建、析构时
// factory->DestroyBuilding(mod)——这次会话撤销了早前"按类型共享"的设计（当时是为了
// candidateWeights/RandomAcreage按类型共享而引入的，现在RandomAcreage等已经改成不需要实例的
// static方法，见building_mod.h，共享模型不再必要，改回独占反而让"寻址唯一性计数器写在mod
// 构造函数里"这套和老工程一致的机制能正确工作）。
//
// 不自己存一份rotation，直接转发parentLot->GetRotation()再叠加relativeRotation——普通(非
// 园区内部)building的relativeRotation恒为0，等价于原来纯转发的行为；园区内部building由
// Map::PlaceZoneInternalBuilding用SetParentLot(zone->GetParentLot(), spec.relativeRotation)
// 设置，parentLot直接复用zone自己的parentLot(所以转发基准天然和zone一致)，relativeRotation
// 是相对zone自身旋转的附加偏移。
class Building : public Quad {
public:
	Building() = delete;

	// @factory: building工厂(用于~Building()里DestroyBuilding); @mod: 这个Building独占持有的
	// mod实例(调用方保证不会再有别的Building共用同一个mod指针)。
	Building(BuildingFactory* factory, BuildingMod* mod);
	~Building();

	std::string GetType() const;
	std::string GetName() const;

	// 这个Building持有的mod实例——不需要另外拷贝一份。
	BuildingMod* GetMod() const;

	// 调用方在SetPosition/SetBoundaryRoad都设好之后调用一次：内部先调
	// mod->Layout(direction, *this, GetBoundaryRoads())（this已经是Quad、边界路也已经是真实
	// 数据），再把mod->footprint/basements/layers/floorHeights/lodMaterial解析成Building自己
	// 的绝对(相对自身中心)数值并缓存。direction对显式占位落地是Assign选中的真实方向，对权重
	// CDF/FillRemainder落地传-1（没有方向概念），对园区内部建筑传spec.direction。
	void Layout(int direction);

	float GetBodyOffsetX() const; // 楼体中心相对Building自身中心的偏移(地图单位，未旋转局部坐标)
	float GetBodyOffsetY() const;
	float GetBodySizeX() const;   // 楼体绝对尺寸(地图单位)
	float GetBodySizeY() const;
	int GetBasementCount() const;
	int GetLayerCount() const;
	const std::vector<float>& GetFloorHeights() const; // 长度basements+layers，从下到上
	const std::string& GetLodMaterialPath() const;

	// 转发parentLot->GetRotation()+relativeRotation；parentLot为空时按0.f+relativeRotation算。
	float GetRotation() const;

	Lot* GetParentLot() const;
	void SetParentLot(Lot* lot, float relativeRotation = 0.f);

	// 归属哪个Zone——只是纯粹的反向查询登记(以后"这个建筑在哪个园区里"之类的功能用)，
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
	BuildingFactory* factory;
	std::string type;
	std::string name;
	Lot* parentLot = nullptr;
	Zone* parentZone = nullptr;
	float relativeRotation = 0.f;
	std::unordered_map<int, Road*> boundaryRoads;

	float bodyOffsetX = 0.f;
	float bodyOffsetY = 0.f;
	float bodySizeX = 0.f;
	float bodySizeY = 0.f;
	int basements = 0;
	int layers = 1;
	std::vector<float> floorHeights;
	std::string lodMaterialPath;
};
