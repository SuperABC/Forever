#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForeverWeaponVisualComponent.generated.h"

class Asset;
class AForeverFrameworkActor;

// 武器可视化：肩膀槽位里的武器生成模型，不瞄准时背在背后，瞄准时激活的那把举到枪口位置。
// 只是视觉，详见ForeverWeaponVisualComponent.md。
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FOREVER_API UForeverWeaponVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UForeverWeaponVisualComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 开始/停止显示：玩家占有时开始(PossessedBy)，取消占有时停止并销毁全部模型(UnPossessed)。
	void BeginShowing();
	void EndShowing();

	// 当前激活那把枪的枪口世界坐标(WeaponMod::muzzleOffset)，供开火时画弹道debugLine。没有激活的枪模型时返回false。
	bool GetActiveMuzzleWorldLocation(FVector& OutLocation) const;

	// AForeverCharacter::ActivateShoulderWeapon调用：记录手上武器由哪个Asset装备，之后槽位里的Asset变了就自动重新激活。
	void NotifyActivated(int32 slot, Asset* asset);

	// ---- 位置/旋转：都是大概值，看效果再调 ----
	// 背枪挂的骨骼(UE5 Mannequin的脊柱中段)。
	UPROPERTY(EditAnywhere, Category = "Holster")
	FName holsterBone = TEXT("spine_03");

	// 背枪的姿势(模型+X枪口方向的朝向，角色朝向下的直观坐标，内部按骨骼参考姿势补偿)。
	// Roll绕枪管轴转让枪平着背：UE的Roll正方向实测和预想相反，左肩+90、右肩-90，方向反了就对调符号。
	// 枪托放哪由WeaponMod::backOffset决定。
	UPROPERTY(EditAnywhere, Category = "Holster")
	FRotator holsterRotationLeft = FRotator(85.f, -5.f, 90.f);
	UPROPERTY(EditAnywhere, Category = "Holster")
	FRotator holsterRotationRight = FRotator(85.f, 5.f, -90.f);

private:
	enum class EPose : uint8 { None, Holster, Aim };

	// 一个肩膀槽位当前显示的模型。Asset*是Core类型、不能UPROPERTY；Actor由World持有。
	struct FSlotVisual {
		Asset* asset = nullptr;
		TWeakObjectPtr<AActor> actor;
		float modelScale = 1.f;
		FVector muzzleLocal = FVector::ZeroVector; // 枪口在模型自己坐标系里的位置(缩放后、相对模型原点)，来自WeaponMod::muzzleOffset，只用于debugLine
		FVector anchor = FVector::ZeroVector; // 枪托锚点在模型自己坐标系里的位置(WeaponMod::anchorOffset)
		FVector aimOffset = FVector::ZeroVector; // 瞄准锚点(枪托)在角色本地的位置(WeaponMod::attachOffset)
		FVector backOffset = FVector::ZeroVector; // 背枪锚点(枪托)相对背枪骨骼(WeaponMod::backOffset，已按左右肩取反Y)
		EPose pose = EPose::None;
		bool bHiddenForOwner = false;
	};

	// 武器type(weapon_pistol)→模型路径+瞄准枪口位置，从WeaponMod读，缓存。
	struct FWeaponVisualDef {
		FString path;
		FVector aimOffset = FVector::ZeroVector;
		FVector backOffset = FVector::ZeroVector; // 右肩的位置，左肩取反Y
		FVector anchor = FVector::ZeroVector;
		FVector muzzle = FVector::ZeroVector; // WeaponMod::muzzleOffset
		float modelScale = 1.f;
	};

	// 数据源：这个角色左(1)/右(2)肩上的武器Asset，见md。
	Asset* GetShoulderAsset(int32 slot) const;

	const FWeaponVisualDef* FindDef(const FString& weaponType);
	AActor* SpawnVisualActor(const FString& path, float scale);

	void RebuildSlot(int32 index, Asset* asset);
	void DestroySlot(int32 index);
	void ApplyHolsterPose(int32 index);
	void ApplyAimPose(int32 index);
	void SetHiddenForOwner(FSlotVisual& visual, bool bHide);

	FSlotVisual slots[2];
	TMap<FString, FWeaponVisualDef> defCache;

	TWeakObjectPtr<AForeverFrameworkActor> framework;
	bool bShowing = false;

	// 手上武器是由哪个槽位/Asset装备出来的(NotifyActivated记录)，bTrackActivated=false表示还没激活过。
	bool bTrackActivated = false;
	Asset* activatedAsset = nullptr;
};
