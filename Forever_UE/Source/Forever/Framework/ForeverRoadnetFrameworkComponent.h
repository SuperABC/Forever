#pragma once

#include <vector>
#include <string>

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverRoadnetFrameworkComponent.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMesh;
class Map;
class Road;

// 阶段4-1 Roadnet落地：沿每条Road按其mesh/unit用ISM重复摆放(要求9"沿路线分隔mesh元素"+
// "曲线道路"的落地方式)，路口mesh(直线简化版扇形三角剖分)，开口cube(要求5)，
// 车道分裂/开口demo(临时验证手段)。详见ForeverRoadnetFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverRoadnetFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverRoadnetFrameworkComponent();

	// 由AForeverFrameworkActor在Map::InitRoadnet()跑完后调用一次。map生命周期由调用方持有。
	void GenerateRoadnet(Map* inMap);

private:
	// 沿road的Connection弧长，按其mesh/unit重复摆放ISM实例；命中road->GetOpenings()的弧长
	// 区间跳过实例摆放，改由BuildOpeningMeshes统一生成开口cube。trimStart/trimEnd(地图单位)：
	// 该端如果连着真实路口，需要把tiling往回收缩这么多距离，让路口mesh独占这块面积，不然
	// 两条路的mesh会在路口重叠、z-fighting(见ForeverRoadnetFrameworkComponent.md)；端点是
	// extern(地图边缘)则传0，不收缩。
	void BuildRoadInstances(Road* road, float trimStart, float trimEnd);

	// 每条road的每个开口，生成一个扁平cube的顶点/三角形，追加进outVertices/outTriangles/outUvs
	// (调用方统一汇总后一次性CreateMeshSection，避免多次调用互相覆盖同一个section)。
	void BuildOpeningMeshes(Road* road, TArray<FVector>& outVertices, TArray<int32>& outTriangles, TArray<FVector2D>& outUvs);

	// 遍历map->GetJunctions()，每个路口按approach.angle排序后的curbLeft/curbRight两两相邻
	// 连线围成多边形，从路口中心做扇形三角剖分，贴RoadPlain材质。
	void BuildJunctionMeshes();

	// 临时验证用：取第一个lot的边界Road映射里任意一条路，调用一次Map::AddRoadAccessNode
	// 演示车道分裂/开口cube效果。
	void SpawnAccessNodeDemo();

	// 按mesh资产路径复用/新建ISM组件(同一路径的mesh只创建一个ISM，不同Road共用)。
	UInstancedStaticMeshComponent* GetOrCreateRoadISM(const std::string& meshPath);

	Map* map = nullptr;

	UPROPERTY()
	TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> roadMeshInstances;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> junctionMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> junctionMaterial;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> openingMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> openingMaterial;

	// 固定基础材质(编辑器可覆盖)，CONVENTIONS.md §4的ConstructorHelpers模式，
	// 和ForeverTerrainFrameworkComponent一致。
	UPROPERTY(EditDefaultsOnly, Category = "Roadnet")
	TObjectPtr<UMaterialInterface> roadPlainBaseMaterial;
};
