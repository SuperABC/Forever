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

UCLASS()
class FOREVER_API AForeverCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AForeverCharacter();

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

protected:
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

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> moveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> lookAction;

	// 鼠标视角单独用一个Input Action(而非直接复用lookAction),
	// 因为鼠标的增量输入和手柄摇杆的模拟量输入需要不同的Input Mapping配置,
	// 但两者都路由到同一个Look()处理函数。
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> mouseLookAction;

	// 对应旧蓝图MainCharacter的"Input Mapping"/"Input Look Mapping"变量,
	// 在PossessedBy/UnPossessed里跟随占有状态增删,而不是固定挂在PlayerController上——
	// 这样阶段6"任意NPC可被玩家操控"换人时,输入映射会跟着当前被控制的角色走。
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
	// AForeverPlayerController::GetMeetOptionWidget()，见MAINCONTROLLER_TODO.md
	// "MouseScrollUp/Down + F"这一行热键。
	void MeetOptionFocusUp();
	void MeetOptionFocusDown();
	void MeetOptionSelect();

	// 武器系统：鼠标左键开火(按住/松开转发给UForeverWeaponComponent::StartFire/StopFire，
	// 全自动武器由组件自己的TickComponent按fireRate间隔连发)/R键换弹/数字键1、2切枪
	// （切两把测试武器weapon_pistol/weapon_rifle，见Source/Basic/player/weapon_basic.h）。
	// 这次直接调自己身上的weaponComponent——武器是这个Character自己的私有状态，不是
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForeverWeaponComponent> weaponComponent;

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
	// 供AForeverHUD判断要不要画屏幕中心的准星。
	FORCEINLINE bool IsAiming() const { return bIsAiming; }
};
