#pragma once

#include "map/zone_mod.h"
#include "map/zone_factory.h"
#include "map/geometry.h"

#include <string>

// Zone：持有一个具体ZoneMod实例，代表一块已经落地的Zone占位（继承Quad表示自己占据的矩形，
// 和Lot本身"继承Quad表示自己的矩形"是同一种写法）。这次不实现Zone内部再摆Building的递归
// 布局（关键设计决策1），所以只是一个footprint+类型的占位对象，详见map.md"InitZones"一节。
//
// 不自己存一份rotation——落地的Zone来自某个Lot的freeLots切出来的一块，freeLots全部继承同一个
// 顶层Lot的rotation(SplitWithPath产出的每一段都传了同一个rotation，见geometry.cpp)，所以
// GetRotation()直接转发parentLot->GetRotation()就是正确值，没必要在Zone自己身上再存一份
// 冗余拷贝（早前"照抄Lot自己加一个旋转角度"的做法多此一举，parentLot本来就有）。这意味着
// GetRotation()必须在SetParentLot(lot)之后调用才有意义，构造完/SetParentLot之前调用返回0。
class Zone : public Quad {
public:
	Zone() = delete;

	// @factory: zone工厂; @zoneId: zone静态类型标识(工厂里已注册的id)
	Zone(ZoneFactory* factory, const std::string& zoneId);
	~Zone();

	std::string GetType() const;
	std::string GetName() const;

	// 转发parentLot->GetRotation()；parentLot为空时返回0.f。
	float GetRotation() const;

	Lot* GetParentLot() const;
	void SetParentLot(Lot* lot);

private:
	ZoneMod* mod;
	ZoneFactory* factory;
	std::string type;
	std::string name;
	Lot* parentLot = nullptr;
};
