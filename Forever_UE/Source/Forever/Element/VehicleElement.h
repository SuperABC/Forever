#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "VehicleElement.generated.h"

class Vehicle;
class USkeletalMeshComponent;
class UChaosWheeledVehicleMovementComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

// 一辆车在场景里对应的Actor——只提供驾驶(Chaos真实悬挂/轮胎/引擎物理)/相机/输入这些
// 所有车型公用的逻辑，不写死任何具体车型的外观。不直接继承AWheeledVehiclePawn(它固定
// 用USkeletalMeshComponent存在私有字段里，没有开放子类覆盖的方式)，而是自己组合
// USkeletalMeshComponent+UChaosWheeledVehicleMovementComponent+相机/输入，效果等价。
//
// **具体每种车型(轿车/卡车/……)的外观是继承这个类的蓝图(Blueprint)子类**——骨骼网格
// (CarMesh组件)、四个轮子的骨骼名(VehicleMovement组件的Wheel Setups)都是
// EditAnywhere属性，蓝图子类在Details面板里设置即可，不需要写C++代码。
// `VehicleMod::blueprintPath`存的就是某个这样的蓝图子类的资源路径，
// `UForeverTrafficFrameworkComponent::ToggleVehicle`生成车辆前用这个路径
// `LoadClass<AVehicleElement>()`拿到具体该用哪个类。这样设计是因为
// `UChaosWheeledVehicleMovementComponent`要求骨骼网格必须在Actor构造函数阶段就绑定
// 好(见VehicleElement.md"骨骼网格必须在组件注册之前就绑好"一节)，而`SpawnActor<T>()`
// 不支持给构造函数传自定义参数——"这个Actor该长什么样"只能靠"生成之前就选好用哪个
// UClass"来决定，蓝图子类的属性覆盖恰好在蓝图保存时就烘焙进了那个类自己的CDO，早于
// 任何运行时SpawnActor调用，天然满足这个时序要求。详见VehicleElement.md。
UCLASS(Blueprintable)
class FOREVER_API AVehicleElement : public APawn
{
	GENERATED_BODY()

public:
	AVehicleElement();

	// 由UForeverTrafficFrameworkComponent::ToggleVehicle在玩家按T上车时调用一次：绑定这个
	// Element对应哪个Vehicle，记住previousPawn(上车前被占有的pawn，下车时要恢复谁)。外观
	// (骨骼网格/轮子骨骼名)由生成这个Actor时选用的具体蓝图子类决定，和这里的Vehicle实例
	// 无关，这个函数不需要做任何外观相关的事。
	void Init(Vehicle* inVehicle, APawn* inPreviousPawn);

	// PIE停止/退出游戏时置空vehicle指针——Vehicle所有权在Traffic身上，这里不delete，和
	// ACitizenElement::EndPlay同一套安全原则。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	Vehicle* GetVehicle() const { return vehicle; }
	TWeakObjectPtr<APawn> GetPreviousPawn() const { return previousPawn; }

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// W/S(movement.Y)按正负分别转发给vehicleMovement->SetThrottleInput/SetBrakeInput
	// (不是简单地把movement.Y原样当油门用)——UChaosVehicleMovementComponent默认
	// bReverseAsBrake=true，油门/刹车被设计成像真车的两个独立踏板：车速较高时按S是正常
	// 刹车，车速降到接近0之后继续按住S才会挂倒挡真正往后开，不是一松手就立刻倒车，见
	// VehicleElement.md。A/D(movement.X)转发给SetSteeringInput。
	void Move(const FInputActionValue& value);

	// WSAD松开(Completed/Canceled)时把三个输入归零——Enhanced Input的Triggered事件一旦
	// 输入回到(0,0)就不会再触发，不显式归零的话会停留在松手前的最后一个值上。
	void StopMove(const FInputActionValue& value);

	// 鼠标/手柄视角——只转相机(cameraBoom的bUsePawnControlRotation)，不转车身，和
	// AForeverCharacter::Look同一个处理方式，只是这里没有共同基类，各自维护一份。
	void Look(const FInputActionValue& value);

	// T键：调用同一个"上下车切换"入口，见ForeverTrafficFrameworkComponent.h的
	// RequestToggleVehicle说明。
	void ToggleVehicle();

	// 空格手刹：按住/松开分别转发给vehicleMovement->SetHandbrakeInput(true/false)，
	// 绑在Started/Completed而不是Move()那种Triggered——手刹是纯粹的按下/松开两态开关，
	// 不像油门/转向有个连续的模拟量。
	void Handbrake(const FInputActionValue& value);
	void StopHandbrake(const FInputActionValue& value);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> carMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UChaosWheeledVehicleMovementComponent> vehicleMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> cameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> followCamera;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> moveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> lookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> mouseLookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> inputMapping;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> inputLookMapping;

private:
	Vehicle* vehicle = nullptr;
	TWeakObjectPtr<APawn> previousPawn;
};
