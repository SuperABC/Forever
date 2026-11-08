#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "VehicleElement.generated.h"

class Vehicle;
class Room;
class UForeverTrafficFrameworkComponent;
class USkeletalMeshComponent;
class UChaosWheeledVehicleMovementComponent;
class USpringArmComponent;
class UCameraComponent;
class UBoxComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
struct FHitResult;

// 一辆车在场景里对应的Actor：Chaos真实物理驾驶/相机/输入，所有车型公用，不写死外观。
// 每种车型的外观(骨骼网格、轮子骨骼名)是继承本类的蓝图子类，VehicleMod::blueprintPath指向它，
// 原因和流程见VehicleElement.md。
UCLASS(Blueprintable)
class FOREVER_API AVehicleElement : public APawn
{
	GENERATED_BODY()

public:
	AVehicleElement();

	// 由UForeverTrafficFrameworkComponent生成车辆时调用一次：绑定对应的Vehicle，预置车辆没人上车时
	// inPreviousPawn传nullptr。外观由蓝图子类决定，与这里无关。会建好proximity box(靠近弹"上车"选项)。
	void Init(Vehicle* inVehicle, APawn* inPreviousPawn);

	// 由UForeverTrafficFrameworkComponent::ApplyEnterVehicle在真正上车那一刻调用：记住
	// 上车前被占有的pawn，下车时(ExitVehicle)要恢复谁。
	void SetPreviousPawn(APawn* inPreviousPawn) { previousPawn = inPreviousPawn; }

	// 置空vehicle指针(Vehicle所有权在Traffic身上，这里不delete，同ACitizenElement::EndPlay)。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	Vehicle* GetVehicle() const { return vehicle; }
	USkeletalMeshComponent* GetCarMesh() const { return carMesh; }
	UChaosWheeledVehicleMovementComponent* GetVehicleMovement() const { return vehicleMovement; }

	// 油门/刹车/转向/手刹的统一入口：玩家WASD(Move/StopMove/Handbrake)和AI Controller
	// (AVehicleCommandController)走同一条路，AI开车和手动开车的输入语义完全一致。
	void SetDriveInput(float throttle, float brake, float steer);
	void SetHandbrake(bool bOn);

	// 搭乘模式(公交：玩家当乘客)：Possess之前设置。PossessedBy只加视角Context(WASD无效)，常驻显示
	// 这辆车的选项(停站时的"下车")；Move/手刹/Q键全部忽略；换手时不刹车(AI Controller继续按弱引用开)。
	void SetRideMode(bool bRide) { bRideMode = bRide; }
	bool IsRideMode() const { return bRideMode; }

	// 运动学/物理切换(见Controller/VehicleCommandController)：运动学时关掉物理模拟，位姿由Controller
	// 直接写；切回物理时重新开启并恢复轮子刚体状态，velocity是世界坐标线速度(UE单位/秒)。
	void SetKinematicMode(bool bKinematic);
	bool IsKinematicMode() const { return bKinematicMode; }
	void ResumePhysics(const FVector& velocity);
	// 当前沿车头方向的速度(UE单位/秒)，运动学模式下返回0。
	float GetForwardSpeed() const;
	// 前后轮轴距(UE单位)，取不到轮子骨骼时返回0。
	float ComputeWheelbase() const;
	// 前轴中心的世界位置（前轮骨骼平均），取不到时用车头方向偏半个轴距。AI转向要用它判断离导航线多远。
	FVector GetFrontAxleLocation() const;
	// 前轮最大转向角(度)，取不到时返回0。
	float GetMaxSteerAngleDegrees() const;

	// 交互选项在运行中变了(add_option/remove_option，由AForeverFrameworkActor::RefreshInteractOptions
	// 在游戏线程上调用)：重新烘焙cachedOptions；玩家正站在旁边时同步替换MeetOption列表里的选项。
	void RefreshOptions();
	// 玩家刚下车站回车旁边时调用：如果玩家实际已经在触发盒里，但"进入"事件没把选项带出来
	// (下车瞬间的重叠事件时序不可靠，飞机实测过)，就补一次选项；已经显示的会先移除再加，不会重复。
	void EnsureOptionsForPlayer(APawn* player);
	TWeakObjectPtr<APawn> GetPreviousPawn() const { return previousPawn; }

	// 重心相对物理资产自带重心的偏移(车身本地坐标，UE单位)，在蓝图里按车型填；全0=不动。
	// Z取负=重心下移，防侧翻(公交又高又宽，要下移)。BeginPlay时应用。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
	FVector centerOfMassOffset = FVector::ZeroVector;

	// 四个轮子的物理半径(UE单位)，在蓝图里按车型填，BeginPlay时用SetWheelRadius按轮位写入，
	// 不用为每种车型建轮子蓝图子类。填模型轮胎的实际半径(轮骨Z=半径)；<=0则沿用
	// UVehicleWheelFront/Rear的默认半径。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wheels")
	float wheelRadius = 0.f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// 把和车身重叠的人挪出车身（车不受人影响，人被车推着走），见实现处注释。
	void PushNearbyCharacters();
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// W/S(movement.Y)按正负分别转成油门/刹车(不能把S当负油门，否则无法倒车)，A/D转向，见VehicleElement.md。
	void Move(const FInputActionValue& value);

	// WSAD松开(Completed/Canceled)时三个输入归零(Triggered在输入回到(0,0)后不再触发，不归零会停在最后一个值)。
	void StopMove(const FInputActionValue& value);

	// 鼠标/手柄视角——只转相机(cameraBoom的bUsePawnControlRotation)，不转车身。
	void Look(const FInputActionValue& value);

	// Q键：转发给UForeverTrafficFrameworkComponent::ExitVehicle(带下车点碰撞检测)。
	void ExitVehicle();

	// 空格手刹：绑在Started/Completed(两态开关，没有连续模拟量，不用Triggered)。
	void Handbrake(const FInputActionValue& value);
	void StopHandbrake(const FInputActionValue& value);

	// 搭乘模式下选项列表的上下/确认(键位同市民对话选项)，同ATransitVehicleElement。
	void MeetOptionFocusUp();
	void MeetOptionFocusDown();
	void MeetOptionSelect();

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
	// 玩家靠近检测，写法同ACitizenElement::BuildProximityBox，选项走同一个MeetOption UI/F键。
	void BuildProximityBox();

	// 搭乘期间常驻显示/撤掉选项(先RemoveName再AddOption，不重复)。
	void ShowOptions();
	void HideOptions();
	// 重新打开物理后把轮子骨骼的刚体再关回去(SetSimulatePhysics(true)会把它们重新打开)。
	void DisableWheelBodies();

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	Vehicle* vehicle = nullptr;
	TWeakObjectPtr<APawn> previousPawn;
	bool bRideMode = false;
	bool bKinematicMode = false;

	UPROPERTY()
	TObjectPtr<UBoxComponent> proximityBox;

	// vehicle->GetName()/GetOptions()的快照，Overlap回调只读这份，不解引用vehicle(同ACitizenElement)。
	FString vehicleNameOnly;
	TArray<FString> cachedOptions;
};
