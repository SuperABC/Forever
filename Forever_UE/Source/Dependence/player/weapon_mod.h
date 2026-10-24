#pragma once

#include "common/mod_state.h"

#include <string>
#include <algorithm>

// WeaponMod：一把武器"打起来是什么样"的纯数据描述——伤害/射速/弹匣/挂载点/资产路径这些
// 字段直接是public成员（照抄BuildingMod的风格），没有Fire()/ApplyRecoil()这类虚方法：
// 真正的开火/命中判定整个放在Forever层的UForeverWeaponComponent里，直接读这里的纯数据
// 字段（damage/maxRange/baseSpread/fireRate/magazineCapacity/reloadDuration）算，不需要
// WeaponMod反过来调用UE接口。WeaponMod和BuildingMod::Layout()一样，只用Core类型，没有
// "签名留白等以后再定"的半成品方法。
//
// 武器↔Asset背包系统靠"同一个id字符串"桥接，不靠字段关联——WeaponMod自己不持有assetId，
// 见Source/Core/player/player.h的ActivateShoulderWeapon()/Source/Core/player/asset.md。
// ammoType(备弹关联)见下方"弹药"一节。瞄准倍率明确不做——见weapon_basic.md/
// CitizenElement.md同类"先占位/先不做"的取舍记录。
class WeaponMod {
public:
	WeaponMod() = default;
	virtual ~WeaponMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 具体子类必须在这里填下面这些字段，不要在构造函数里赋值——构造函数只负责id/count
	// 这类登记，和StorageMod::SetProperty()/AssetMod::SetProperty()同一套"两段式"约定。
	// 调用点在Core层的Weapon::Weapon()构造函数(Source/Core/player/weapon.h/.cpp，和
	// Asset::Asset()同一个"创建完mod实例后立刻调一次、再读字段"时机)，不是Forever层。
	virtual void SetProperty() {}

	// ---- 资产引用（纯数据，UE层按路径加载）----
	// 武器模型路径，UE软路径。以"_C"结尾=Actor Blueprint类(整件武器，比如枪体+弹匣的相对位置
	// 在蓝图里，如"/Game/.../AC_pistol_001.AC_pistol_001_C")；否则=单个StaticMesh。留空=没有模型，
	// UForeverWeaponVisualComponent什么都不显示，但开火/伤害/换弹的判定逻辑完全不受影响。
	// 肩膀上有这把武器就显示(背在背后)，瞄准时举到attachOffset处，见
	// Source/Forever/Player/ForeverWeaponVisualComponent.md。
	std::string firstPersonMeshPath;

	// ---- 挂载 ----
	// 固定挂载点（角色骨骼名，留空=直接挂在角色Root/胶囊体上）；以后有真实持枪动画时换成
	// 手部Socket名，语义不变——开火判定读的是mesh组件的实际世界坐标，不是这个字符串本身，
	// 换挂载点不需要碰开火逻辑。
	std::string gripSocketName;

	// 瞄准锚点的微调：瞄准时枪托摆放的位置，基础位置(右手靠前一点)由Core决定
	// (Source/Core/player/weapon.cpp的kAimBase*)，这里写的是在基础位置上的偏移，不填(0)就是基础位置。
	// 相对角色根(胶囊)，角色本地坐标，单位cm，X前/Y右/Z上，不含旋转偏移。枪体从枪托向前延伸(枪随视线
	// 俯仰，绕枪托转)。不瞄准时枪背在背后，位置由下面的backOffset决定。
	float attachOffsetX = 0.f;
	float attachOffsetY = 0.f;
	float attachOffsetZ = 0.f;

	// ---- 模型和锚点：两个锚点都是枪托(模型原点，资源包的模型轴心在枪托/握把附近，+X朝前) ----
	// 模型缩放(第三方模型不一定是真实大小)。
	float modelScale = 1.f;

	// "枪托"这个锚点在模型自己坐标系里的位置：相对模型原点的偏移，cm，缩放之后的尺寸，X前/Y右/Z上。
	// 默认0=锚点就是模型原点。背枪锚点(backOffset)和瞄准锚点(attachOffset)都是把"这个点"放到
	// 对应位置，所以往下(-Z)挪锚点=枪整体显示得更高，往上(+Z)=更低，往枪托方向(-X)=枪更靠前。
	float anchorOffsetX = 0.f;
	float anchorOffsetY = 0.f;
	float anchorOffsetZ = 0.f;

	// 枪口在模型自己坐标系里的位置：相对模型原点，cm，缩放之后的尺寸，X前/Y右/Z上(和anchorOffset
	// 同一个坐标系，不是相对锚点)。开火时的弹道debugLine从这里画出去。
	float muzzleOffsetX = 0.f;
	float muzzleOffsetY = 0.f;
	float muzzleOffsetZ = 0.f;

	// 背枪锚点的微调：不瞄准时枪托放在哪，基础位置(肩胛附近)由Core决定(weapon.cpp的kBackBase*)，
	// 这里写的是在基础位置上的偏移，不填(0)就是基础位置。相对背枪骨骼(组件的holsterBone)，角色朝向下的
	// 坐标(cm，X前/Y右/Z上)。写的是右肩的位置，左肩上的枪自动把Y取反(左右对称)。
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

	// 换弹时从Player背包按这个type扣减备弹(Player::ConsumeByType)，比如"asset_ammo_pistol"，见
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

	// ---- 存档(见Source/Dependence/common/mod_state.md) ----
	// mod私有数据的存档/读档钩子，默认不存任何东西。基类里的公共声明字段由Core直接快照，
	// 这里只放mod自己的私有数据。
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	// SaveState写出数据的格式版本：改了自己SaveState的格式就加一，读档时从
	// StateReader::GetSavedVersion()拿到当时的版本自己迁移。默认0。
	virtual int GetStateVersion() const { return 0; }
	// 这个mod类用来生成唯一实例名的静态计数器——存档保存、读档后恢复，避免读档后新建的实例和
	// 恢复出来的名字撞车。没有计数器的类保持默认(返回-1、Set不做事)。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
