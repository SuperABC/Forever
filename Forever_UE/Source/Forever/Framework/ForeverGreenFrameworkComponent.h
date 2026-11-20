#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverGreenFrameworkComponent.generated.h"

class UInstancedStaticMeshComponent;
class Map;

// 绿化区(Green)的自定义模型：把Green::GetDecorations()按mesh合并成ISM常驻(绿化区没有LOD，数量也少)。
// 树不在这里，走ForeverVegetationFrameworkComponent；圆/导航不画，见ForeverTrafficFrameworkComponent的导航线。
// 详见ForeverGreenFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverGreenFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverGreenFrameworkComponent();

	// 由AForeverFrameworkActor在Map::InitGreens()和地形生成之后调用一次(Z要取地形高度)。
	void GenerateGreens(Map* inMap);

private:
	UInstancedStaticMeshComponent* GetOrCreateISM(const FString& meshPath);

	UPROPERTY()
	TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> meshInstances;
};
