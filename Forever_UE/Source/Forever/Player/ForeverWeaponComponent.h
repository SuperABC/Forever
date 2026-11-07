#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForeverWeaponComponent.generated.h"

class Weapon;

// 挂在AForeverCharacter上的武器组件：开火/换弹/切枪等高频路径全是纯C++调用，只有致死时才
// 跨到Story，见ForeverWeaponComponent.md。
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FOREVER_API UForeverWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UForeverWeaponComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 按WeaponFactory里注册的id切换武器；id未注册/未启用时静默保留当前武器。
	void EquipWeapon(const FString& weaponId);

	// 按下/松开开火键。单发武器只在按下瞬间打一发；全自动武器按住期间由TickComponent按冷却连发。
	void StartFire();
	void StopFire();

	// 手动换弹，备弹在换弹计时结束时才扣(见md)。
	void Reload();

	bool IsReloading() const { return bReloading; }
	int32 GetCurrentAmmo() const { return currentAmmo; }
	// 读档恢复弹匣余量(夹到[0, 弹匣容量])。
	void SetCurrentAmmo(int32 value) { currentAmmo = FMath::Clamp(value, 0, GetMagazineCapacity()); }
	int32 GetMagazineCapacity() const;

	// 手上是否激活了武器。
	bool HasWeapon() const { return currentWeapon != nullptr; }

	// 清空当前武器，不装备新的(切到空肩膀时手上要跟着空下来)。
	void ClearWeapon();

private:
	// 一次开火：瞄准/冷却/弹药检查通过后从摄像机打一条射线，命中市民则结算伤害，致死则广播事件。
	void Fire();

	// 独占持有；Weapon自己经Factory创建/销毁WeaponMod(跨DLL安全)，这一层普通delete即可。
	Weapon* currentWeapon = nullptr;

	int32 currentAmmo = 0;
	float lastFireTime = -1000.f;
	bool bWantsToFire = false;
	bool bReloading = false;
	float reloadEndTime = 0.f;
};
