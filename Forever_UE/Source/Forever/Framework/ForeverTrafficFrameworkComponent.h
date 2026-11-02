#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include <string>
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverTrafficFrameworkComponent.generated.h"

class APlayerController;
class AVehicleElement;
class ATransitVehicleElement;
class ABusController;
class Station;
class Route;
class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class Map;
class Traffic;
class Vehicle;

// 载具系统落地：车辆不再按T临时生成/销毁，改成开局时按停车位预置生成(GenerateVehicles)，
// 玩家走近车辆用MeetOption选"上车"（见UForeverStoryFrameworkComponent::ApplyEnterVehicle→
// ApplyEnterVehicle），驾驶时按Q下车（AVehicleElement::ExitVehicle→ExitVehicle，带下车点
// 碰撞检测）。详见ForeverTrafficFrameworkComponent.md。
//
// 这次新增公共交通(大巴/火车/飞机)：GenerateVehicles额外给GetRoute()非空的Vehicle生成
// ATransitVehicleElement(运动学立方体占位，不能上下车)；BuildRouteDebugMesh/BuildTracks
// 是公共交通专属的调试画线/铺轨道，见public_transport_plan.md。
UCLASS()
class FOREVER_API UForeverTrafficFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverTrafficFrameworkComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// 由AForeverFrameworkActor::EnsureTrafficGenerated()在traffic->Init(map)之后立刻调用一次：
	// 遍历traffic->GetVehicles()，category=="car"(GetRoute()为空)的按原逻辑生成AVehicleElement
	// (照抄BuildingElement.cpp::ComputeWorldPosition公式换算世界坐标、
	// LoadClass<AVehicleElement>(vehicle->GetBlueprintPath())、SpawnActor、Init、建proximity
	// box，存进activeVehicles供ApplyEnterVehicle按名字反查)；GetRoute()非空的(公共交通车辆)
	// 生成ATransitVehicleElement(不需要换算停车位世界坐标，Tick里直接读vehicle->GetTransform()
	// 驱动，初始位置无所谓，存进activeTransitVehicles)。
	// 注意：非公共交通的车辆(AVehicleElement)不在这里直接生成，只登记停放位置(carEntries)，由
	// TickComponent按玩家距离流式生成/销毁，和市民(UForeverPopulaceFrameworkComponent)同一个
	// 思路：停着的车是带物理模拟的骨骼网格+Chaos车辆组件+触发盒，全城一次性生成几百辆对游戏
	// 线程负担很重。
	void GenerateVehicles(Map* map, Traffic* traffic);

	// EnterVehicleChange的真正执行：按名字在activeVehicles里找到AVehicleElement，隐藏+
	// despawn-exempt当前pawn、记进vehicleElement的previousPawn，Possess过去。找不到车辆/
	// 当前没有pawn时打一条Warning，不崩溃。
	// bSilent=true时不给车辆script发enter_vehicle事件(读档恢复"正在开的车"：script变量/选项已随存档
	// 恢复，再发一次事件会让script重复处理)。
	void ApplyEnterVehicle(const FString& vehicleName, APlayerController* controller, bool bSilent = false);

	// board_vehicle Change的真正执行：搭乘线路上的公交(ATransitVehicleElement)。和ApplyEnterVehicle
	// 同一套(隐藏原pawn、记previousPawn、Possess)，区别是搭乘者只有鼠标视角、车自己跑。bSilent同上。
	void ApplyBoardVehicle(const FString& vehicleName, APlayerController* controller, bool bSilent = false);

	// leave_vehicle / force_leave Change的真正执行：玩家下车(驾驶和搭乘都行)。vehicleName为空=玩家当前
	// 所在的任何载具，非空=只在玩家正在这辆里才执行。bForce=true(force_leave)时驾驶者也不做遮挡判定。
	void ApplyLeaveVehicle(const FString& vehicleName, APlayerController* controller, bool bForce);

	// 线路上的公交Actor(按Vehicle::GetName()查)，找不到返回nullptr。
	ATransitVehicleElement* FindTransitElement(const FString& vehicleName) const;

	// ---- 公交 Controller 驱动（见 Controller/BusController、ForeverTrafficFrameworkComponent.md） ----
	// 线路公交：远处（地形 busPhysicsLodLevel 级 LOD 范围外）是运动学的 ATransitVehicleElement（Core 时刻表），
	// 范围内换成物理 AVehicleElement + ABusController 按站序循环；停车场公交由"测试运行"交给 ABusController。

	// 公交 Controller 的进站/出站（物理段由 Controller 判定，运动学段由 Core 时刻表发，同一时刻只有一个发出方）。
	// stationNodeId 是 Map 辅助导航节点 id（Station 的 stationNode）。
	void FireStopEvent(Vehicle* vehicle, int32 stationNodeId, bool entering);

	// vehicle_command Change 的执行：测试运行 = 随机选一条公交环线的随机站，先导航过去再开始循环；
	// 停止运行 = 刹停并保持（线路公交同时退出时刻表，由 Controller 继续持有）。
	void StartBusTestRun(const FString& vehicleName);
	void StopBusRun(const FString& vehicleName);

	// 公交（线路公交处于物理段时）的物理 Actor，没有返回 nullptr。
	AVehicleElement* FindBusPhysicsElement(const FString& vehicleName) const;
	// 线路公交是否当前是物理段 Actor；搭乘需要。不在物理段时强制切换（不看距离），失败返回 nullptr。
	AVehicleElement* EnsureBusPhysicsElement(const FString& vehicleName);
	// 这辆车是不是线路公交（有物理类、能做远近切换）。
	bool IsSwitchableRouteBus(const FString& vehicleName) const;

	// 线路公交在这个地形 LOD 级别内用物理（默认 4：LOD4 范围外 Chaos 地形碰撞太粗糙，物理不可靠）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Bus")
	int32 busPhysicsLodLevel = 4;
	// 进入物理的范围系数（<1，滞回：进入用缩小的范围，离开用完整范围，避免边界抖动）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Bus")
	float busPhysicsEnterScale = 0.9f;

	// 这辆车当前在场景里的Actor(停着/私家车；公交车没有交互选项，不在这张表里)，找不到返回nullptr。
	AVehicleElement* FindVehicleElement(const FString& vehicleName) const;

	// 按名字找这辆车的Actor，当前没有生成(离玩家太远)就不看距离强制生成一个——剧情的
	// enter_vehicle/读档恢复"正在开的车"指定的车不一定在玩家附近。找不到这辆车返回nullptr。
	AVehicleElement* FindOrSpawnVehicle(const FString& vehicleName);

	// AVehicleElement::ExitVehicle按Q调用：算出候选下车世界坐标(车辆当前transform+
	// VehicleMod的exitOffsetX/Y/Z，按车身当前旋转变换)，用ECC_Pawn通道做胶囊体sweep检测
	// (见[[memory:pawn_preset_ignores_visibility]]，不能用ECC_Visibility；也不能用
	// OverlapAnyTestByChannel，建筑/房间/园区边界盒是Trigger档案，重叠类查询会把它们也算成
	// "挡住"，见ExitVehicle实现里的说明)，被挡住就在左上角打印拒绝提示、不下车；没被挡住就
	// 恢复previousPawn（位置/显示/碰撞/Possess），车辆本身不销毁(这次是预置的真实物件，不是
	// 一次性测试对象)。
	// 搭乘者(ATransitVehicleElement)也走这条流程，但不做遮挡判定；bForce=true时驾驶者也不做。
	void ExitVehicle(APlayerController* controller, bool bForce = false);

	// script的test_navigate_vehicle Change（和市民的test_navigate_citizen对称）：用橙色折线显示这辆车
	// 当前的行驶路线，数据来自AVehicleCommandController::GetRemainingWaypoints，不给车发任何命令。
	// 折线跟着车走、越走越短，一直显示到这条寻路任务完成（见TrackTestNavigationLine）。车没有在开就什么都不做。
	void StartTestNavigation(const FString& vehicleName);

	// 清掉导航调试折线并停止跟踪。
	void ClearTestNavigationLine();

	// 跟踪显示一条路线（车辆和市民的导航测试共用同一条调试线，同一时刻只显示最后一次）：约每0.1秒调一次
	// source取剩余路径重画，source返回false（任务完成/对象没了）就清线并停止。没有计时器，不会自己消失。
	void TrackTestNavigationLine(TFunction<bool(TArray<FVector>&)> source);

	// 公共交通线路的调试画线：每种stationType一个UProceduralMeshComponent+MID(bus绿/train蓝/
	// plane红/其它默认色)，按route->GetLines()里每条腿的edge几何采样连成ribbon，边中点画一个
	// 箭头指示行驶方向，站点接口画小方块。由bShowRouteDebug控制，关闭时清空已有section而不是
	// 跳过(避免旧可视化残留)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Debug")
	bool bShowRouteDebug = true;

	void BuildRouteDebugMesh(Traffic* traffic);

	// 铺instantiate线路(火车)的轨道mesh——只处理ShouldInstantiate()&&!GetTrackMesh().empty()的线路，
	// 思路同ForeverRoadnetFrameworkComponent的BuildRoadInstances(按弧长重复摆放
	// InstancedStaticMeshComponent)，区别是轨道mesh的轴心在起点边(沿X轴向前延伸)，所以每块摆在
	// 自己的起点而不是中点；隧道段换用GetTrackTunnelMesh()(空则复用GetTrackMesh())。
	void BuildTracks(Traffic* traffic);

	// 车辆流式生成/销毁的距离(地图单位)，是市民(citizenSpawnDistance 2.0/citizenDespawnDistance 4.0)
	// 的两倍；判断距离时Z方向放大5倍(不同楼层的东西一般互相看不到)，和市民同一个度量。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	float vehicleSpawnDistance = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	float vehicleDespawnDistance = 8.f;

	// 每帧最多生成几辆车(车是物理模拟的骨骼网格+Chaos组件，生成成本比市民还高)，超过就等下一帧。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	int32 maxVehicleSpawnsPerFrame = 1;

private:
	// 公交运行状态：线路公交（bRouteBus）和跑过"测试运行"的停车场公交共用。element/controller 是弱引用，
	// Actor 的生命周期归世界。
	struct FBusState {
		Vehicle* vehicle = nullptr;
		FString name;
		bool bRouteBus = false;                 // 有 Core 时刻表的线路公交
		bool bDetached = false;                 // 线路公交已退出时刻表（停止运行/测试运行过），由 Controller 持有
		bool bNoPhysics = false;                // 没有物理类/没有站序，永远运动学
		UClass* physicsClass = nullptr;         // AVehicleElement 蓝图子类，线路公交的物理表示
		TWeakObjectPtr<ATransitVehicleElement> kinematic;
		TWeakObjectPtr<AVehicleElement> element;
		TWeakObjectPtr<ABusController> controller;
	};
	TMap<FString, FBusState> busStates;
	// FBusState::physicsClass 是裸指针，LoadClass 出来的蓝图类没有别的引用，GC 后会悬空（SpawnActor 时崩溃），
	// 所以另存一份 UPROPERTY 让 GC 看得见。
	UPROPERTY()
	TSet<TObjectPtr<UClass>> busPhysicsClasses;
	TMap<int32, Station*> stationByNodeId; // 辅助导航节点 id -> 车站，FireStopEvent 用

	void UpdateBusPhysicsHandoff();
	bool BusToPhysics(FBusState& state);
	void BusToKinematic(FBusState& state);
	ABusController* SpawnBusController(AVehicleElement* element);
	ABusController* GetOrCreateBusController(FBusState& state);
	FBusState* FindOrAddBusState(const FString& vehicleName);
	// 停车场公交/已脱离时刻表的公交是否正被 Controller 持续驱动（流式销毁时不能销毁）。
	bool IsDrivenByController(const AVehicleElement* element) const;
	// 收集所有公交环线：(route,line) 及其站序节点 id。
	void CollectBusLoops(TArray<TArray<int32>>& outLoops) const;

	Map* mapRef = nullptr;
	Traffic* trafficRef = nullptr;

	// 玩家进出载具(驾驶或搭乘)时给这辆车自己的script发enter_vehicle/leave_vehicle事件。
	void FireVehicleEvent(Vehicle* vehicle, bool entering, const char* mode);

	// 一辆非公共交通车辆的登记项：不管当前有没有生成Actor都存在。location/yaw是它最后一次已知
	// 的位置(没生成时=停车位或存档/上次销毁时的位置；生成着时每帧同步)。不能标UPROPERTY：
	// Vehicle*是Core层类型。
	struct FCarEntry {
		Vehicle* vehicle = nullptr;
		FString name;
		FString blueprintPath;
		FVector location = FVector::ZeroVector;
		float yaw = 0.f;
		bool bBroken = false; // 加载蓝图失败过，不再重试(避免每帧刷警告)
		TWeakObjectPtr<AVehicleElement> element;
	};
	TArray<FCarEntry> carEntries;

	// 按登记项生成Actor并记进activeVehicles，失败返回nullptr。
	AVehicleElement* SpawnCar(FCarEntry& entry);

	// Vehicle::GetName()→对应的AVehicleElement，照抄
	// UForeverPopulaceFrameworkComponent::activeInstances的思路，ApplyEnterVehicle按名字
	// 反查用。
	UPROPERTY()
	TMap<FString, TObjectPtr<AVehicleElement>> activeVehicles;

	// Vehicle::GetName()→对应的ATransitVehicleElement——这次公共交通车辆不能上下车，不需要
	// 按名字反查，纯粹持有引用防止被GC，和activeVehicles分开存只是为了类型不同。
	UPROPERTY()
	TMap<FString, TObjectPtr<ATransitVehicleElement>> activeTransitVehicles;

	// mesh路径→轨道ISM(每种mesh一个，RF_Transient，不进存档，每次开局/读档重新铺)。
	UPROPERTY()
	TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> trackMeshInstances;
	UInstancedStaticMeshComponent* GetOrCreateTrackISM(const std::string& meshPath);

	UPROPERTY()
	TObjectPtr<UMaterialInterface> pureBaseMaterial;

	// 车辆导航测试的调试折线（单独一个UProceduralMeshComponent，用完清空section）。
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> testNavMesh;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> testNavMaterial;
	// 画导航折线（只由TrackTestNavigationLine的刷新调用）。
	void ShowTestNavigationLine(const TArray<FVector>& path);
	TFunction<bool(TArray<FVector>&)> testNavSource; // 当前跟踪的路线来源，空=没有在显示
	float testNavRefreshCountdown = 0.f;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> busRouteMesh;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> trainRouteMesh;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> planeRouteMesh;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> otherRouteMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> busRouteMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> trainRouteMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> planeRouteMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> otherRouteMaterial;

	// 按stationType选调试mesh/MID("bus"/"train"/"plane"，其它一律落到other那一组)，没有
	// pureBaseMaterial(资产缺失)时返回的MID可能是nullptr，调用方自己判空。
	void GetRouteDebugTarget(const FString& stationType, UProceduralMeshComponent*& outMesh,
		UMaterialInstanceDynamic*& outMaterial);
};
