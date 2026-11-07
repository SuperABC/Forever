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

// 公共交通线路车辆(公交/火车/飞机)在场景里对应的Actor：运动学地沿Route算出的transform移动，不模拟物理。
// 外观选择、可搭乘(触发盒/搭乘视角)规则见TransitVehicleElement.md。
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

	// 搭乘前被占有的pawn(下车时要恢复谁)。
	void SetPreviousPawn(APawn* inPreviousPawn) { previousPawn = inPreviousPawn; }
	TWeakObjectPtr<APawn> GetPreviousPawn() const { return previousPawn; }

	// 这辆车能不能被搭乘(骨骼网格 + boardable，Init里决定)。
	bool CanBoard() const { return bBoardable; }

	// 公交近处换成物理AVehicleElement时，这辆运动学的挂起：隐藏、关Tick、关触发盒（撤掉屏幕上它名下的选项）。
	// 恢复时反过来。搭乘中的不应被挂起（调用方保证）。
	void SetSuspended(bool bSuspend);
	bool IsSuspended() const { return bSuspended; }

	// 交互选项在运行中变了(add_option/remove_option，由AForeverFrameworkActor::RefreshInteractOptions
	// 在游戏线程上调用)：重新烘焙cachedOptions；玩家在触发盒内或正乘坐时同步换掉屏幕上的选项。
	void RefreshOptions();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// 鼠标/手柄视角——只转相机。
	void Look(const FInputActionValue& value);

	// F/滚轮的选项焦点输入(搭乘时玩家不再是Character，所以这里要自己绑)。
	void MeetOptionFocusUp();
	void MeetOptionFocusDown();
	void MeetOptionSelect();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> bodyMesh;

	// 骨骼网格外观(公交)：挂在bodyMesh(RootComponent)下，构造时隐藏，Init加载成功才显示。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> skeletalBody;

	// 可搭乘的静态网格外观(火车)：同skeletalBody，缩放只作用在自己身上(root保持1倍，触发盒/相机才不被缩)。
	// 构造时隐藏，Init加载成功且可搭乘才显示；不可搭乘的静态网格(飞机)直接放在bodyMesh上。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> staticBody;

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

	// 把cachedOptions推给MeetOption(先清再加，不用ReplaceOptions：它在名下没有按钮时是空操作，新选项会丢)。
	void ShowOptions();
	void HideOptions();

	void BakeOptions();

	Vehicle* vehicle = nullptr;
	TWeakObjectPtr<APawn> previousPawn;

	// 缓存vehicle->GetMeshYawOffsetDegrees()，叠进Tick()每帧算的朝向里(不能设在bodyMesh的相对旋转上，会被Tick覆盖)。
	float meshYawOffsetDegrees = 0.f;

	bool bBoardable = false;
	bool bSuspended = false;
	// 玩家的AForeverCharacter正在触发盒里 / 玩家正乘坐(PossessedBy~UnPossessed)。
	bool bPlayerInBox = false;
	bool bAboard = false;

	UPROPERTY()
	TObjectPtr<UBoxComponent> proximityBox;

	// vehicle->GetName()/GetOptions()的快照，Overlap回调只读这份，不解引用vehicle。
	FString vehicleNameOnly;
	TArray<FString> cachedOptions;
};
