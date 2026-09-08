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

private:
	std::string name;
};
