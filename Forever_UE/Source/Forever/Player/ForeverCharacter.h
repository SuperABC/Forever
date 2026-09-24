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

	// T键：上下车切换——转发给UForeverTrafficFrameworkComponent::RequestToggleVehicle。
	// AVehicleElement（车辆Actor，不继承AForeverCharacter，见VehicleElement.h）自己也绑了
	// 一份同名调用，因为占有对象换成车之后这个类的SetupPlayerInputComponent就不会再生效了，
	// 详见ForeverTrafficFrameworkComponent.h的说明。这次覆盖掉上一版"T键输出附近市民关系"
	// 的用法（ACitizenElement::LogNearbyRelationships()本身没有删除，只是不再绑定按键）。
	void ToggleVehicle();

	// MeetOption对话选项UI：滚轮上下移动高亮/F键确认选中，转发给
	// AForeverPlayerController::GetMeetOptionWidget()，见MAINCONTROLLER_TODO.md
	// "MouseScrollUp/Down + F"这一行热键。
	void MeetOptionFocusUp();
	void MeetOptionFocusDown();
	void MeetOptionSelect();

	// 武器系统：鼠标左键开火(按住/松开转发给UForeverWeaponComponent::StartFire/StopFire，
	// 全自动武器由组件自己的TickComponent按fireRate间隔连发)/R键换弹/数字键1、2切枪
	// （切两把测试武器weapon_pistol/weapon_rifle，见Source/Basic/player/weapon_basic.h）。
	// 和ToggleVehicle同一个"转发给专门组件"的模式，这次直接调自己身上的weaponComponent
	// （不需要像ToggleVehicle那样经过静态Request*辅助函数反查AForeverFrameworkActor——
	// 武器是这个Character自己的私有状态，不是map/world级别的全局单例）。
	void StartFireWeapon();
	void StopFireWeapon();
	void ReloadWeapon();
	void SwitchToWeapon1();
	void SwitchToWeapon2();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForeverWeaponComponent> weaponComponent;

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
};
