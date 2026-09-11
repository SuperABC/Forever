#pragma once

#include "map/zone_mod.h"
#include "map/zone_factory.h"
#include "map/geometry.h"

#include <string>

// Zone：持有一个具体ZoneMod实例，代表一块已经落地的Zone占位（继承Quad表示自己占据的矩形，
// 和Lot本身"继承Quad表示自己的矩形"是同一种写法）。这次不实现Zone内部再摆Building的递归
// 布局（关键设计决策1），所以只是一个footprint+类型的占位对象，详见map.md"InitZones"一节。
//
// rotation不是继承自Quad(Quad本身没有旋转)，是照抄Lot"在Quad基础上自己加一个旋转角度"的
// 同款做法单独补上的——落地的Zone来自某个Lot的freeLots切出来的一块，freeLots全部继承同一个
// 顶层Lot的rotation(SplitWithPath产出的每一段都传了同一个rotation，见geometry.cpp)，
// 所以调用方(Map::InitZones)直接从request.lot->GetRotation()取值就是正确的，不需要
// Lot::RequestPlacement返回值额外带一份。
class Zone : public Quad {
public:
	Zone() = delete;

	// @factory: zone工厂; @zoneId: zone静态类型标识(工厂里已注册的id)
	Zone(ZoneFactory* factory, const std::string& zoneId);
	~Zone();

	std::string GetType() const;
	std::string GetName() const;

	float GetRotation() const;
	void SetRotation(float r);

	Lot* GetParentLot() const;
	void SetParentLot(Lot* lot);

private:
	ZoneMod* mod;
	ZoneFactory* factory;
	std::string type;
	std::string name;
	float rotation = 0.f;
	Lot* parentLot = nullptr;
};
