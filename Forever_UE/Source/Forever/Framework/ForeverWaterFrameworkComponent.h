#pragma once

#define NOMINMAX

#include <utility>
#include <vector>

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverWaterFrameworkComponent.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class Map;

// 水面：整张图一个共享顶点网格(还原自原来的地形组件BuildOceanMesh)，材质用引擎Water插件的Water_Material_CustomMesh。
// 每帧驱动MPC_Water(时间/相机所在格的水位)和水下后处理的开关。详见ForeverWaterFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverWaterFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverWaterFrameworkComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// 由AForeverFrameworkActor在地形生成之后调用一次：创建水面mesh。map生命周期由调用方持有。
	void GenerateWater(Map* inMap);

private:
	// 每帧：写MPC、按相机所在格有没有水开关水下后处理。
	void UpdateWaterParameters();

	Map* map = nullptr; // 非持有

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> oceanMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> oceanMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> waterCollection;

	// 水下后处理材质(PP_Underwater)，加到天空组件的PostProcessVolume上，权重0/1开关。
	UPROPERTY()
	TObjectPtr<UMaterialInterface> underwaterMaterial;

	bool bUnderwaterBlendableAdded = false;
	bool bOceanHidden = false; // 水面mesh当前是否被"缩到几乎为0"藏起来(不用SetVisibility：它会重建整张图大小的mesh的渲染状态，相机在水面附近来回会严重掉帧)
	bool bUnderwaterShaderReady = false; // PP_Underwater的shader是否已编译完(没完之前用极小权重预热)
	float underwaterWeight = 0.f;
	bool bCollectionFailed = false;
	bool bCollectionWarned = false;
};
