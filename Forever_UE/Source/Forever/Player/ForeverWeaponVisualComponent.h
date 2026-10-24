#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForeverWeaponVisualComponent.generated.h"

class Asset;
class AForeverFrameworkActor;

// 武器可视化：左/右肩槽位里有武器就生成模型、取下就删除；不瞄准时背在背后(绑在脊柱骨骼上，
// 跟着动画走)，瞄准时当前激活的那把枪移到右手靠前(WeaponMod::attachOffset，原来的弹道视觉起点)，
// 枪口朝摄像机瞄准方向。只是视觉：开火判定仍然是UForeverWeaponComponent::Fire()从摄像机射出。
// 详见ForeverWeaponVisualComponent.md。
//
// 作为AForeverCharacter的常驻组件(市民以后装备武器也复用)，组件自己不关心"是谁的武器"：
// 武器从GetShoulderAsset()取，现在的实现是"被玩家占有时读Player的肩膀，否则没有"，以后市民
// 有自己的装备时改这一个函数。组件默认不Tick，BeginShowing()才打开。
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

	// 当前激活(瞄准持枪)那把枪的枪口世界坐标，供开火时画弹道debugLine检查"弹道是不是从枪口出来"。
	// 枪口=模型联合包围盒在+X方向的最远点。没有激活的枪模型时返回false。
	bool GetActiveMuzzleWorldLocation(FVector& OutLocation) const;

	// AForeverCharacter::ActivateShoulderWeapon调用：记录"手上武器是由哪个Asset装备出来的"，
	// 之后肩膀里这个槽位的Asset变了(拖走/换成别的)就自动重新ActivateShoulderWeapon。
	void NotifyActivated(int32 slot, Asset* asset);

	// ---- 位置/旋转：都是大概值，看效果再调 ----
	// 背枪挂的骨骼(UE5 Mannequin的脊柱中段)。
	UPROPERTY(EditAnywhere, Category = "Holster")
	FName holsterBone = TEXT("spine_03");

	// 背枪的姿势(旋转)：模型+X(枪口方向)的朝向，用角色朝向下的直观坐标，内部按骨骼参考姿势补偿，
	// 和AForeverCharacter::PlaceAvatar同一个办法。Pitch=60表示枪口朝上偏前。Roll是绕模型自己的X轴
	// (枪管轴)转，FRotator合成时先转Roll，所以是在物体自己坐标系里转：左肩+90、右肩-90(UE的Roll正方向实测和预想相反，
	// 即从枪托往枪口看右肩顺时针、左肩逆时针对应的Roll符号)，让枪平着背在肩上而不是立着；方向反了就把两个Roll的符号对调。枪托放哪(背枪锚点)由
	// WeaponMod::backOffset决定，不在这里。
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

	// 数据源：这个角色左(1)/右(2)肩上的武器Asset。现在只有被玩家占有的角色有(Player的肩膀)，
	// 以后市民有自己的装备时改这里。
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
