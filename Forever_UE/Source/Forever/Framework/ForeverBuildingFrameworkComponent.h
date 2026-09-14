#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "Containers/Queue.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "ForeverBuildingFrameworkComponent.generated.h"

class UProceduralMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class Map;
class Building;
class Floor;
struct FloorAssetSpec;

// 阶段4-1 Building落地 + 楼体footprint/楼层/两级LOD + 楼层内部布局：Building的可视化cube
// 收缩到BuildingMod声明的楼体子矩形，按basements/layers/floorHeights逐层堆叠(地下室往
// Z=0以下堆)。两级LOD：远处整栋一个box(材质由BuildingMod::lodMaterial指定，留空用默认
// 灰色)，继续用PMC section(buildingLodMesh，只有1个box，没有"频繁增删细节"的问题)；近处
// 是真正的楼层内部结构(走廊/房间隔墙/门洞/窗户/楼梯/电梯井/坡道)，每面墙体分段/地板/天花板
// slab/楼梯坡道网格都是一个独立的UStaticMeshComponent(不用PMC section也不用ISM——建筑楼体
// 要频繁整层增删，独立组件增删最直接，见ForeverBuildingFrameworkComponent.md)。切换距离
// lodSwitchDistance(地图单位)。LOD切换产生的mesh增删操作通过一个队列节流，TickComponent
// 每帧最多处理maxLodOpsPerTick条，避免大量建筑同时穿越阈值时卡顿。
UCLASS()
class FOREVER_API UForeverBuildingFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverBuildingFrameworkComponent();

	// 由AForeverFrameworkActor在Map::InitBuildings()跑完后调用一次。map生命周期由调用方持有。
	// 遍历map->GetBuildings()给每栋building分配LOD状态，同步建好远处灰色cube作为基线状态；
	// 不在这一步建任何近处楼层几何，近处楼层完全交给TickComponent按距离增量构建。
	void GenerateBuildings(Map* inMap);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// PIE停止/退出游戏时置空map——AForeverFrameworkActor::EndPlay同步delete了Core侧的Map
	// (含它拥有的所有Building/Room/Component)，但那是Actor自己的Map*字段，和这个组件
	// GenerateBuildings()时缓存的map是两个不同的变量，Actor那边delete之后并不会连带把这里
	// 也清空。这个组件的TickComponent每帧都会解引用renderStates里存的Building*(building->
	// GetPosX()等)，如果不清空，Actor::EndPlay delete map之后、这个组件真正被引擎销毁之前
	// 万一还有一帧Tick漏进来，就是踩野指针——退出游戏崩溃的根因(PIE验证发现)。
	// AActor::EndPlay会自动分发调用每个ActorComponent自己的EndPlay，且分发发生在
	// AForeverFrameworkActor::EndPlay里delete map那两行之后(Super::EndPlay(...)那一行)，
	// 所以这里置空的时候map已经不是野指针、可以放心比较/赋值，只是不能再解引用。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
		int32 nearFloorCount = 0;           // basements+layers，BuildFloorMesh按floorIndex 0..nearFloorCount-1处理
		int32 farSectionIndex = INDEX_NONE; // buildingLodMesh的section
		// 近处这栋building当前占用的所有独立组件(墙体分段/地板/天花板slab/楼梯/坡道/窗户网格)，
		// 按floorIndex分组，方便ClearNearSections只清空单层或整栋。
		TArray<TArray<TObjectPtr<UStaticMeshComponent>>> nearComponentsByFloor;
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
	UMaterialInstanceDynamic* ResolveMaterial(const std::string& softPath, UMaterialInstanceDynamic* fallback);
	UStaticMesh* ResolveMesh(const std::string& softPath, UStaticMesh* fallback);

	// 复用一个通用的单位立方体网格，缩放到目标尺寸摆一个墙体/地板/天花板slab；返回值已经
	// RegisterComponent+AddInstanceComponent，调用方负责收集进state.nearComponentsByFloor。
	UStaticMeshComponent* SpawnCube(float centerX, float centerY, float centerZ,
		float sizeX, float sizeY, float sizeZ, float rotation, UMaterialInterface* material);

	// 按目标尺寸缩放摆放一个网格实体，供楼梯/坡道这类有真实3D资产的元素用——和SpawnCube同一套
	// "资产包围盒是边长BUILDING_CUBE_MESH_SIZE的正方体"约定，缩放系数=目标尺寸/这个边长，
	// 保证不同美术资产只要包围盒统一就能互相替换，不用改代码。
	UStaticMeshComponent* SpawnMesh(float centerX, float centerY, float centerZ,
		float sizeX, float sizeY, float sizeZ, float rotation, UStaticMesh* mesh, UMaterialInterface* material);

	// 照抄老工程BuildingBase.cpp::ConstructQuad里processFace的算法：按这一侧墙的门/窗开口
	// 位置把墙体在水平方向切分成若干段(开口前的墙段/开口上方过梁(仅当开口没到天花板才有)/
	// 开口下方门槛或窗台(仅当开口没到地板才有))，每段一个SpawnCube；门/窗开口本身都不生成
	// 任何东西(窗户资产有问题，这次删掉了窗户网格显示逻辑，和门一样只是纯几何缺口)。
	// center/size是这面墙所在Floor局部坐标下的整面墙范围(BuildWallsForElement在
	// BuildFloorSection里按stair/elevator/ramp/corridor/single/row各自的矩形+墙标志调用
	// 4次，每次对应FACE_DIRECTION一侧)。worldRotation是building自身的世界旋转(弧度)，
	// 用来把局部坐标转成世界坐标摆放各个组件；建筑局部坐标到世界坐标的转换见
	// Building::LocalToWorld(Core侧只提供点转换，墙体分段的批量生成在这里现算，
	// 复用同一套cos/sin公式)。
	void BuildWallsForElement(Building* building, float floorBaseZ, float floorHeight,
		float elemCenterX, float elemCenterY, float elemSizeX, float elemSizeY,
		bool wallWest, bool wallEast, bool wallNorth, bool wallSouth,
		const std::unordered_map<int, std::vector<std::array<float, 8>>>& doors,
		const std::unordered_map<int, std::vector<std::array<float, 8>>>& windows,
		UMaterialInterface* wallMaterial, FBuildingRenderState& state, int32 floorIndex);

	Map* map = nullptr;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> buildingLodMesh; // 远处，每栋building一个section

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
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> lodMaterialCache; // 按软路径缓存MID(远处LOD/近处墙地顶材质共用一份缓存)

	UPROPERTY()
	TMap<FString, TObjectPtr<UStaticMesh>> meshCache; // 按软路径缓存UStaticMesh(楼梯/坡道网格)

	// 固定基础材质(编辑器可覆盖)，CONVENTIONS.md §4的ConstructorHelpers模式。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	TObjectPtr<UMaterialInterface> buildingBaseMaterial;

	// LOD切换距离，地图单位(1地图单位=1000 UE单位=10米)。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	float lodSwitchDistance = 20.f;

	// 主线程Tick每帧最多处理的LOD操作队列条数，避免大量建筑同时穿越阈值时卡顿。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	int32 maxLodOpsPerTick = 4;

	TMap<Building*, FBuildingRenderState> renderStates;
	TQueue<FBuildingLodOp> lodOpQueue;
	int32 nextFarSectionIndex = 0;
};
