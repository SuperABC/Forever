#pragma once

#include "map/building_mod.h"

// 阶段3占位:trivial默认实现,真正的默认map内容目录留到阶段4从旧工程
// Basic/map/building_basic.h迁移,完整对照表见 Source/Basic/README.md。
// Basic现在编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由Config/ModLoader在
// 运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
class BuildingBasic : public BuildingMod {
public:
	static const char* GetId() { return "building_basic"; }
	virtual const char* GetType() const override { return "building_basic"; }
	virtual const char* GetName() override { return "BuildingBasic"; }
};
