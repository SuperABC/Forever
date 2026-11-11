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

	// 沿road弧长按其mesh/unit重复摆放ISM实例，同时处理这条路上的开口(重叠的开口先合并成一段)：
	//  - 开口只在一侧、且这侧有人行道并配了无人行道资产(Road::GetSidewalkFreeMesh)：开口段换成一个拉伸到开口长度的
	//    无人行道资产(横向挪半个人行道宽对齐原点)，缺的人行道区域补一块路面材质矩形，另一侧人行道保留；
	//  - 其余(两侧都开口、该侧本来没人行道、资产没配/加载失败)：整条路宽换成一块扁平cube。
	// trimStart/trimEnd(地图单位)：该端连着真实路口时tiling往回收缩的距离，避免与路口mesh重叠z-fighting；
	// 端点是地图边缘则传0。铺不出一节实例的短缺口也退化成扁平cube。cube/矩形写进outVertices等缓冲区(调用方汇总后一次性CreateMeshSection)。
	void BuildRoadInstances(Road* road, float trimStart, float trimEnd,
		TArray<FVector>& outVertices, TArray<int32>& outTriangles, TArray<FVector2D>& outUvs);

	// 生成一块扁平矩形追加进缓冲区：以road在centerT处为中心，沿切线长lengthAlongRoad、垂直方向宽crossWidth，
	// lateralOffset(地图单位)是矩形中心相对road连线沿side0方向(右手垂线)的横向偏移，默认0=居中。
	// 开口cube、补人行道的矩形和短缺口共用。
	void AppendFlatRoadCube(Road* road, float centerT, float lengthAlongRoad, float crossWidth,
		TArray<FVector>& outVertices, TArray<int32>& outTriangles, TArray<FVector2D>& outUvs, float lateralOffset = 0.f);

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
	// 加载失败的道路资产路径，只警告一次，之后直接返回空(开口会退回整条路宽的扁平cube)。
	TSet<FString> failedRoadMeshes;

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
