#pragma once

#define NOMINMAX

#include <vector>
#include <utility>
#include <memory>
#include <cstdint>

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "map/terrain_blend.h"
#include "ForeverTerrainFrameworkComponent.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;
class UTexture2DArray;
class Map;

// 局部坐标(单位:地形格,1格=10m,相对格子左下角)三角形,construction/挖洞格子的实心地形
// 三角化输出用——按格子局部坐标[0,1]x[0,1]和hatch矩形做精确多边形裁剪(不再是"轴对齐外接矩形
// 减矩形+角落补丁三角形"的近似),任意凸多边形扇形三角化后都能用这一个统一的三角形结构表示,
// 不需要再分rects/tris两种特化形状。
struct FTerrainTri2D {
	FVector2D A, B, C;
};

// 某一级LOD mesh经过接缝修正后的四条边界顶点(每条边TERRAIN_LOD_GRID_SUBDIV+1个点,
// 见ForeverTerrainFrameworkComponent.cpp里这个常量)
struct FForeverLodBoundary {
	TArray<FVector> bottom; // vy=0
	TArray<FVector> top;    // vy=TERRAIN_LOD_GRID_SUBDIV
	TArray<FVector> left;   // vx=0
	TArray<FVector> right;  // vx=TERRAIN_LOD_GRID_SUBDIV
};

// 动态LOD(clipmap式)地形网格 + 海面网格。挖洞(construction格子的
// 洞)直接在网格生成时用LookupTerrain的凸多边形裁剪算法完成,不用ISM摆放小立方体。
// 详见ForeverTerrainFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverTerrainFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverTerrainFrameworkComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// 由AForeverFrameworkActor在Map::InitTerrains跑完后调用一次:创建LOD网格
	// 组件、构建材质/纹理数组、构建海面网格。map生命周期由调用方(AForeverFrameworkActor)持有。
	void GenerateTerrain(Map* inMap);

	// 地图正中心的世界坐标(含地形高度采样,向上留出安全余量),供ForeverGameMode计算出生点用。
	FVector GetMapCenterWorldLocation() const;

	bool IsTerrainGenerated() const { return map != nullptr; }

private:
	// edgeFlags: bit0=左边与父级重合, bit1=右边, bit2=下边, bit3=上边
	void BuildLevel(int levelIdx, std::pair<int, int> pos, float size, int childPos,
		const FForeverLodBoundary* parentBoundary, int parentOffsetX, int parentOffsetY, int edgeFlags);

	// 构建覆盖所有"ocean"地形格的海面网格
	void BuildOceanMesh();

	// 重建近处混合控制纹理(NearIndexMap/NearWeightMap)，覆盖范围是以(originX,originY)为
	// 左下角、边长sizeElements(地图元素单位)的正方形区域——只在nearControlLevel那一级LOD
	// 自己的pivot变化时调用，见ForeverTerrainFrameworkComponent.md"近处地形混合"一节。
	void RebuildNearControl(float originX, float originY, int32 sizeElements);

	// 双线性采样地图高度(mapX/mapY为地图元素单位坐标),BuildLevel与出生点计算共用
	float SampleHeight(float mapX, float mapY) const;

	// construction/挖洞格子的实心地形几何:只有该格子是"construction"或者命中了hatch时才有
	// 输出。用精确的凸多边形裁剪(格子[0,1]x[0,1]依次按每个hatch矩形的4条边分割成"确定在矩形外
	// (实心)"/"还需要继续判断"两部分,详见.cpp实现)算出真正的实心地形区域(可能不止一块,比如
	// 一条跨格的hatch可能把格子劈成两侧独立的两块)，扇形三角化后
	// 输出，不是BlueprintCallable(没有Blueprint/ISM消费方)。
	void LookupTerrain(int elemX, int elemY, FString& type, float& height,
		TArray<FTerrainTri2D>& tris) const;

	Map* map = nullptr; // 非持有,AForeverFrameworkActor拥有生命周期

	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> gridMeshes;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> oceanMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> terrainMaterial;

	UPROPERTY()
	TObjectPtr<UTexture2DArray> terrainDiffuseArray;

	UPROPERTY()
	TObjectPtr<UTexture2D> terrainIndexMap;

	// 法线+粗糙度打包数组(BGRA8,sRGB关,R/G=法线xy,B=粗糙度,A=255)，和terrainDiffuseArray
	// 槡位对齐，近/远两个材质共用一份。
	UPROPERTY()
	TObjectPtr<UTexture2DArray> terrainNormalRoughArray;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> oceanMaterial;

	// 固定的基础材质(编辑器可覆盖),CONVENTIONS.md §4的ConstructorHelpers模式
	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> terrainBaseMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> oceanBaseMaterial;

	// 近处(细LOD)材质和它的控制纹理——见terrain_blend.md/ForeverTerrainFrameworkComponent.md
	// "近处地形混合"一节。
	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Near")
	int32 nearControlLevel = 3; // 控制纹理对应哪一级LOD的覆盖区域，夹到[0,num-1]；从2调到3是为了把near/far缝合线挪远，缓解K在两种材质里算法不一致导致的法线接缝（见ForeverTerrainFrameworkComponent.md）

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Near")
	int32 nearTexelsPerElement = 16;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Near")
	float nearBlendRadius = 2.f; // 夹到[1,3]，见NearBlendParams；从1.5调到2加宽近处混合过渡带

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Near")
	TObjectPtr<UMaterialInterface> nearBaseMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> nearMaterial;

	UPROPERTY()
	TObjectPtr<UTexture2D> nearIndexMap;

	UPROPERTY()
	TObjectPtr<UTexture2D> nearWeightMap;

	std::unique_ptr<NearTerrainBlend> nearBlend;
	// 整张地图的槡位id/construction标记，GenerateTerrain里填一次，RebuildNearControl每次
	// 调用NearTerrainBlend::BuildRegion时复用，不用每次重新扫全图。
	std::vector<uint8_t> nearSliceIds, nearIsConstruction;
	// 当前控制纹理覆盖区域的左下角(地图元素坐标，取整)，RebuildNearControl只在这个值真的
	// 变化时才重建纹理。
	std::pair<int, int> nearRegionPivot{ INT_MIN, INT_MIN };

	// {{pivotX,pivotY}, size(地图元素单位)},下标即LOD等级(0最细)
	std::vector<std::pair<std::pair<int, int>, float>> currentPivots;
	std::vector<FForeverLodBoundary> lodBoundaries;
};
