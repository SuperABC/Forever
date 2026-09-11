#pragma once

#include <string>

#include "map/building_mod.h"

// 阶段3示例mod:只实现GetId/GetType/GetName三个身份接口,用于验证Mod发现/加载/
// 注册链路。真正的建筑玩法逻辑留到阶段4迁移map系统时,对照旧工程
// E:\Projects\Forever_Mods\Test\Cpp\Yuanshen\building_yuanshen.h 补上。
class YuanshenBuilding : public BuildingMod {
public:
	static const char* GetId() { return "yuanshen"; }
	virtual const char* GetType() const override { return "yuanshen"; }
	virtual const char* GetName() override { name = "yuanshen"; return name.data(); }

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
