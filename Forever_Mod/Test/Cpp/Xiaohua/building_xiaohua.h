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

private:
	std::string name;
};

class YizhongBuilding : public BuildingMod {
public:
	static const char* GetId() { return "yizhong"; }
	virtual const char* GetType() const override { return "yizhong"; }
	virtual const char* GetName() override { name = "yizhong"; return name.data(); }

private:
	std::string name;
};
