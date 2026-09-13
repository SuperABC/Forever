#pragma once

#include <string>

#include "map/building_mod.h"

// 阶段3示例mod:只实现GetId/GetType/GetName三个身份接口,用于验证Mod发现/加载/
// 注册链路。真正的建筑玩法逻辑(PlaceConstruction/LayoutBuilding/PlacePivots/
// GetPowers/BuildingAssigner等)留到阶段4迁移map系统时,对照旧工程
// E:\Projects\Forever_Mods\Test\Cpp\Xiaohua\building_xiaohua.h 补上。
//
// 这次会话改回"一个本体独占一个mod实例"模型：RandomAcreage/GetAcreageMin/GetAcreageMax/
// GetPower/Assign全部改成不需要实例的static方法（纯占位，返回0.f/空实现，不涉及任何真实
// 行为迁移），寻址用的唯一名字计数器直接写在构造函数里。
class PengzhanBuilding : public BuildingMod {
public:
	PengzhanBuilding() { lastName = std::string("pengzhan") + std::to_string(count++); }

	static const char* GetId() { return "pengzhan"; }
	virtual const char* GetType() const override { return "pengzhan"; }
	virtual const char* GetName() override { return lastName.c_str(); }

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {}
	static float RandomAcreage() { return 0.f; }
	static float GetAcreageMin() { return 0.f; }
	static float GetAcreageMax() { return 0.f; }
	static float GetPower(AREA_TYPE area) { return 0.f; }

private:
	std::string lastName;
	static int count;
};

class YizhongBuilding : public BuildingMod {
public:
	YizhongBuilding() { lastName = std::string("yizhong") + std::to_string(count++); }

	static const char* GetId() { return "yizhong"; }
	virtual const char* GetType() const override { return "yizhong"; }
	virtual const char* GetName() override { return lastName.c_str(); }

	static void Assign(const std::vector<Lot*>& lots, PlacementEmitFunc emit, void* context) {}
	static float RandomAcreage() { return 0.f; }
	static float GetAcreageMin() { return 0.f; }
	static float GetAcreageMax() { return 0.f; }
	static float GetPower(AREA_TYPE area) { return 0.f; }

private:
	std::string lastName;
	static int count;
};
