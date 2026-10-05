#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "TransitVehicleElement.generated.h"

class Vehicle;
class UStaticMeshComponent;

// 公共交通线路车辆(公交/火车/飞机)在场景里对应的Actor——纯展示物，运动学地沿Route算出来的
// transform移动，不模拟物理、不接受操控/搭乘(这次预留接口，见VehicleMod::drivable/
// boardable)。blueprintPath为空时(这次三种公共交通车型都是空)，用一个按VehicleMod::sizeX/Y/Z
// 缩放的/Engine/BasicShapes/Cube占位，见public_transport_plan.md"载具外观：立方体"一节。
UCLASS(Blueprintable)
class FOREVER_API ATransitVehicleElement : public APawn
{
	GENERATED_BODY()

public:
	ATransitVehicleElement();

	// 由UForeverTrafficFrameworkComponent::GenerateVehicles在生成时调用一次：绑定这个Element
	// 对应哪个Vehicle，并按vehicle->GetSize()缩放立方体网格。
	void Init(Vehicle* inVehicle);

	Vehicle* GetVehicle() const { return vehicle; }

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> bodyMesh;

private:
	Vehicle* vehicle = nullptr;

	// 缓存vehicle->GetMeshYawOffsetDegrees()——bodyMesh是这个Actor的RootComponent，
	// Tick()里SetActorLocationAndRotation直接设的是RootComponent的世界旋转，Init()
	// 时单独对bodyMesh调SetRelativeRotation不会有任何效果(马上被下一次Tick覆盖)，这个
	// 偏移必须叠进Tick()每帧算的朝向里，见.cpp Tick()的说明。
	float meshYawOffsetDegrees = 0.f;
};
