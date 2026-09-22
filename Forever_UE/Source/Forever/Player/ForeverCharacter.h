#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ForeverCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

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

	// T键：把当前"附近市民名单"（ACitizenElement::nearbyCitizens，由子类ACitizenElement的
	// 碰撞盒维护）里每一个人的acquaintances/experiences输出到log——不再切换玩家控制的
	// 市民（原来是Possess切换，见CitizenElement.md"T键：输出附近市民的人际关系数据"一节）。
	// 定义在基类上，不管当前被占有的是最初的ADefaultPawn（没有绑这个方法，T键不响应）还是
	// 某个ACitizenElement，只要是AForeverCharacter的子类都自动获得同一个T键行为。
	void LogNearbyCitizenRelationships();

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float walkSpeed = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float sprintSpeedMultiplier = 10.f;

	bool bIsSprinting = false;

public:
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return cameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return followCamera; }
	FORCEINLINE UCameraComponent* GetFirstPersonCamera() const { return firstPersonCamera; }
};
