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
// 武器↔Asset背包系统靠"同一个id字符串"桥接，不靠字段关联——WeaponMod自己不持有assetId，
// 见Source/Core/player/player.h的ActivateShoulderWeapon()/Source/Core/player/asset.md。
// ammoType(备弹关联)见下方"弹药"一节。这次范围明确不做：瞄准倍率——见weapon_basic.md/
// CitizenElement.md同类"先占位/先不做"的取舍记录。后坐力这次已经做了，见下面"后坐力"字段组。
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

	// 挂载点(gripSocketName对应的骨骼socket，或者留空时的角色Root)基础上的本地偏移——
	// 没有真实持枪socket之前，直接挂Root会让枪的可视网格出现在角色身体正中心(实测卡在了
	// 胯部)，这三个字段就是用来把它挪到看起来合理的位置(比如右肩膀靠前一点)。单位和UE一致
	// (cm)，X前/Y右/Z上，是角色本地坐标系下的相对偏移，不是世界坐标。这次没有做旋转偏移——
	// 用户只要求挪位置，没有要求调朝向。这个偏移同时决定了子弹轨迹debugLine的视觉起点，见
	// UForeverWeaponComponent::Fire()："gunLocation"直接读挂了这个偏移之后的
	// weaponMesh->GetComponentLocation()。
	float attachOffsetX = 0.f;
	float attachOffsetY = 0.f;
	float attachOffsetZ = 0.f;

	// ---- 射击参数 ----
	float damage = 25.f;
	float fireRate = 0.15f;        // 两次开火最小间隔，秒
	bool fullAuto = false;         // false=每次按键最多打一发，true=按住连发
	float maxRange = 5000.f;       // UE单位(cm)
	float baseSpread = 1.5f;       // 散射半角，度——命中方向在瞄准方向为轴的这个半角圆锥内随机

	// ---- 弹药 ----
	int magazineCapacity = 12;
	float reloadDuration = 1.5f;   // 秒

	// 换弹时从Player背包按这个type扣减备弹(Player::ConsumeByType)，比如"ammo_pistol"——
	// 这正是本文件之前"这次不做的ammoObjectId"，现在Asset背包系统落地后补上，见
	// UForeverWeaponComponent::TickComponent换弹完成分支。
	std::string ammoType;

	// ---- 后坐力（手感仿PUBG：每次开火瞬间踢一下视角，不会自动回正，全靠玩家自己压枪/
	// 甩枪抵消——PUBG本身也没有"松开鼠标后视角自动回到开火前"这种机制，持续连发时准心会
	// 一直往上/往两边走，直到玩家主动把鼠标往反方向拉。单发/连发都会踢，因为两者最终都走
	// 同一个Fire()调用，见UForeverWeaponComponent::Fire()"后坐力"一节）----
	float recoilPitchMin = 0.3f;   // 每次开火向上踢的角度范围下限，度。UE的FRotator约定
	float recoilPitchMax = 0.5f;   // Pitch是"+Up，-Down"，所以这两个是正数，不是负数——
	                                // 每次开火在[recoilPitchMin, recoilPitchMax]里随机取一个
	                                // 值，不是固定踢同样的角度，模拟每发后坐力不完全一致的
	                                // 手感。两个值都给正数、且都>0，保证垂直方向始终是往上踢，
	                                // 不会随机踢成往下（真实后坐力也不会把枪口往下压）。
	float recoilYawMin = -0.25f;   // 每次开火左右方向踢动范围下限，度(负=向左)。
	float recoilYawMax = 0.25f;    // 每次开火左右方向踢动范围上限，度(正=向右)——和Pitch
	                                // 同样每次开火随机取一个值，不是固定往一个方向偏，模拟
	                                // PUBG"左右小幅度随机漂移+整体向上"的手感，不是精确复刻
	                                // 某支枪的真实后坐力轨迹。

	// 单发伤害怎么算——默认对maxRange做线性衰减，特殊武器可以override成不衰减/更复杂的曲线。
	virtual float ComputeDamage(float distance) const {
		if (maxRange <= 0.f) return damage;
		float falloff = 1.f - std::min(distance, maxRange) / maxRange;
		return damage * falloff;
	}
};
