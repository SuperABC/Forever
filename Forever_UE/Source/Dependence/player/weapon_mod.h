#pragma once

#include <string>
#include <algorithm>

// WeaponMod：一把武器"打起来是什么样"的纯数据描述——伤害/射速/弹匣/挂载点/资产路径这些
// 字段直接是public成员（照抄BuildingMod的风格），没有Fire()/ApplyRecoil()这类虚方法。
//
// 这是和weapon_system_plan.md原始设计的一处刻意偏离：原计划把Fire()/ApplyRecoil()做成
// 虚方法、签名留到落地时再定，理由是"要接UE的碰撞查询/相机接口，Dependence层看不到UE类型"。
// 但既然连方法体都要在Forever层实现，不如干脆不在WeaponMod上放这两个虚方法——真正的开火/
// 命中判定整个放在Forever层的UForeverWeaponComponent里，直接读这里的纯数据字段（damage/
// maxRange/baseSpread/fireRate/magazineCapacity/reloadDuration）算，不需要WeaponMod
// 反过来调用UE接口。这样WeaponMod和BuildingMod::Layout()一样，只用Core类型，没有任何
// "签名留白等以后再定"的半成品。
//
// 这次范围明确不做：assetId(资产/背包关联)、ammoObjectId/maxReserveAmmo(备弹真实扣减)、
// 瞄准倍率/后坐力恢复这类需要专门调优的手感参数——见weapon_basic.md/CitizenElement.md
// 同类"先占位/先不做"的取舍记录。
class WeaponMod {
public:
	WeaponMod() = default;
	virtual ~WeaponMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// config.json里"weapon_mods"数组中该mod id后面的命令行式参数字符串，和其余concept
	// 同一个入口，这次没有武器需要参数化的字段，默认空实现即可。
	virtual void ApplyArgs(const std::string& args) {}

	// ---- 资产引用（纯数据，UE层按路径LoadObject）----
	// 一阶段占位：用户还没有下载导入武器模型，这里留空——Forever层SpawnWeaponMesh()发现
	// 路径为空时会跳过挂mesh这一步，但开火/伤害/换弹的判定逻辑完全不受影响，可以先测手感。
	std::string firstPersonMeshPath;

	// ---- 挂载 ----
	// 现阶段填固定挂载点（角色骨骼名，留空=直接挂在角色Root/胶囊体上）；以后有真实持枪动画时
	// 换成手部Socket名，语义不变——开火判定读的是mesh组件的实际世界坐标，不是这个字符串本身，
	// 换挂载点不需要碰开火逻辑。
	std::string gripSocketName;

	// ---- 射击参数 ----
	float damage = 25.f;
	float fireRate = 0.15f;        // 两次开火最小间隔，秒
	bool fullAuto = false;         // false=每次按键最多打一发，true=按住连发
	float maxRange = 5000.f;       // UE单位(cm)
	float baseSpread = 1.5f;       // 散射半角，度——命中方向在瞄准方向为轴的这个半角圆锥内随机

	// ---- 弹药（MVP阶段：备弹视为无限，见weapon_basic.md）----
	int magazineCapacity = 12;
	float reloadDuration = 1.5f;   // 秒

	// 单发伤害怎么算——默认对maxRange做线性衰减，特殊武器可以override成不衰减/更复杂的曲线。
	virtual float ComputeDamage(float distance) const {
		if (maxRange <= 0.f) return damage;
		float falloff = 1.f - std::min(distance, maxRange) / maxRange;
		return damage * falloff;
	}
};
