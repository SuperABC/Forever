#pragma once

#include <string>

#include "map/building_mod.h"

// 阶段3示例mod:只实现GetId/GetType/GetName三个身份接口,用于验证Mod发现/加载/
// 注册链路。真正的建筑玩法逻辑(RandomAcreage/PlaceConstruction/LayoutBuilding/
// PlacePivots/GetPowers/BuildingAssigner等)留到阶段4迁移map系统时,对照旧工程
// E:\Projects\Forever_Mods\Test\Cpp\Xiaohua\building_xiaohua.h 补上。
class PengzhanBuilding : public BuildingMod {
public:
	static const char* GetId() { return "pengzhan"; }
	virtual const char* GetType() const override { return "pengzhan"; }
	virtual const char* GetName() override { name = "pengzhan"; return name.data(); }

	// 阶段4-1 Zone/Building落地时BuildingMod新增了这四个纯虚接口,这里先补最小实现避免
	// 抽象类无法实例化/vtable残缺,真正的Distribute/RandomAcreage玩法逻辑仍然留到对照旧工程
	// 补上,见上方注释。
	virtual void Distribute(const std::vector<Lot*>& lots) override {}
	virtual float RandomAcreage() override { return 0.f; }
	virtual float GetAcreageMin() override { return 0.f; }
	virtual float GetAcreageMax() override { return 0.f; }

private:
	std::string name;
};

class YizhongBuilding : public BuildingMod {
public:
	static const char* GetId() { return "yizhong"; }
	virtual const char* GetType() const override { return "yizhong"; }
	virtual const char* GetName() override { name = "yizhong"; return name.data(); }

	virtual void Distribute(const std::vector<Lot*>& lots) override {}
	virtual float RandomAcreage() override { return 0.f; }
	virtual float GetAcreageMin() override { return 0.f; }
	virtual float GetAcreageMax() override { return 0.f; }

private:
	std::string name;
};
