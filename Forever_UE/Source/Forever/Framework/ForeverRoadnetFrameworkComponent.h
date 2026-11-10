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

// 路网渲染：沿每条Road按其mesh/unit用ISM重复摆放，路口mesh，开口cube，小路mesh，导航图debug可视化。
// 详见ForeverRoadnetFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverRoadnetFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverRoadnetFrameworkComponent();

	// 由AForeverFrameworkActor在Map::InitRoadnet()跑完后调用一次。map生命周期由调用方持有。
	void GenerateRoadnet(Map* inMap);

	// 导航图debug可视化：车行/行人锚点各画小box，连接画细ribbon；仅bShowNavigationDebug为true时生成几何
	// (为false时清空已有section)。公开是因为运行时往导航图加锚点/边后需要重新调用来刷新。
	void BuildNavigationDebugMesh();

private:
	// BuildNavigationDebugMesh的实现细节：给一张导航图(车行或行人)建出对应的debug mesh。
	void BuildNavGraphDebugMesh(const std::unordered_map<int, std::vector<std::pair<int, Connection*>>>& graph,
		UProceduralMeshComponent* mesh, UMaterialInstanceDynamic* material);

	// 沿road弧长按其mesh/unit重复摆放ISM实例，开口区间跳过(由BuildOpeningMeshes画cube)。
	// trimStart/trimEnd(地图单位)：该端连着真实路口时tiling往回收缩的距离，避免与路口mesh重叠z-fighting；
	// 端点是地图边缘则传0。铺不出一节实例的短缺口退化成扁平cube，写入和BuildOpeningMeshes共用的缓冲区。
	void BuildRoadInstances(Road* road, float trimStart, float trimEnd,
		TArray<FVector>& outVertices, TArray<int32>& outTriangles, TArray<FVector2D>& outUvs);

	// 每条road的每个开口生成一个扁平cube，追加进缓冲区(调用方汇总后一次性CreateMeshSection)。
	void BuildOpeningMeshes(Road* road, TArray<FVector>& outVertices, TArray<int32>& outTriangles, TArray<FVector2D>& outUvs);

	// 生成一块扁平cube追加进缓冲区：以road在centerT处为中心，沿切线长lengthAlongRoad、垂直方向宽crossWidth。
	// 开口cube和短缺口共用。
	void AppendFlatRoadCube(Road* road, float centerT, float lengthAlongRoad, float crossWidth,
		TArray<FVector>& outVertices, TArray<int32>& outTriangles, TArray<FVector2D>& outUvs);

	// 每个路口按approach.angle排序后的curb点围成多边形，从路口中心扇形三角剖分，贴RoadPlain材质。
	void BuildJunctionMeshes();

	// map->GetPathRoads()(裁剪Lot自动生成的小路)每条按起止点生成一个扁平ribbon，不走ISM管线(原因见.md)。
	// 材质优先用map->GetPathRoadMaterial()运行时加载，取不到退化用roadPlainBaseMaterial。
	void BuildPathRoadMeshes();

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

	// 固定基础材质(编辑器可覆盖)，构造函数里ConstructorHelpers加载，和ForeverTerrainFrameworkComponent一致。
	UPROPERTY(EditDefaultsOnly, Category = "Roadnet")
	TObjectPtr<UMaterialInterface> roadPlainBaseMaterial;

	// 导航图debug可视化开关，目前默认打开(核对导航图用)，不看时在编辑器里取消勾选以免常驻开销。
	UPROPERTY(EditAnywhere, Category = "Roadnet|Debug")
	bool bShowNavigationDebug = true;

	// 车行/行人导航debug共用的基础材质(Color参数默认白色)；行人MID把Color设成黑色以区分。
	UPROPERTY(EditDefaultsOnly, Category = "Roadnet|Debug")
	TObjectPtr<UMaterialInterface> pureBaseMaterial;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> vehicleNavMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> vehicleNavMaterial;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> pedestrianNavMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> pedestrianNavMaterial;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> pathRoadMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> pathRoadMaterial;
};
