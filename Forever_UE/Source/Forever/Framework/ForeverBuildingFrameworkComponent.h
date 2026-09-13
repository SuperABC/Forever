#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "Containers/Queue.h"
#include "ForeverBuildingFrameworkComponent.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class Map;
class Building;

// 阶段4-1 Building落地 + 楼体footprint/楼层/两级LOD：Building的可视化cube收缩到
// BuildingMod声明的楼体子矩形(不再填满整个落地Quad)，按basements/layers/floorHeights
// 逐层堆叠(地下室往Z=0以下堆)。两级LOD：近处每层一个cube(白色，贴buildingMaterial)，
// 远处整栋一个cube(材质由BuildingMod::lodMaterial指定，留空用默认灰色)，切换距离
// lodSwitchDistance(地图单位)。LOD切换产生的mesh增删操作通过一个队列节流，TickComponent
// 每帧最多处理maxLodOpsPerTick条，避免大量建筑同时穿越阈值时卡顿。详见
// ForeverBuildingFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverBuildingFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverBuildingFrameworkComponent();

	// 由AForeverFrameworkActor在Map::InitBuildings()跑完后调用一次。map生命周期由调用方持有。
	// 遍历map->GetBuildings()给每栋building分配LOD状态+section下标，同步建好远处灰色cube
	// 作为基线状态；不在这一步建任何近处楼层mesh，近处楼层完全交给TickComponent按距离
	// 增量构建。
	void GenerateBuildings(Map* inMap);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	enum class EBuildingLod : uint8 { Near, Far };

	enum class EBuildingLodOpType : uint8 {
		BuildFarMesh,        // 近->远，第1步
		DeleteAllNearMeshes, // 近->远，最后一步(执行完才更新currentLod)
		BuildFloorMesh,      // 远->近，每层一条
		DeleteFarMesh,       // 远->近，最后一步(执行完才更新currentLod)
	};

	struct FBuildingRenderState {
		EBuildingLod currentLod = EBuildingLod::Far;
		bool transitionPending = false;
		int32 nearSectionBase = INDEX_NONE; // buildingMesh的section = nearSectionBase+floorIndex
		int32 nearFloorCount = 0;           // basements+layers
		int32 farSectionIndex = INDEX_NONE; // buildingLodMesh的section
	};

	struct FBuildingLodOp {
		EBuildingLodOpType type = EBuildingLodOpType::BuildFarMesh;
		Building* building = nullptr;
		int32 floorIndex = -1; // 仅BuildFloorMesh使用
	};

	void BuildFarSection(Building* building, FBuildingRenderState& state);
	void BuildFloorSection(Building* building, FBuildingRenderState& state, int32 floorIndex);
	void ClearNearSections(Building* building, FBuildingRenderState& state);
	void ClearFarSection(Building* building, FBuildingRenderState& state);
	void ExecuteLodOp(const FBuildingLodOp& op);
	UMaterialInstanceDynamic* ResolveLodMaterial(Building* building);

	Map* map = nullptr;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> buildingMesh; // 近处/楼层，每栋building一段连续section区间

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> buildingLodMesh; // 远处，每栋building一个section

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> buildingMaterial; // 近处白色(不变)

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> defaultLodMaterial; // 远处默认灰色(Pure+Color染灰)

	UPROPERTY()
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> lodMaterialCache; // 按BuildingMod::lodMaterial路径缓存

	// 固定基础材质(编辑器可覆盖)，CONVENTIONS.md §4的ConstructorHelpers模式。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	TObjectPtr<UMaterialInterface> buildingBaseMaterial;

	// LOD切换距离，地图单位(1地图单位=1000 UE单位=10米)。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	float lodSwitchDistance = 20.f;

	// 主线程Tick每帧最多处理的LOD操作队列条数，避免大量建筑同时穿越阈值时卡顿。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	int32 maxLodOpsPerTick = 8;

	TMap<Building*, FBuildingRenderState> renderStates;
	TQueue<FBuildingLodOp> lodOpQueue;
	int32 nextNearSectionIndex = 0;
	int32 nextFarSectionIndex = 0;
};
