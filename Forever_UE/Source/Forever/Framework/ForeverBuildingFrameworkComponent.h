#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverBuildingFrameworkComponent.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class Map;

// 阶段4-1 Building落地：这次Building只是一个footprint+类型的占位对象(Room/Component布局
// 明确推迟，见map.md"InitBuildings"一节)，可视化手段是按其矩形范围画一个矮扁procedural
// mesh box，人工检查面积分配是否合理，不做真实建筑外观。详见ForeverBuildingFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverBuildingFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverBuildingFrameworkComponent();

	// 由AForeverFrameworkActor在Map::InitBuildings()跑完后调用一次。map生命周期由调用方持有。
	void GenerateBuildings(Map* inMap);

private:
	Map* map = nullptr;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> buildingMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> buildingMaterial;

	// 固定基础材质(编辑器可覆盖)，CONVENTIONS.md §4的ConstructorHelpers模式。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	TObjectPtr<UMaterialInterface> buildingBaseMaterial;
};
