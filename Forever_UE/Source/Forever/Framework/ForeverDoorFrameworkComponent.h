#pragma once

#include <vector>

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverDoorFrameworkComponent.generated.h"

class Door;
class Map;
class UForeverDoorComponent;

// 全图所有门(建筑门/房间门/园区大门)的流式加载：门从建筑LOD里独立出来，按玩家距离生成/销毁UForeverDoorComponent，
// 距离跟随市民的流式加载(移动中市民的生成/销毁距离，见ForeverPopulaceFrameworkComponent)，和建筑近/远LOD无关。
// 详见ForeverDoorFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverDoorFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverDoorFrameworkComponent();

	// 由AForeverFrameworkActor在建筑模型生成之后调用一次：按map->GetAllDoors()建空间网格，之后全靠Tick流式加载。
	void GenerateDoors(Map* inMap);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FDoorEntry {
		Door* door = nullptr;
		FVector location = FVector::ZeroVector; // 门洞中心(UE单位)
		TWeakObjectPtr<UForeverDoorComponent> component;
	};

	void SpawnDoor(FDoorEntry& entry);
	void DespawnDoor(FDoorEntry& entry);
	bool IsWorldGenerationDone() const;
	FIntPoint CellOf(const FVector& location) const;

	Map* map = nullptr;
	TArray<FDoorEntry> entries;
	// 空间网格：格子 -> entries下标，格子边长kGridCellSize。
	TMap<FIntPoint, TArray<int32>> grid;
	// 当前已生成组件的entries下标。
	TSet<int32> activeEntries;

	// 每帧最多生成几扇门：一扇门要建骨骼网格+动画实例+挡板+触发盒，比墙体/地板贵，批量进入范围时(传送、读档、
	// 切换控制)不能一帧全建。优先建离玩家最近的。
	UPROPERTY(EditDefaultsOnly, Category = "Door")
	int32 maxSpawnsPerFrame = 4;

	// 每帧最多销毁几扇门：销毁同样要反注册组件，一次性远离大片区域时也分帧处理。
	UPROPERTY(EditDefaultsOnly, Category = "Door")
	int32 maxDespawnsPerFrame = 8;

	// 没有人口组件可读时的后备距离(地图单位)：与市民默认值一致(移动中 2*2 / 4*2)。
	static constexpr float kFallbackSpawnDistance = 4.f;
	static constexpr float kFallbackDespawnDistance = 8.f;
	// 空间网格边长(UE单位)。
	static constexpr float kGridCellSize = 4000.f;

	// 每帧复用的候选缓冲，避免反复分配。
	TArray<TPair<float, int32>> spawnCandidates;
};
