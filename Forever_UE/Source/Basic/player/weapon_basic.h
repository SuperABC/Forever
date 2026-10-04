#pragma once

#include "player/weapon_mod.h"

// 武器域默认内容——这次只给两把测试用的武器，验证WeaponMod/WeaponFactory这套机制本身+
// 数字键切枪+开火/换弹手感，不代表最终武器数值(见weapon_basic.md"耗时排查/后续"一节，
// 具体弹道/后坐力数值调优明确留给PIE里试)。
//
// firstPersonMeshPath这次留空——用户还没有下载导入武器模型（见weapon_system_plan.md
// "MVP测试简化"一节推荐的两个免费资源），Forever层UForeverWeaponComponent发现路径为空
// 会跳过挂mesh这一步，纯粹用来测开火/伤害/换弹逻辑本身，不影响手感验证。等资产导入后，
// 直接把实际路径填进这两个类的SetProperty()即可，不需要改其它任何代码。
class PistolWeapon : public WeaponMod {
public:
	PistolWeapon();

	static const char* GetId() { return "weapon_pistol"; }
	virtual const char* GetType() const override { return "weapon_pistol"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};

class RifleWeapon : public WeaponMod {
public:
	RifleWeapon();

	static const char* GetId() { return "weapon_rifle"; }
	virtual const char* GetType() const override { return "weapon_rifle"; }
	virtual const char* GetName() override;
	virtual void SetProperty() override;

private:
	static int count;
	int id;
	std::string name;
};
