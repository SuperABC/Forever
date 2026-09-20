#pragma once

#include "map/component_mod.h"

#include <string>


// ResidenceComponent/ShopComponent/FactoryComponent：Component域默认内容，三个都是trivial
// 占位(InitComponent逻辑本来就是空的，照抄老工程对应类型)，合并进同一份
// component_basic.h/.cpp(不再按residence/shop/plant各开一个文件)，和terrain_basic.h/.cpp里
// OceanTerrain/MountainTerrain合并的方式一样。
// Basic现在编译为DynamicLibrary(Basic.dll),和Forever_Mod下的Mod一样由Config/ModLoader在
// 运行时扫描加载,不会静态链进Forever.Build.cs,见 Source/Basic/README.md。
//
// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id+GetName里现拼，和
// Source/Basic/map/terrain_basic.h/.cpp的OceanTerrain同一个模式(之前这里是固定字符串，
// 没有任何唯一性)。
class ResidenceComponent : public ComponentMod {
public:
	ResidenceComponent();

	static const char* GetId() { return "component_residence"; }
	virtual const char* GetType() const override { return "component_residence"; }
	virtual const char* GetName() override;

private:
	static int count;
	int id;
	std::string name;
};

class ShopComponent : public ComponentMod {
public:
	ShopComponent();

	static const char* GetId() { return "component_shop"; }
	virtual const char* GetType() const override { return "component_shop"; }
	virtual const char* GetName() override;

private:
	static int count;
	int id;
	std::string name;
};

class FactoryComponent : public ComponentMod {
public:
	FactoryComponent();

	static const char* GetId() { return "component_factory"; }
	virtual const char* GetType() const override { return "component_factory"; }
	virtual const char* GetName() override;

private:
	static int count;
	int id;
	std::string name;
};
