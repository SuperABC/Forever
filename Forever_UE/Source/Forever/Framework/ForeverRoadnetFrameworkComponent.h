#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <utility>

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
class Connection;

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

	// 导航图debug可视化：车行/行人锚点各画一个小box(贴White材质/RoadPlain材质)，锚点之间的
	// 每条连接各画一条细的双面ribbon，仅在bShowNavigationDebug为true时才会真正生成几何
	// (为false时清空已有mesh section，不留旧数据)。公开成方法而不是只在GenerateRoadnet里
	// 跑一次，是因为以后Building域会在运行时用Map::AddRoadAccessNode之类的接口继续往
	// 导航图里加锚点/边——那时候需要能重新调用这个方法刷新可视化，检查新增的导航连接对不对，
	// 不能假设只有Roadnet自己生成时的那一份数据是唯一需要可视化的时机。
	void BuildNavigationDebugMesh();

private:
	// BuildNavigationDebugMesh的实现细节：给一张导航图(车行或行人)建出对应的debug mesh。
	void BuildNavGraphDebugMesh(const std::unordered_map<int, std::vector<std::pair<int, Connection*>>>& graph,
		UProceduralMeshComponent* mesh, UMaterialInstanceDynamic* material);

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

	// 导航图debug可视化开关——默认关闭(不影响正常运行时的常驻开销)，需要检查导航图连接/
	// 锚点是否正确时在编辑器里勾选，或以后Building域接完导航相关逻辑后临时打开核对。
	UPROPERTY(EditAnywhere, Category = "Roadnet|Debug")
	bool bShowNavigationDebug = true;

	// 车行导航debug用的纯白材质，和roadPlainBaseMaterial一样走ConstructorHelpers固定加载。
	UPROPERTY(EditDefaultsOnly, Category = "Roadnet|Debug")
	TObjectPtr<UMaterialInterface> whiteBaseMaterial;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> vehicleNavMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> vehicleNavMaterial;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> pedestrianNavMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> pedestrianNavMaterial;
};
