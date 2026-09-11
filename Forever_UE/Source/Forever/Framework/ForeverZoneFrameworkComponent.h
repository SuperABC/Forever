#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverZoneFrameworkComponent.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class Map;

// 阶段4-1 Zone落地：这次Zone只是一个footprint+类型的占位对象(Zone内部再摆Building的递归
// 布局明确推迟，见map.md"InitZones"一节)，可视化手段是按其矩形范围画一个矮扁procedural
// mesh box，人工检查面积分配是否合理，不做真实建筑外观。详见ForeverZoneFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverZoneFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverZoneFrameworkComponent();

	// 由AForeverFrameworkActor在Map::InitZones()跑完后调用一次。map生命周期由调用方持有。
	void GenerateZones(Map* inMap);

private:
	Map* map = nullptr;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> zoneMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> zoneMaterial;

	// 固定基础材质(编辑器可覆盖)，CONVENTIONS.md §4的ConstructorHelpers模式。
	UPROPERTY(EditDefaultsOnly, Category = "Zone")
	TObjectPtr<UMaterialInterface> zoneBaseMaterial;
};
