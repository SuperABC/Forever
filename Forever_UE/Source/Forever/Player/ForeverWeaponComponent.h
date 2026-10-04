#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForeverWeaponComponent.generated.h"

class Weapon;
class UStaticMeshComponent;

// 挂在AForeverCharacter上的武器组件——开火/换弹/切枪这些"每帧/每次交互都要做"的高频路径
// 全部是纯C++调用，直接读WeaponMod的数据字段算，不经过Story/Change/Event（见
// ForeverWeaponComponent.md"高频/低频两条路径"一节，照抄weapon_system_plan.md原始设计的
// 这条核心原则）。只有命中导致Citizen死亡这一个"低频/叙事阈值"时刻会跨到Story
// （UForeverStoryFrameworkComponent::BroadcastCitizenDecease）。
//
// MVP范围（这次会话选定，见ForeverWeaponComponent.md）：hitscan开火+伤害+换弹+数字键切枪，
// 不做：通缉值、警察JobMod、反抗小游戏、ArrestPlayerChange。
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FOREVER_API UForeverWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UForeverWeaponComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 按WeaponFactory里注册的id切换到那把武器（比如"weapon_pistol"/"weapon_rifle"，见
	// Source/Basic/player/weapon_basic.h）——找不到该id、或该id没在config.json
	// "weapon_mods"数组里启用时，静默保留当前武器不变（和其余concept的CreateXxx失败处理
	// 惯例一致，不抛异常）。
	// @weaponId: WeaponMod::GetType()/静态GetId()对应的字符串
	void EquipWeapon(const FString& weaponId);

	// 按下/松开开火键。单发武器(WeaponMod::fullAuto为false)只在按下瞬间打一发(仍然受
	// fireRate冷却限制)；全自动武器按住期间TickComponent每帧检查冷却到了没有，到了就
	// 自动再打一发，直到StopFire()。
	void StartFire();
	void StopFire();

	// 手动换弹——换弹完成时从Player背包按WeaponMod::ammoType扣减整个弹匣容量的备弹，见.cpp
	// TickComponent换弹完成分支。
	void Reload();

	bool IsReloading() const { return bReloading; }
	int32 GetCurrentAmmo() const { return currentAmmo; }
	int32 GetMagazineCapacity() const;

	// 当前手上是不是空的(没有激活任何武器)——供AForeverCharacter::StartAim()判断能不能
	// 瞄准(手上没武器没法瞄准)，也供ActivateShoulderWeapon()判断要不要先清空。
	bool HasWeapon() const { return currentWeapon != nullptr; }

	// 清空当前武器(销毁mesh+WeaponMod实例)，不装备新的——供
	// AForeverCharacter::ActivateShoulderWeapon()在"切到的那个肩膀是空的"时调用：这时
	// 手上应该跟着空下来，不能继续保留切换前的那把武器。
	void ClearWeapon();

private:
	// 真正的一次开火：冷却/弹药检查通过后，从当前摄像机（第一/第三人称由
	// AForeverCharacter::IsFirstPerson()决定用哪个）沿视线方向(叠加baseSpread随机散射)
	// 打一条LineTraceSingleByChannel，命中ACitizenElement就调Citizen::TakeDamage()，
	// 致死则广播CitizenDeceaseEvent+通知UForeverPopulaceFrameworkComponent清理。
	void Fire();

	void SpawnWeaponMesh();
	void DestroyWeaponMesh();

	// 独占持有——Weapon(Core层wrapper，见Source/Core/player/weapon.h)构造时自己调
	// Registry::Get().GetWeaponFactory().CreateWeapon(id)，析构时自己调DestroyWeapon()
	// 释放(跨DLL new/delete安全，和其余XxxFactory同一个约定)，EndPlay/切枪时这一层只需要
	// 普通delete。
	Weapon* currentWeapon = nullptr;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> weaponMesh;

	int32 currentAmmo = 0;
	float lastFireTime = -1000.f;
	bool bWantsToFire = false;
	bool bReloading = false;
	float reloadEndTime = 0.f;
};
