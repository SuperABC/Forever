#pragma once

#include "map/building_mod.h"
#include "map/building_factory.h"
#include "map/geometry.h"

#include <string>

// Building：持有一个具体BuildingMod实例，代表一栋已经落地的Building占位（继承Quad表示自己
// 占据的矩形）。这次不实现Room/Component布局，只是一个footprint+类型的占位对象，详见
// map.md"InitBuildings"一节。落地那一刻采样出来的面积直接体现在继承来的矩形尺寸上，不用
// 单独存一份。
// 不自己存一份rotation，直接转发parentLot->GetRotation()，理由见zone.h同名字段注释。
class Building : public Quad {
public:
	Building() = delete;

	// @factory: building工厂; @buildingId: building静态类型标识(工厂里已注册的id)
	Building(BuildingFactory* factory, const std::string& buildingId);
	~Building();

	std::string GetType() const;
	std::string GetName() const;

	// 转发parentLot->GetRotation()；parentLot为空时返回0.f。
	float GetRotation() const;

	Lot* GetParentLot() const;
	void SetParentLot(Lot* lot);

private:
	BuildingMod* mod;
	BuildingFactory* factory;
	std::string type;
	std::string name;
	Lot* parentLot = nullptr;
};
