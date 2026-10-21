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
class UStaticMeshComponent;
class UMaterialInterface;
class UTexture2D;

UCLASS()
class FOREVER_API AForeverCharacter : public ACharacter
{
	GENERATED_BODY()

	// 投掷物激活/用完/手不够时要直接开关瞄准状态(StartAim/StopAim是protected)。
	friend class UForeverThrowableComponent;

public:
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

	// 头像方块：没有角色资产时用来区分不同角色(所有人都是同一个小白人)。挂在head骨骼socket上，跟着
	// 走路姿势一起动(和firstPersonCamera同一种挂法)；方块用项目的Cube，表面贴头像贴图(Avatar材质的
	// AvatarTexture参数，BeginPlay里建动态材质实例设进去)。见ForeverCharacter.md"头像方块"一节。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> avatarMesh;

	// 方块边长(厘米)；avatarOffset是相对head骨骼的偏移(骨骼局部坐标)；avatarRotation是相对**角色本身**
	// 的旋转——BeginPlay按head骨骼的参考姿势把骨骼自己的朝向抵消掉，avatarRotation为0时方块和角色对齐：
	// 方块+X面朝角色正前方(正面)、+Z朝上，之后再跟着head骨骼的动画一起动。
	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	float avatarSize = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	FVector avatarOffset = FVector(10.f, 0.f, 0.f);

	// 在avatarOffset基础上再按**角色本身**的方向微调(厘米，+X前、+Y右、+Z上)——骨骼局部轴不直观，
	// 上下前后的调整都写在这里。当前往下5cm，盖住露出来的下巴。
	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	FVector avatarOffsetInCharacter = FVector(0.f, 0.f, -5.f);

	// 绕Z轴顺时针(俯视)转90度，让头顶那一面的头像摆正——UE里正yaw俯视是顺时针。
	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	FRotator avatarRotation = FRotator(0.f, 90.f, 0.f);

	// 头像材质(需要一个名为AvatarTexture的贴图参数)和头像贴图。现在所有角色都用同一张default，
	// 以后按角色分配时只需要在BeginPlay之前/之后换这张贴图。
	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	TObjectPtr<UMaterialInterface> avatarBaseMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	TObjectPtr<UTexture2D> avatarTexture;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> moveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> lookAction;

	// 鼠标视角单独用一个Input Action(而非直接复用lookAction),
	// 因为鼠标的增量输入和手柄摇杆的模拟量输入需要不同的Input Mapping配置,
	// 但两者都路由到同一个Look()处理函数。
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> mouseLookAction;

	// 在PossessedBy/UnPossessed里跟随占有状态增删,而不是固定挂在PlayerController上——
	// 这样任意NPC被玩家操控时,输入映射会跟着当前被控制的角色走。
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> inputMapping;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> inputLookMapping;

	void ToggleCameraView();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bIsFirstPerson = false;

	void StartSprint();
	void StopSprint();

	// MeetOption对话选项UI：滚轮上下移动高亮/F键确认选中，转发给
	// AForeverPlayerController::GetMeetOptionWidget()，见ForeverPlayerController.md"旧MainController热键"一节。
	void MeetOptionFocusUp();
	void MeetOptionFocusDown();
	void MeetOptionSelect();

	// 武器系统：鼠标左键开火(按住/松开转发给UForeverWeaponComponent::StartFire/StopFire，
	// 全自动武器由组件自己的TickComponent按fireRate间隔连发)/R键换弹/数字键1、2切枪
	// （切两把测试武器weapon_pistol/weapon_rifle，见Source/Basic/player/weapon_basic.h）。
	// 直接调自己身上的weaponComponent——武器是这个Character自己的私有状态，不是
	// map/world级别的全局单例，不需要反查AForeverFrameworkActor。
	void StartFireWeapon();
	void StopFireWeapon();
	void ReloadWeapon();
	void SwitchToWeapon1();
	void SwitchToWeapon2();

	// 数字键1/2的真正实现：激活左肩(slot=1)/右肩(slot=2)当前挂着的武器(若有)——不是按
	// 硬编码id切枪。同一时刻最多一把武器"激活"(ForeverWeaponComponent::currentWeapon)，
	// 槽位里的Asset和激活的活武器是互斥的两种表示，见Source/Core/player/player.h
	// "武器只挂肩膀"一节。
	void ActivateShoulderWeapon(int32 slot);

	// 最近一次ActivateShoulderWeapon的slot(1=左肩/2=右肩，0=还没激活过)，存档用。
	int32 activeShoulderSlot = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForeverWeaponComponent> weaponComponent;

	// 投掷物：数字键3。短按切换身上的投掷物(长按弹出选择列表本期未做)；激活后右键瞄准显示
	// 预览曲线、左键扔出。和肩上的武器互斥，见ForeverThrowableComponent.md。
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

	// 瞄准（默认鼠标右键，按住生效/松开取消）：第三人称下把cameraBoom的SocketOffset/
	// TargetArmLength平滑插值到肩膀附近的取景位置，让角色自己的身体不挡住准星指向的东西——
	// 仿照大部分第三人称射击游戏的"越肩瞄准"做法。只改cameraBoom/followCamera这一份现有
	// 组件的参数，不新增第三个摄像机——UForeverWeaponComponent::Fire()本来就读
	// GetFollowCamera()的实时位置做开火起点，瞄准时摄像机移过去了，开火起点自动跟着变，
	// 不需要改武器组件一行代码，见ForeverWeaponComponent.md"瞄准"一节。
	void StartAim();
	void StopAim();

	// 瞄准时角色朝向要跟着摄像机(鼠标)转，不能只跟着移动方向转——人不能"边跑边朝身后瞄准"。
	// 复用第一人称视角本来就有的"朝向跟摄像机走"这套开关(bUseControllerRotationYaw+
	// CharacterMovement::bOrientRotationToMovement)，瞄准时临时借用同一套开关，取消瞄准后
	// 按bIsFirstPerson恢复原状——不是瞄准专属的新变量，是同一套朝向模式的另一个触发条件。
	// ToggleCameraView/StartAim/StopAim三处都会调这个，统一算一遍当前该用哪种朝向模式。
	void UpdateRotationMode();

	bool bIsAiming = false;

	// 非瞄准状态下cameraBoom的SocketOffset/TargetArmLength目标值——构造函数里
	// TargetArmLength=400.f，SocketOffset默认FVector::ZeroVector，这里各自存一份供Tick()
	// 插值时做"取消瞄准该回到哪"的目标，不依赖读取cameraBoom当前值(那是插值过程中的中间态，
	// 不是目标值)。
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Aim")
	FVector defaultSocketOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Aim")
	float defaultArmLength = 400.f;

	// 瞄准时cameraBoom的目标SocketOffset/TargetArmLength——Y轴正值=向右肩偏移，
	// Z轴正值=略微升高，配合缩短的ArmLength(离角色更近)做出"越肩瞄准"的效果。具体数值是
	// 按经验给的初始值，实际手感需要在PIE里试，见ForeverWeaponComponent.md。
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Aim")
	FVector aimSocketOffset = FVector(0.f, 60.f, 40.f);

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Aim")
	float aimArmLength = 150.f;

	// 瞄准/取消瞄准过渡的插值速度(FMath::VInterpTo/FInterpTo的InterpSpeed参数)——越大过渡
	// 越快，用户明确要求"瞄准和取消瞄准的过程相机是平滑移动"，不能瞬间跳变，所以这里必须是
	// 一个有限值，不能直接在StartAim/StopAim里一次性把SocketOffset/TargetArmLength设成
	// 目标值。
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
	FORCEINLINE UForeverThrowableComponent* GetThrowableComponent() const { return throwableComponent; }
	// 瞄准中且手上激活的是投掷物——供AForeverHUD决定画投掷曲线还是准星。
	bool IsAimingThrow() const;

	// 存档/读档：当前激活的肩膀槽位；读档时RestoreWeaponState按存档重新激活并恢复弹匣余量。
	int32 GetActiveShoulderSlot() const { return activeShoulderSlot; }
	void RestoreWeaponState(int32 slot, int32 magazineAmmo);
	// 供AForeverHUD判断要不要画屏幕中心的准星。
	FORCEINLINE bool IsAiming() const { return bIsAiming; }
};
