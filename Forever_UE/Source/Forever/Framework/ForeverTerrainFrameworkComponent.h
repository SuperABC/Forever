#pragma once

#define NOMINMAX

#include <vector>
#include <utility>
#include <memory>
#include <cstdint>

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "map/terrain.h"
#include "ForeverTerrainFrameworkComponent.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;
class UTexture2DArray;
class Map;

// 局部坐标(单位:地形格,1格=10m,相对格子左下角)三角形:construction/挖洞格子的实心地形
// 经精确多边形裁剪、扇形三角化后的输出。
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

// 动态LOD(clipmap式)地形网格 + 水面网格 + 隧道口侧壁。挖洞在网格生成时用凸多边形裁剪完成。
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

	// 地图正中心的世界坐标(含地形高度采样,向上留出安全余量),供ForeverPlayerGameMode计算出生点用。
	FVector GetMapCenterWorldLocation() const;

	bool IsTerrainGenerated() const { return map != nullptr; }

	// worldPos(UE世界坐标)是否在第level级LOD(0最细)当前覆盖的正方形内。scale<1把范围按中心缩小（做进入/离开
	// 的滞回用）。LOD还没建好/level越界返回false。物理不可靠的远处判断用，见ForeverTrafficFrameworkComponent
	// 的公交远近切换。
	bool IsInsideLodLevel(const FVector& worldPos, int32 level, float scale = 1.f) const;

private:
	// edgeFlags: bit0=左边与父级重合, bit1=右边, bit2=下边, bit3=上边
	void BuildLevel(int levelIdx, std::pair<int, int> pos, float size, int childPos,
		const FForeverLodBoundary* parentBoundary, int parentOffsetX, int parentOffsetY, int edgeFlags);

	// 构建覆盖所有"ocean"地形格的海面网格
	void BuildOceanMesh();

	// 隧道口侧壁mesh：对Map::GetTunnelWallHatches()的每个hatch在洞的两条长边各做一条竖直的墙(有厚度，
	// 内面可见、可碰撞)，其它hatch没有墙，见.cpp注释。
	void BuildTunnelWalls();

	// 重建近处混合控制纹理(NearIndexMap/NearWeightMap)，覆盖范围是以(originX,originY)为
	// 左下角、边长sizeElements(地图元素单位)的正方形区域——只在nearControlLevel那一级LOD
	// 自己的pivot变化时调用，见ForeverTerrainFrameworkComponent.md"近处地形混合"一节。
	void RebuildNearControl(float originX, float originY, int32 sizeElements);

	// 采样地图高度(mapX/mapY为地图元素单位坐标),转发Map::SampleHeight(含细化高度),BuildLevel与出生点计算共用
	float SampleHeight(float mapX, float mapY) const;

	// construction/有hatch格子的实心地形几何(精确凸多边形裁剪+扇形三角化，可能不止一块，算法见.md)。
	// 格子既不是"construction"也没有hatch时没有输出。
	void LookupTerrain(int elemX, int elemY, FString& type,
		TArray<FTerrainTri2D>& tris) const;

	Map* map = nullptr; // 非持有,AForeverFrameworkActor拥有生命周期

	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> gridMeshes;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> oceanMesh;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> tunnelWallMesh;

	// 侧壁材质：地形材质按世界XY投影采样，竖直面上是拉伸的竖条纹，所以侧壁用单色Pure材质(Color参数)；
	// 以后有三平面/UV贴图的材质时换这个。
	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> tunnelWallBaseMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> tunnelWallMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> terrainMaterial;

	UPROPERTY()
	TObjectPtr<UTexture2DArray> terrainDiffuseArray;

	UPROPERTY()
	TObjectPtr<UTexture2D> terrainIndexMap;

	// 法线+粗糙度打包数组(BGRA8,sRGB关,R/G=法线xy,B=粗糙度,A=255)，和terrainDiffuseArray
	// 槽位对齐，近/远两个材质共用一份。
	UPROPERTY()
	TObjectPtr<UTexture2DArray> terrainNormalRoughArray;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> oceanMaterial;

	// 固定的基础材质(编辑器可覆盖),构造函数里ConstructorHelpers加载
	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> terrainBaseMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> oceanBaseMaterial;

	// 近处(细LOD)材质和它的控制纹理——见terrain.md和.md的"近处地形混合"一节。
	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Near")
	int32 nearControlLevel = 3; // 控制纹理对应哪一级LOD的覆盖区域，夹到[0,num-1]；取大一些把near/far缝合线挪远，缓解K在两种材质里算法不一致导致的法线接缝（见.md）

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Near")
	int32 nearTexelsPerElement = 16;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Near")
	float nearBlendRadius = 2.f; // 夹到[1,3]，见NearBlendParams；越大近处混合过渡带越宽

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Near")
	TObjectPtr<UMaterialInterface> nearBaseMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> nearMaterial;

	UPROPERTY()
	TObjectPtr<UTexture2D> nearIndexMap;

	UPROPERTY()
	TObjectPtr<UTexture2D> nearWeightMap;

	std::unique_ptr<NearTerrainBlend> nearBlend;
	// 整张地图的槽位id/construction标记，GenerateTerrain里填一次，RebuildNearControl每次
	// 调用NearTerrainBlend::BuildRegion时复用，不用每次重新扫全图。
	std::vector<uint8_t> nearSliceIds, nearIsConstruction;
	// 当前控制纹理覆盖区域的左下角(地图元素坐标，取整)，RebuildNearControl只在这个值真的
	// 变化时才重建纹理。
	std::pair<int, int> nearRegionPivot{ INT_MIN, INT_MIN };

	// {{pivotX,pivotY}, size(地图元素单位)},下标即LOD等级(0最细)
	std::vector<std::pair<std::pair<int, int>, float>> currentPivots;
	std::vector<FForeverLodBoundary> lodBoundaries;
};
