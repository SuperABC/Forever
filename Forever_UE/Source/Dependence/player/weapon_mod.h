#pragma once

#include "common/mod_state.h"

#include <string>
#include <algorithm>

// 设计见weapon_mod.md:纯数据描述,开火/命中判定在Forever层的UForeverWeaponComponent里。
class WeaponMod {
public:
	WeaponMod() = default;
	virtual ~WeaponMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 子类在这里填下面的字段(不在构造函数里),由Core层Weapon::Weapon()创建实例后立刻调一次。
	virtual void SetProperty() {}

	// ---- 资产引用 ----
	// 武器模型UE软路径,以"_C"结尾=Actor Blueprint类,否则=单个StaticMesh;留空=没有模型(判定逻辑不受影响)。
	std::string firstPersonMeshPath;

	// ---- 挂载 ----
	// 固定挂载点(角色骨骼名,留空=挂在角色Root上);开火判定读mesh的实际世界坐标,与此字符串无关。
	std::string gripSocketName;

	// 瞄准锚点微调:在Core基础位置(weapon.cpp的kAimBase*)上的偏移,角色本地坐标,cm,X前/Y右/Z上。
	float attachOffsetX = 0.f;
	float attachOffsetY = 0.f;
	float attachOffsetZ = 0.f;

	// ---- 模型和锚点:两个锚点都是枪托(模型原点,+X朝前) ----
	float modelScale = 1.f;

	// "枪托"锚点在模型坐标系里的位置(缩放后,cm,X前/Y右/Z上),默认0=模型原点。
	float anchorOffsetX = 0.f;
	float anchorOffsetY = 0.f;
	float anchorOffsetZ = 0.f;

	// 枪口在模型坐标系里的位置(同anchorOffset坐标系),开火弹道debugLine从这里画出。
	float muzzleOffsetX = 0.f;
	float muzzleOffsetY = 0.f;
	float muzzleOffsetZ = 0.f;

	// 背枪锚点微调:在Core基础位置(weapon.cpp的kBackBase*)上的偏移,相对背枪骨骼,写右肩位置,左肩自动Y取反。
	float backOffsetX = 0.f;
	float backOffsetY = 0.f;
	float backOffsetZ = 0.f;

	// ---- 射击参数 ----
	float damage = 25.f;
	float fireRate = 0.15f;        // 两次开火最小间隔，秒
	bool fullAuto = false;         // false=每次按键最多打一发，true=按住连发
	float maxRange = 5000.f;       // UE单位(cm)
	float baseSpread = 1.5f;       // 散射半角，度——命中方向在瞄准方向为轴的这个半角圆锥内随机(0=没有散射)

	// ---- 弹药 ----
	int magazineCapacity = 12;
	float reloadDuration = 1.5f;   // 秒

	// 换弹时从Player背包按此type扣备弹(Player::ConsumeByType)。
	std::string ammoType;

	// ---- 后坐力:每次开火瞬间在范围内随机踢视角,不自动回正,见weapon_mod.md ----
	float recoilPitchMin = 0.3f;   // 每次开火向上踢的角度范围(度,UE约定Pitch +Up,两个值都为正才保证只往上踢)
	float recoilPitchMax = 0.5f;
	float recoilYawMin = -0.25f;   // 左右踢动范围(度,负=向左)
	float recoilYawMax = 0.25f;

	// 单发伤害怎么算——默认对maxRange做线性衰减，特殊武器可以override成不衰减/更复杂的曲线。
	virtual float ComputeDamage(float distance) const {
		if (maxRange <= 0.f) return damage;
		float falloff = 1.f - std::min(distance, maxRange) / maxRange;
		return damage * falloff;
	}

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
