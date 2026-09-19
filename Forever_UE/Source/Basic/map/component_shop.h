#pragma once

#include "map/component_mod.h"

// ShopComponent：照抄老工程ShopComponent，trivial占位(InitComponent逻辑本来就是空的)，
// 和ResidenceComponent同样极简，见building_shop.md。
class ShopComponent : public ComponentMod {
public:
	static const char* GetId() { return "component_shop"; }
	virtual const char* GetType() const override { return "component_shop"; }
	virtual const char* GetName() override { return "ShopComponent"; }
};
