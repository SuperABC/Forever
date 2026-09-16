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

// 阶段4-1 Building落地 + 楼体footprint/楼层/两级LOD + 楼层内部布局：Building的可视化cube
// 收缩到BuildingMod声明的楼体子矩形，按basements/layers/floorHeights逐层堆叠(地下室往
// Z=0以下堆)。两级LOD：远处整栋一个box(材质由BuildingMod::lodMaterial指定，留空用默认
// 灰色)；近处是真正的楼层内部结构(走廊/房间隔墙/门洞/窗户/楼梯/电梯井/坡道)，每面墙体分段/
// 地板/天花板slab/楼梯坡道网格都是一个独立的UStaticMeshComponent(不用PMC section也不用
// ISM——建筑楼体要频繁整层增删，独立组件增删最直接)。
//
// 这个组件本身不再直接持有任何per-building的渲染状态/组件——每栋building各有一个专属的
// ABuildingElement Actor(见Source/Forever/Element/BuildingElement.h)，LOD状态机、近/远
// 处组件、电梯轿厢动画全部在Element自己的Tick里做。这样每栋building的组件都attach到它自己
// 专属的Actor上，不会像早期实现那样全地图共用一个owner Actor(会被物理引擎的碰撞体焊接开销
// 拖累，且开销随owner身上组件总数线性增长，全地图共用就等于挨个building互相拖累，见
// ForeverBuildingFrameworkComponent.md"性能"一节)。
//
// 这个组件保留的职责：GenerateBuildings()时按map->GetBuildings()各SpawnActor一个Element；
// 集中加载/缓存全地图共用的默认资产(材质/网格，按软路径缓存，不适合每个Element各自维护一份)；
// 通过TryConsumeLodOpBudget()给所有Element提供一个全局共享的"每帧最多处理几条LOD操作"预算，
// 避免大量building同时穿越距离阈值时所有Element在同一帧一起疯狂建组件。
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
	// 远->近(建细节)的触发距离——比近->远的阈值小，两者不共用一个值(见下)，形成一段迟滞区间。
	float GetLodNearEnterDistance() const { return lodNearEnterDistance; }
	// 近->远(退化成单box)的触发距离——比远->近的阈值大。两个阈值之间(这次是20~40)是"迟滞区"：
	// 已经是Near的building要离得比刚进入时更远才会退回Far，避免玩家在临界距离附近小范围
	// 来回走动时，building反复Near/Far抖动式切换(每次切换都要建/删一整层楼的组件，抖动等于
	// 反复触发这次会话花大力气排查的那个卡顿)。
	float GetLodFarExitDistance() const { return lodFarExitDistance; }
	float GetCabinCruiseSpeed() const { return cabinCruiseSpeed; }
	float GetCabinEaseSeconds() const { return cabinEaseSeconds; }

	// 全地图共享的每帧LOD操作预算——每个ABuildingElement在自己的Tick里执行任何一条LOD操作
	// (建/删一层楼、建电梯轿厢等)之前都要先来这里申请一份，申请失败就等下一帧再试。预算本身
	// 每帧由这个组件的TickComponent重置一次，替代之前"框架组件自己维护一个全局队列、自己
	// 挨个执行"的做法——现在队列下放到每个Element自己维护，这里只保留节流的"总闸"。
	bool TryConsumeLodOpBudget() {
		if (frameOpBudgetRemaining <= 0) return false;
		frameOpBudgetRemaining--;
		return true;
	}

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
	float lodNearEnterDistance = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Building")
	float lodFarExitDistance = 40.f;

	// 全地图共享的每帧最多处理LOD操作条数，避免大量建筑同时穿越阈值时卡顿——具体消耗见
	// TryConsumeLodOpBudget()。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	int32 maxLodOpsPerTick = 1;

	int32 frameOpBudgetRemaining = 0;

	// 电梯轿厢匀速巡航速度(UE单位/秒，默认300≈3m/s)。用户要求"速度和加速度都变成2倍"——
	// ComputeCabinZ里缓入/缓出用的speedup(u)=cruiseSpeed*smoothstep(u)这条曲线，峰值加速度
	// =1.5*cruiseSpeed/cabinEaseSeconds，只要cabinEaseSeconds不变，cruiseSpeed翻倍就会让
	// 峰值加速度也自动翻倍(两者都正比于cruiseSpeed)，不需要额外再调cabinEaseSeconds——原来
	// 150(≈1.5m/s)翻倍成300(≈3m/s)。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	float cabinCruiseSpeed = 300.f;

	// 电梯轿厢两端缓入/缓出各自的时长(秒)，超过半程距离对应时间时会被自动夹到刚好半程，
	// 退化成"没有匀速段、只有缓入接缓出"，不会算出负的匀速时间。这次不改——上面cruiseSpeed
	// 翻倍已经让加速度自动翻倍，这个值再改会变成加速度4倍，超出"2倍"的要求。
	UPROPERTY(EditDefaultsOnly, Category = "Building")
	float cabinEaseSeconds = 1.5f;
};
