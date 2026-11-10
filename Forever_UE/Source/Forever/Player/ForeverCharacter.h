#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ForeverCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
class UForeverWeaponComponent;
class UForeverThrowableComponent;
class UForeverWeaponVisualComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;

UCLASS()
class FOREVER_API AForeverCharacter : public ACharacter
{
	GENERATED_BODY()

	// 投掷物激活/用完/手不够时要直接开关瞄准状态(StartAim/StopAim是protected)。
	friend class UForeverThrowableComponent;
	// 肩膀里的武器被拖走/换掉时要重新ActivateShoulderWeapon(见ForeverWeaponVisualComponent.cpp)。
	friend class UForeverWeaponVisualComponent;

public:
	// 按性别换身体网格(男Manny/女Quinn)，并重新摆放头像方块。由ACitizenElement::Init调用。
	void ApplyBodyMesh(bool bFemale);

	// 换头像贴图(市民按各自的头像文件贴)，见UForeverAvatarSubsystem。
	void SetAvatarTexture(UTexture2D* texture);

	AForeverCharacter();

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& value);
	void Look(const FInputActionValue& value);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> cameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> followCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> firstPersonCamera;

	// 头像方块：用来区分长得一样的角色，见ForeverCharacter.md的头像方块一节。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> avatarMesh;

	// 身体网格：男Manny、女Quinn(同一骨骼，共用动画蓝图)，ApplyBodyMesh按性别切换。
	UPROPERTY(EditDefaultsOnly, Category = "Mesh")
	TObjectPtr<USkeletalMesh> maleBodyMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Mesh")
	TObjectPtr<USkeletalMesh> femaleBodyMesh;

	// 按骨骼参考姿势摆放头像方块(avatarOffset/avatarRotation等)，换身体网格后要重新摆。
	void PlaceAvatar();

	// 方块边长(厘米)；avatarOffset相对head骨骼(骨骼局部坐标)；avatarRotation相对角色本身(PlaceAvatar抵消骨骼朝向，0=+X面朝正前)。
	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	float avatarSize = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	FVector avatarOffset = FVector(10.f, 0.f, 0.f);

	// 在avatarOffset基础上按角色方向微调(厘米，+X前、+Y右、+Z上)；当前往下5cm盖住下巴。
	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	FVector avatarOffsetInCharacter = FVector(0.f, 0.f, -5.f);

	// 绕Z轴顺时针(俯视)转90度，让头顶那一面的头像摆正——UE里正yaw俯视是顺时针。
	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	FRotator avatarRotation = FRotator(0.f, 90.f, 0.f);

	// 头像材质(需要名为Texture的贴图参数)和默认头像贴图；市民的头像经SetAvatarTexture替换。
	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	TObjectPtr<UMaterialInterface> avatarBaseMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	TObjectPtr<UTexture2D> avatarTexture;

	// 头像方块的动态材质实例(PlaceAvatar建，SetAvatarTexture更新贴图参数)。
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> avatarMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> moveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> lookAction;

	// 鼠标视角单独一个Input Action：增量输入和摇杆模拟量需要不同的Mapping配置，但都路由到Look()。
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> mouseLookAction;

	// 在PossessedBy/UnPossessed里跟随占有状态增删(CONVENTIONS第7节)。
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> inputMapping;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> inputLookMapping;

	void ToggleCameraView();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bIsFirstPerson = false;

	void StartSprint();
	void StopSprint();

	// MeetOption对话选项UI：滚轮上下移动高亮/F键确认，转发给AForeverPlayerController::GetMeetOptionWidget()。
	void MeetOptionFocusUp();
	void MeetOptionFocusDown();
	void MeetOptionSelect();

	// 武器系统：左键开火/R键换弹/数字键1、2切枪，转发给自己身上的weaponComponent。
	void StartFireWeapon();
	void StopFireWeapon();
	void ReloadWeapon();
	void SwitchToWeapon1();
	void SwitchToWeapon2();

	// 数字键1/2的实现：激活左肩(slot=1)/右肩(slot=2)挂着的武器(若有)，同一时刻最多一把激活。
	void ActivateShoulderWeapon(int32 slot);

	// 最近一次ActivateShoulderWeapon的slot(1=左肩/2=右肩，0=还没激活过)，存档用。
	int32 activeShoulderSlot = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForeverWeaponComponent> weaponComponent;

	// 武器可视化，见ForeverWeaponVisualComponent.md。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForeverWeaponVisualComponent> weaponVisualComponent;

	// 投掷物：数字键3，短按切换、长按弹出选择列表，与肩上的武器互斥，见ForeverThrowableComponent.md。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForeverThrowableComponent> throwableComponent;

	void ThrowKeyPressed();
	void ThrowKeyReleased();
	// 按下3的**真实**时间(秒)——世界时间会被长按列表的慢放影响，不能用。
	double throwKeyPressTime = 0.0;
	// 3键正按着、还没判断出是长按。Tick里到阈值就弹出选择列表。
	bool bThrowKeyHeld = false;
	// 这次按键已经用来弹出列表(或尝试弹出)：随后的松开不再当作短按切换。下一次按下重置，所以
	// 即使松开事件在UI Only输入模式下丢了，也不会吞掉下一次短按。
	bool bSwallowThrowRelease = false;
	void OpenThrowList();

	// 按住超过这个真实时间(秒)算长按，弹出选择列表；没到就松开算短按，切换到下一个投掷物。
	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	float longPressThreshold = 0.3f;

	// 瞄准(默认右键，按住生效)：越肩瞄准，插值cameraBoom的SocketOffset/TargetArmLength，见md。
	void StartAim();
	void StopAim();

	// 统一计算朝向模式(第一人称或瞄准时朝向跟摄像机)，ToggleCameraView/StartAim/StopAim都调。
	void UpdateRotationMode();

	bool bIsAiming = false;

	// 非瞄准状态下cameraBoom的目标值(不能读cameraBoom当前值，那是插值中间态)。
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Aim")
	FVector defaultSocketOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Aim")
	float defaultArmLength = 400.f;

	// 瞄准时cameraBoom的目标值：Y正=向右肩偏移，Z正=略升高，配合更短的臂长。数值为初始值，需PIE调。
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Aim")
	FVector aimSocketOffset = FVector(0.f, 60.f, 40.f);

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Aim")
	float aimArmLength = 150.f;

	// 瞄准过渡的插值速度(VInterpTo/FInterpTo的InterpSpeed)，越大越快；必须是有限值才平滑。
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Aim")
	float aimTransitionSpeed = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float walkSpeed = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float sprintSpeedMultiplier = 10.f;

	bool bIsSprinting = false;

public:
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return cameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return followCamera; }
	FORCEINLINE UCameraComponent* GetFirstPersonCamera() const { return firstPersonCamera; }
	// 供UForeverWeaponComponent::Fire()判断该用哪个摄像机做开火射线的起点/方向。
	FORCEINLINE bool IsFirstPerson() const { return bIsFirstPerson; }
	FORCEINLINE UForeverWeaponComponent* GetWeaponComponent() const { return weaponComponent; }
	FORCEINLINE UForeverWeaponVisualComponent* GetWeaponVisualComponent() const { return weaponVisualComponent; }
	FORCEINLINE UForeverThrowableComponent* GetThrowableComponent() const { return throwableComponent; }
	// 瞄准中且手上激活的是投掷物——供AForeverHUD决定画投掷曲线还是准星。
	bool IsAimingThrow() const;

	// 存档/读档：当前激活的肩膀槽位；读档时RestoreWeaponState按存档重新激活并恢复弹匣余量。
	int32 GetActiveShoulderSlot() const { return activeShoulderSlot; }
	void RestoreWeaponState(int32 slot, int32 magazineAmmo);
	// 供AForeverHUD判断要不要画屏幕中心的准星。
	FORCEINLINE bool IsAiming() const { return bIsAiming; }
};
