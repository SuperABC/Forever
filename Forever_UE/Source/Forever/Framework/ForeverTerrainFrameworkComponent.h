#pragma once

#define NOMINMAX

#include <vector>
#include <utility>

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
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

// 精细地形混合单元:权重最高的4个地形id及其权重
struct FFineCell {
	uint8 Ids[4] = { 0, 0, 0, 0 };
	float Weights[4] = { 0.f, 0.f, 0.f, 0.f };
};

// 某一级LOD mesh经过接缝修正后的四条边界顶点
struct FForeverLodBoundary {
	TArray<FVector> bottom; // vy=0,  vx=0..32
	TArray<FVector> top;    // vy=32, vx=0..32
	TArray<FVector> left;   // vx=0,  vy=0..32
	TArray<FVector> right;  // vx=32, vy=0..32
};

// 阶段4-1 Terrain落地:动态LOD(clipmap式)地形网格 + 海面网格。挖洞(construction格子的
// 洞)直接在网格生成时用LookupTerrain的矩形减矩形分解算法完成,不再用ISM摆放小立方体。
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

	// 将输入网格的地形混合数据下采样(分辨率翻倍)到输出网格
	void DownsampleFine(const TArray<FFineCell>& in, int32 inRes, TArray<FFineCell>& out) const;

	// 双线性采样地图高度(mapX/mapY为地图元素单位坐标),BuildLevel与出生点计算共用
	float SampleHeight(float mapX, float mapY) const;

	// construction/挖洞格子的实心地形几何:只有该格子是"construction"或者命中了hatch时才有
	// 输出。用精确的凸多边形裁剪(格子[0,1]x[0,1]依次按每个hatch矩形的4条边分割成"确定在矩形外
	// (实心)"/"还需要继续判断"两部分,详见.cpp实现)算出真正的实心地形区域(可能不止一块,也可能
	// 是这次会话之前"轴对齐外接矩形+角落补丁三角形"近似算法算不对的跨格hatch)，扇形三角化后
	// 输出，不再是BlueprintCallable(没有Blueprint/ISM消费方了)。
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

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> fineMaterial0;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> fineMaterial1;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> fineMaterial2;

	UPROPERTY()
	TObjectPtr<UTexture2D> fineIndexMap0;

	UPROPERTY()
	TObjectPtr<UTexture2D> finePowerMap0;

	UPROPERTY()
	TObjectPtr<UTexture2D> fineIndexMap1;

	UPROPERTY()
	TObjectPtr<UTexture2D> finePowerMap1;

	UPROPERTY()
	TObjectPtr<UTexture2D> fineIndexMap2;

	UPROPERTY()
	TObjectPtr<UTexture2D> finePowerMap2;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> oceanMaterial;

	// 固定的基础材质(编辑器可覆盖),CONVENTIONS.md §4的ConstructorHelpers模式
	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> terrainBaseMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> fineBaseMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain")
	TObjectPtr<UMaterialInterface> oceanBaseMaterial;

	// {{pivotX,pivotY}, size(地图元素单位)},下标即LOD等级(0最细)
	std::vector<std::pair<std::pair<int, int>, float>> currentPivots;
	std::vector<FForeverLodBoundary> lodBoundaries;
};
