#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Math/BoxSphereBounds.h"

#include "TransitVehicleElement.generated.h"

class Vehicle;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UBoxComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
struct FHitResult;

// 公共交通线路车辆(公交/火车/飞机)在场景里对应的Actor——运动学地沿Route算出来的transform
// 移动，不模拟物理(Tick里直接读vehicle->GetTransform())。外观有三种，按VehicleMod的字段选：
// transitMeshPath指向骨骼网格(公交PAZ_652)用skeletalBody显示；指向静态网格(飞机)用bodyMesh；
// 为空用按sizeX/Y/Z缩放的/Engine/BasicShapes/Cube占位，见public_transport_plan.md。
//
// 骨骼网格且VehicleMod::boardable时这辆车可以被搭乘(公交)：
// - 触发盒(模型包围盒外扩一圈)：玩家的AForeverCharacter走进盒里，把车辆script的options推给
//   MeetOption(上车选项由vehicle_bus.script在停站时通过add_option/remove_option控制)；
// - 搭乘：UForeverTrafficFrameworkComponent::ApplyBoardVehicle让玩家Possess这个Pawn，
//   只有鼠标视角(IMC_MouseLook)，没有IMC_Default，所以WASD无效；车继续自己跑。车上只要
//   在乘坐就一直显示车辆的options(不需要触发盒)，F/滚轮选择；没有Q下车键，下车只走script的
//   "下车"选项(board后的leave_vehicle Change)或force_leave。
UCLASS(Blueprintable)
class FOREVER_API ATransitVehicleElement : public APawn
{
	GENERATED_BODY()

public:
	ATransitVehicleElement();

	// 由UForeverTrafficFrameworkComponent::GenerateVehicles在生成时调用一次：绑定这个Element
	// 对应哪个Vehicle，加载外观；骨骼网格且可搭乘时建触发盒/相机。
	void Init(Vehicle* inVehicle);

	Vehicle* GetVehicle() const { return vehicle; }

	// 搭乘前被占有的pawn(下车时要恢复谁)，和AVehicleElement同一套。
	void SetPreviousPawn(APawn* inPreviousPawn) { previousPawn = inPreviousPawn; }
	TWeakObjectPtr<APawn> GetPreviousPawn() const { return previousPawn; }

	// 这辆车能不能被搭乘(骨骼网格 + boardable，Init里决定)。
	bool CanBoard() const { return bBoardable; }

	// 交互选项在运行中变了(add_option/remove_option，由AForeverFrameworkActor::RefreshInteractOptions
	// 在游戏线程上调用)：重新烘焙cachedOptions；玩家在触发盒内或正乘坐时同步换掉屏幕上的选项。
	void RefreshOptions();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// 鼠标/手柄视角——只转相机，和AVehicleElement::Look同一个处理方式。
	void Look(const FInputActionValue& value);

	// F/滚轮的选项焦点输入，照抄AForeverCharacter的同名函数(搭乘时玩家不再是Character)。
	void MeetOptionFocusUp();
	void MeetOptionFocusDown();
	void MeetOptionSelect();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> bodyMesh;

	// 骨骼网格外观(公交)：挂在bodyMesh(RootComponent)下，构造时隐藏，Init加载成功才显示。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> skeletalBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> cameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> followCamera;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> lookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> mouseLookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> inputLookMapping;

private:
	// 骨骼网格加载成功后建触发盒、摆相机，localBounds是骨骼网格自己的局部包围盒。
	void BuildBoardingComponents(const FBoxSphereBounds& localBounds);

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 把cachedOptions推给MeetOption(先清掉这辆车名下的旧按钮再逐个加，不依赖ReplaceOptions——
	// 它在名下没有按钮时是空操作，停站前选项列表是空的，这时加上去的新选项就丢了)。
	void ShowOptions();
	void HideOptions();

	void BakeOptions();

	Vehicle* vehicle = nullptr;
	TWeakObjectPtr<APawn> previousPawn;

	// 缓存vehicle->GetMeshYawOffsetDegrees()——bodyMesh是这个Actor的RootComponent，
	// Tick()里SetActorLocationAndRotation直接设的是RootComponent的世界旋转，Init()
	// 时单独对bodyMesh调SetRelativeRotation不会有任何效果(马上被下一次Tick覆盖)，这个
	// 偏移必须叠进Tick()每帧算的朝向里，见.cpp Tick()的说明。
	float meshYawOffsetDegrees = 0.f;

	bool bBoardable = false;
	// 玩家的AForeverCharacter正在触发盒里 / 玩家正乘坐(PossessedBy~UnPossessed)。
	bool bPlayerInBox = false;
	bool bAboard = false;

	UPROPERTY()
	TObjectPtr<UBoxComponent> proximityBox;

	// vehicle->GetName()/GetOptions()的快照，Overlap回调只读这份快照，不解引用vehicle，和
	// AVehicleElement同一套安全原则。
	FString vehicleNameOnly;
	TArray<FString> cachedOptions;
};
