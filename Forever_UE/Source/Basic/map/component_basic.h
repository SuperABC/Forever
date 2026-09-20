#pragma once

#include "map/component_mod.h"


// ResidenceComponent/ShopComponent/FactoryComponent：Component域默认内容，三个都是trivial
// 占位(InitComponent逻辑本来就是空的，照抄老工程对应类型)，合并进同一份
// component_basic.h/.cpp(不再按residence/shop/plant各开一个文件)，和terrain_basic.h/.cpp里
// OceanTerrain/MountainTerrain合并的方式一样。
// Basic现在编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由Config/ModLoader在
// 运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
class ResidenceComponent : public ComponentMod {
public:
	static const char* GetId() { return "component_residence"; }
	virtual const char* GetType() const override { return "component_residence"; }
	virtual const char* GetName() override { return "ResidenceComponent"; }
};

class ShopComponent : public ComponentMod {
public:
	static const char* GetId() { return "component_shop"; }
	virtual const char* GetType() const override { return "component_shop"; }
	virtual const char* GetName() override { return "ShopComponent"; }
};

class FactoryComponent : public ComponentMod {
public:
	static const char* GetId() { return "component_factory"; }
	virtual const char* GetType() const override { return "component_factory"; }
	virtual const char* GetName() override { return "FactoryComponent"; }
};
