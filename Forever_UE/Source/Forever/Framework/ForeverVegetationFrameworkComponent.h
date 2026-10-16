#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverVegetationFrameworkComponent.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class Map;

// 全图植被的统一渲染：每个StaticMesh一个HISM，所有树都是它的实例。GenerateVegetation一开始就把
// Core(Map::GetVegetation())里用到的mesh全部加载好、建好HISM，再批量添加初始实例；之后运行时
// 只通过AddTree/RemoveTree增删实例，不再创建组件。详见ForeverVegetationFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverVegetationFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverVegetationFrameworkComponent();

	// 由AForeverFrameworkActor在Map::InitBuildings跑完、地形生成之后调用一次。map生命周期由
	// 调用方持有。
	void GenerateVegetation(Map* inMap);

	// 运行时新增一棵树，mapX/mapY是地图element坐标，yaw是弧度，scale是最终缩放(不再叠加随机)，
	// Z按地形高度自动取。mesh不在预加载列表里时现场加载一次(会打Warning)。返回树id，失败返回-1。
	int32 AddTree(const FString& meshPath, float mapX, float mapY, float yaw, float scale);

	// 按id删除一棵树，id不存在返回false。
	bool RemoveTree(int32 treeId);

private:
	struct FTreeSlot {
		int32 meshIndex = INDEX_NONE;
		int32 instanceIndex = INDEX_NONE;
	};

	// 返回mesh在meshComponents里的下标，加载失败返回INDEX_NONE。
	int32 GetOrCreateMesh(const FString& meshPath);
	FTransform MakeTreeTransform(float mapX, float mapY, float yaw, float scale) const;
	void RegisterInstance(int32 treeId, int32 meshIndex, int32 instanceIndex);

	Map* map = nullptr; // 非持有,AForeverFrameworkActor拥有生命周期

	// HISM实例的剔除距离(UU)，超过End的树不渲染。
	UPROPERTY(EditDefaultsOnly, Category = "Vegetation")
	int32 cullDistanceStart = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Vegetation")
	int32 cullDistanceEnd = 60000;

	UPROPERTY()
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> meshComponents;

	TMap<FString, int32> meshIndexByPath;
	// treeId -> 它当前所在的HISM和实例下标
	TMap<int32, FTreeSlot> treeSlots;
	// 每个HISM的实例下标 -> treeId(RemoveAtSwap之后用来修正被换过来的那棵树)
	TArray<TArray<int32>> instanceTreeIds;
	int32 nextTreeId = 0;
};
