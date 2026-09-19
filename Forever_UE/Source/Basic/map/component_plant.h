#pragma once

#include "map/component_mod.h"

// FactoryComponent：照抄老工程FactoryComponent，trivial占位，和ResidenceComponent同样
// 极简，见building_factory.md。
//
// 文件名是component_plant.h而不是component_factory.h：理由同room_plant.h顶部注释——避免
// 和Source/Dependence/map/component_factory.h(ComponentFactory注册表)同名冲突。类名/
// GetId()仍然是FactoryComponent/"component_factory"。
class FactoryComponent : public ComponentMod {
public:
	static const char* GetId() { return "component_factory"; }
	virtual const char* GetType() const override { return "component_factory"; }
	virtual const char* GetName() override { return "FactoryComponent"; }
};
