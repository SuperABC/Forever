#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"

#include <string>

#include "ForeverBuildingFrameworkComponent.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class Map;
class Building;

// 每栋Building各有一个专属ABuildingElement(Source/Forever/Element/BuildingElement.h)，两级LOD
// (远处单box/近处楼层内部结构)、电梯轿厢动画全在Element自己的Tick里做。这个组件只负责按
// map->GetBuildings()生成Element、集中缓存全地图共用的默认资产、提供全局共享的LOD操作预算
// 和"冻结世界"机制。设计与性能背景见ForeverBuildingFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverBuildingFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverBuildingFrameworkComponent();

	// 由AForeverFrameworkActor在Map::InitBuildings()跑完后调用一次。map生命周期由调用方持有。
	// 遍历map->GetBuildings()给每栋building各SpawnActor一个ABuildingElement并Init()——Element
	// 会自己同步建好远处灰色cube作为基线状态；不在这一步建任何近处楼层几何，近处楼层完全交给
	// 各Element自己的Tick按距离增量构建。
	void GenerateBuildings(Map* inMap);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// PIE停止/退出游戏时置空map——这个组件自己的TickComponent只用来重置每帧LOD操作预算，
	// 不解引用任何Building*/Map*，理论上不置空也不会崩，但和其它Framework组件保持同一套
	// 约定(见AForeverFrameworkActor::EnsureMapGenerated的Map生命周期说明)。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 供ABuildingElement调用——按BuildingMod软路径覆盖返回对应的材质/网格，路径为空或加载
	// 失败时回退到调用方传入的fallback(通常是下面几个Get默认值)。按软路径缓存，避免同一个
	// 软路径被不同building各自重复LoadObject。
	UMaterialInstanceDynamic* ResolveLodMaterial(Building* building);
	UMaterialInstanceDynamic* ResolveMaterial(const std::string& softPath, UMaterialInstanceDynamic* fallback);
	UStaticMesh* ResolveMesh(const std::string& softPath, UStaticMesh* fallback);

	// 全地图共用的默认资产——ABuildingElement在没有mod覆盖时用这些做ResolveMaterial/
	// ResolveMesh的fallback参数。
	UStaticMesh* GetCubeMesh() const { return cubeMesh; }
	UStaticMesh* GetDefaultStairMesh() const { return defaultStairMesh; }
	UStaticMesh* GetDefaultRampMesh() const { return defaultRampMesh; }
	UStaticMesh* GetDefaultCabinMesh() const { return defaultCabinMesh; }
	UMaterialInstanceDynamic* GetDefaultWallMaterial() const { return defaultWallMaterial; }
	UMaterialInstanceDynamic* GetDefaultFloorMaterial() const { return defaultFloorMaterial; }
	UMaterialInstanceDynamic* GetDefaultCeilingMaterial() const { return defaultCeilingMaterial; }

	// 共享配置，Element的Tick/动画计算直接读取。
	// 远->近的触发距离。比近->远的阈值小，两者之间是迟滞区，避免临界距离附近反复Near/Far切换。
	float GetLodNearEnterDistance() const { return lodNearEnterDistance; }
	// 近->远的触发距离。
	float GetLodFarExitDistance() const { return lodFarExitDistance; }
	float GetCabinCruiseSpeed() const { return cabinCruiseSpeed; }
	float GetCabinEaseSeconds() const { return cabinEaseSeconds; }

	// 每条"门批次"操作最多处理几扇门——门的构造比墙体/地板贵，拆成小批次、和楼层级操作分开节流。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	int32 maxDoorsPerOp = 4;
	int32 GetMaxDoorsPerOp() const { return maxDoorsPerOp; }

	// 全地图共享的每帧LOD操作预算：Element执行任何LOD操作前先来申请，失败就下一帧再试，预算每帧由
	// TickComponent重置。门级操作另有全局优先级门槛：全地图任意building还有楼层级操作排队
	// (pendingFloorTierOps>0)时，所有门级申请都被拒绝，不需要building互相感知。
	bool TryConsumeLodOpBudget(bool isDoorTierOp) {
		if (isDoorTierOp && pendingFloorTierOps > 0) return false;
		if (frameOpBudgetRemaining <= 0) return false;
		frameOpBudgetRemaining--;
		return true;
	}

	// 供ABuildingElement排队/执行楼层级操作(不含BuildDoorBatch)时成对调用，维护"排队中的楼层级操作条数"。
	// 和NotifyLodTransitionStarted/Finished(按转场计数，用于冻结世界)是两件事，不要混用。
	void NotifyFloorTierOpEnqueued(int32 count) { pendingFloorTierOps += count; }
	void NotifyFloorTierOpExecuted() { pendingFloorTierOps = FMath::Max(0, pendingFloorTierOps - 1); }

	// 冻结世界(全局时间倍率置0)直到所有building的LOD切换队列清空，TickComponent检测到清空后恢复1。
	// 由切换玩家控制权(ForeverStoryFrameworkComponent::ApplyControlChange)触发。Tick仍按正常帧率
	// 调用、只是DeltaTime缩放为0，而LOD队列的处理不读DeltaTime，所以冻结期间队列照常排空。
	void RequestFreezeUntilLodSettled();

	// 世界是否"视觉稳定"：没有处于RequestFreezeUntilLodSettled的冻结中，且没有building还在切换LOD。
	// 供开局黑屏(UFadeWidget)判断什么时候可以渐隐。
	bool IsLodSettled() const { return postGenerationFrames > kPostGenerationGateFrames && !bFrozenForLod && pendingLodTransitionCount <= 0; }

	// 世界生成流程(AForeverFrameworkActor::IsInitialGenerationDone)是否已经走完。生成期间玩家只是个
	// 过渡用的DefaultPawn，building/terrain/市民的Tick都要等生成完再开始干活(见各自的TickComponent)。
	bool IsWorldGenerationDone() const;

	// 供ABuildingElement在自己的Tick里transitionPending置true/false时同步调用，维护一个
	// 全局"还有多少栋building正在切换LOD"的计数——不需要框架组件反过来遍历所有Element逐个
	// 查询transitionPending，O(1)。
	void NotifyLodTransitionStarted();
	void NotifyLodTransitionFinished();

private:
	Map* map = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> defaultLodMaterial; // 远处默认灰色(Pure+Color染灰)

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> defaultWallMaterial; // 近处墙体默认白色(这次不分内外墙)

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> defaultFloorMaterial; // 近处地板默认白色

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> defaultCeilingMaterial; // 近处天花板默认白色

	UPROPERTY()
	TObjectPtr<UStaticMesh> defaultStairMesh; // 默认楼梯网格(/Game/Asset/Meshes/Stair.Stair)

	UPROPERTY()
	TObjectPtr<UStaticMesh> defaultRampMesh; // 默认坡道网格(/Game/Asset/Meshes/Ramp.Ramp)

	UPROPERTY()
	TObjectPtr<UStaticMesh> cubeMesh; // 单位立方体网格(/Game/Asset/Meshes/Cube.Cube)，SpawnCube用

	UPROPERTY()
	TObjectPtr<UStaticMesh> defaultCabinMesh; // 默认电梯轿厢网格(/Game/Asset/Meshes/Elevator.Elevator)

	UPROPERTY()
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> lodMaterialCache; // 按软路径缓存MID(远处LOD/近处墙地顶材质共用一份缓存)

	UPROPERTY()
	TMap<FString, TObjectPtr<UStaticMesh>> meshCache; // 按软路径缓存UStaticMesh(楼梯/坡道网格)

	// 固定基础材质(编辑器可覆盖)，CONVENTIONS.md §4的ConstructorHelpers模式。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	TObjectPtr<UMaterialInterface> buildingBaseMaterial;

	// LOD切换距离(地图单位，1地图单位=1000 UE单位=10米)，两个阈值不同形成迟滞区间，见
	// GetLodNearEnterDistance/GetLodFarExitDistance注释。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	float lodNearEnterDistance = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "Building")
	float lodFarExitDistance = 80.f;

	// 全地图共享的每帧最多处理LOD操作条数，避免大量建筑同时穿越阈值时卡顿——具体消耗见
	// TryConsumeLodOpBudget()。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	int32 maxLodOpsPerTick = 1;

	int32 frameOpBudgetRemaining = 0;

	// 全地图范围内还有多少条楼层级操作排队未执行——见TryConsumeLodOpBudget/
	// NotifyFloorTierOpEnqueued/Executed注释。
	int32 pendingFloorTierOps = 0;

	// 见RequestFreezeUntilLodSettled/NotifyLodTransitionStarted/Finished注释。
	int32 pendingLodTransitionCount = 0;
	bool bFrozenForLod = false;
	// 生成完成之后经过的帧数：生成完的那一帧，各ABuildingElement的Tick可能排在本组件TickComponent之后才把
	// 近处LOD排进队列，立刻判定"没有待处理的转场"会提前解冻/误报稳定，所以多等几帧。
	int32 postGenerationFrames = 0;
	static constexpr int32 kPostGenerationGateFrames = 2;

	// 电梯轿厢匀速巡航速度(UE单位/秒，默认300≈3m/s)。缓入/缓出速度曲线是cruiseSpeed*smoothstep(u)，
	// 加速度正比于cruiseSpeed/cabinEaseSeconds。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	float cabinCruiseSpeed = 300.f;

	// 电梯轿厢两端缓入/缓出各自的时长(秒)，超过半程时间会被夹到半程(没有匀速段，不会算出负时间)。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	float cabinEaseSeconds = 1.5f;
};
