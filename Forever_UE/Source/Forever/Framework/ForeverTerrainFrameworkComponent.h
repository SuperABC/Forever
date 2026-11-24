#pragma once

#define NOMINMAX

#include <vector>
#include <utility>
#include <memory>
#include <cstdint>

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverTerrainFrameworkComponent.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class Map;

// 某一级LOD mesh经过接缝修正后的四条边界顶点(每条边TERRAIN_LOD_GRID_SUBDIV+1个点,
// 见ForeverTerrainFrameworkComponent.cpp里这个常量)
struct FForeverLodBoundary {
	TArray<FVector> bottom; // vy=0
	TArray<FVector> top;    // vy=TERRAIN_LOD_GRID_SUBDIV
	TArray<FVector> left;   // vx=0
	TArray<FVector> right;  // vx=TERRAIN_LOD_GRID_SUBDIV
};

// 动态LOD(clipmap式)地形网格 + 隧道口侧壁(水面见ForeverWaterFrameworkComponent)。挖洞在网格生成时用凸多边形裁剪完成。
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
	// 组件、构建材质/纹理数组、构建隧道口侧壁。map生命周期由调用方(AForeverFrameworkActor)持有。
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

	// 隧道口侧壁mesh：对Map::GetTunnelWallHatches()的每个hatch在洞的两条长边各做一条竖直的墙(有厚度，
	// 内面可见、可碰撞)，其它hatch没有墙，见.cpp注释。
	void BuildTunnelWalls();

	// 采样地图高度(mapX/mapY为地图元素单位坐标),转发Map::SampleHeight(含细化高度),BuildLevel与出生点计算共用
	float SampleHeight(float mapX, float mapY) const;

	Map* map = nullptr; // 非持有,AForeverFrameworkActor拥有生命周期

	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> gridMeshes;

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

	// 固定的基础材质M_Terrain(编辑器可覆盖),构造函数里ConstructorHelpers加载。材质实例的全部纹理/参数由FTerrainMaterialBuilder创建。
	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> terrainBaseMaterial;

	// 每个element的最近水面高度(见FTerrainMaterialBuilder::BuildWaterLevelField)，BuildLevel用它给顶点算水深(顶点色R)。
	std::vector<float> waterLevelField;
	int32 waterFieldWidth = 0;

	// 顶点处的水深(米，正=在水下)，没有水返回WaterDepthMinMeters。mapX/mapY是element坐标，height是该点地面高度(element)。
	float SampleWaterDepthMeters(float mapX, float mapY, float height) const;

	// {{pivotX,pivotY}, size(地图元素单位)},下标即LOD等级(0最细)
	std::vector<std::pair<std::pair<int, int>, float>> currentPivots;
	std::vector<FForeverLodBoundary> lodBoundaries;
};
