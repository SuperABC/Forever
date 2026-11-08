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
class Room;
class JunctionTraffic;
class AVehicleCommandController;

// 载具：按停车位预置的小汽车(按距离流式生成/销毁)、玩家上下车、公共交通(公交/火车/飞机)、
// 公交Controller驱动与远近切换、线路调试画线与铺轨。详见ForeverTrafficFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverTrafficFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverTrafficFrameworkComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// 由AForeverFrameworkActor在traffic->Init(map)之后调用一次。GetRoute()非空的公共交通车辆立刻生成
	// ATransitVehicleElement(存进activeTransitVehicles)；普通小汽车只登记停放位置(carEntries)，
	// 由TickComponent按玩家距离流式生成/销毁AVehicleElement(全城一次性生成物理车辆负担太重)。
	void GenerateVehicles(Map* map, Traffic* traffic);

	// EnterVehicleChange的真正执行：按名字找(不在场景则强制生成)AVehicleElement，隐藏+despawn-exempt当前pawn、
	// 记进vehicleElement的previousPawn，Possess过去。找不到车辆/当前没有pawn时打Warning。
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

	// AVehicleElement::ExitVehicle按Q调用：按车身transform+VehicleMod的exitOffset算下车点，ECC_Pawn胶囊体
	// sweep检测遮挡(坑见.cpp)，被挡住就打印拒绝提示、不下车；否则恢复previousPawn，车辆本身不销毁。
	// 搭乘者(ATransitVehicleElement/搭乘模式的物理公交)也走这条流程但不做遮挡判定；bForce=true时驾驶者也不做。
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

	// 公共交通线路调试画线(每种stationType一个mesh+MID，详见.md)，bShowRouteDebug为false时清空已有section。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Debug")
	bool bShowRouteDebug = true;

	void BuildRouteDebugMesh(Traffic* traffic);

	// 铺instantiate线路(火车)的轨道ISM，只处理ShouldInstantiate()且有trackMesh的线路，详见.md。
	void BuildTracks(Traffic* traffic);

	// 跳时间(jump_time)结束后调用：线路公交的物理表示(实车位置)已经跟时刻表脱节，全部收回运动学、
	// 以时刻表为准(不对齐实车进度)，靠近玩家的之后会被UpdateBusPhysicsHandoff重新切回物理。
	void ResyncBusesAfterTimeSkip();

	// ---- 有序通行（路口管理 + 跟车，见AVehicleCommandController.md"有序通行"） ----
	// 被Controller驱动的车辆登记表（跟车判断遍历它）；Controller在Setup时登记，Actor销毁后弱引用自然失效。
	void RegisterDriver(AVehicleCommandController* driver);
	const TArray<TWeakObjectPtr<AVehicleCommandController>>& GetDrivers() const { return drivers; }
	// 路口通行管理器，第一次要用时按Map的路口建（读档/新开局都走这条，不依赖Traffic::Init）；没有Map时返回nullptr。
	JunctionTraffic* GetJunctionTraffic();
	// 交通时钟（秒）：只在游戏未暂停时累加（暂停=time dilation 0，DeltaTime为0），单帧增量夹到0.25s；
	// 路口请求的0.1s间隔和1s租约都按它计，不随游戏内时间加速。
	double GetTrafficClock() const { return trafficClock; }

	// ---- 脚本驱动的车辆导航（vehicle_navigate change / vehicle_arrive 事件） ----
	// 请求入队，TickComponent里每帧最多处理maxVehicleSpawnsPerFrame个（必要时生成车辆Actor）。
	// destination：房间地址，或"random"=随机一个有车行节点的地下室房间。
	void NavigateVehicle(const FString& vehicleName, const FString& destination);

	// 车辆流式生成/销毁的距离(地图单位)，是市民的两倍；判断距离时Z方向放大5倍，和市民同一个度量。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	float vehicleSpawnDistance = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	float vehicleDespawnDistance = 8.f;

	// 流式生成行进中的车(Core推算中，停着的车不限制)时，离路口中心(2D)这个距离以内不生成(地图单位，1=10m；默认4=40m)，等车开出这个范围再出现。<=0关闭。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	float minSpawnDistanceFromJunction = 4.f;

	// 车生成后至少存活这么多秒才允许销毁/交还Core(Chaos载具的物理代理还没建好就销毁会断言崩溃)。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	float minVehicleLifetime = 2.f;

	// 行进中的车(Core推算中)生成时离玩家至少这个距离(地图单位，默认4=40m)，免得在玩家眼前凭空冒出来。<=0关闭。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	float minSpawnDistanceFromPlayer = 4.f;

	// 场景里同时存在(有Actor)的行进中车辆上限(被Controller驱动的车，含公交)；满了之后Core推算的车继续推算，不生成Actor。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	int32 maxMovingVehicles = 16;

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
	// bSyncProgress=false：不把时刻表对齐到实车位置(跳时间之后实车位置已作废，以时刻表为准)。
	void BusToKinematic(FBusState& state, bool bSyncProgress = true);
	ABusController* SpawnBusController(AVehicleElement* element);
	ABusController* GetOrCreateBusController(FBusState& state);
	FBusState* FindOrAddBusState(const FString& vehicleName);
	// 停车场公交/已脱离时刻表的公交是否正被 Controller 持续驱动（流式销毁时不能销毁）。
	bool IsDrivenByController(const AVehicleElement* element) const;
	// 收集所有公交环线：(route,line) 及其站序节点 id。
	void CollectBusLoops(TArray<TArray<int32>>& outLoops) const;

	Map* mapRef = nullptr;
	Traffic* trafficRef = nullptr;

	// ---- 脚本驱动的车辆导航 ----
	struct FNavRequest {
		FString vehicleName;
		FString destination;
	};
	struct FNavResult {
		Vehicle* vehicle = nullptr;
		Room* room = nullptr;
		bool bArrived = false;
		bool bRandom = false;
	};
	TMap<FString, int32> randomRetries;
	TMap<FString, bool> randomTargets;
	TArray<FNavRequest> pendingNavigations;
	TArray<FNavResult> pendingNavResults; // Controller回调只入队，下一次Tick再发事件，避免vehicle_arrive->vehicle_navigate递归
	TMap<FString, TWeakObjectPtr<AVehicleCommandController>> carDrivers; // 脚本导航用的通用Controller(公交用busStates)
	TMap<FString, Room*> navTargets; // 车辆名 -> 正在去的房间
	TArray<Room*> vehicleRoomCache;
	TMap<int32, TSet<int32>> reachableCache; // 起点导航节点id -> 沿车行图能到的所有节点id（懒算），随机目的地只在这里面挑
	TArray<FVector2D> junctionCenters; // UE单位，懒建，换地图时清空
	bool bJunctionCentersBuilt = false;
	bool IsNearJunction(const FVector& location);  // 有车行节点的房间，先放地下室的，懒建
	void ProcessPendingNavigations();
	void ProcessNavResults();
	void StartVehicleNavigation(const FNavRequest& request);
	// Core推算(没有Actor)时的车速倍数：和市民的kUnspawnedWalkSpeedScale一样，实际速度=Controller的maxSpeed×该倍数。
	static constexpr float kUnspawnedDriveSpeedScale = 10.f;

	Room* PickRandomVehicleRoom(const Room* exclude);
	AVehicleCommandController* GetOrCreateCarDriver(const FString& vehicleName, AVehicleElement* element);
	AVehicleCommandController* SpawnDriver(UClass* controllerClass, AVehicleElement* element);
	void OnCarNavigateFinished(AVehicleCommandController* controller, int32 nodeId, bool bArrived);

	TSharedPtr<JunctionTraffic> junctionTraffic;
	TArray<TWeakObjectPtr<AVehicleCommandController>> drivers;
	double trafficClock = 0.0;

	// 玩家进出载具(驾驶或搭乘)时给这辆车自己的script发enter_vehicle/leave_vehicle事件。
	void FireVehicleEvent(Vehicle* vehicle, bool entering, const char* mode);

	// 一辆非公共交通车辆的登记项：不管当前有没有生成Actor都存在。location/yaw是最后一次已知的位置
	// (没生成时=停车位或存档/上次销毁时的位置；生成着时每帧同步)。不能标UPROPERTY：Vehicle*是Core层类型。
	struct FCarEntry {
		Vehicle* vehicle = nullptr;
		FString name;
		FString blueprintPath;
		FVector location = FVector::ZeroVector;
		float yaw = 0.f;
		bool bBroken = false; // 加载蓝图失败过，不再重试(避免每帧刷警告)
		double spawnedAt = 0.0; // 最近一次生成的世界时间；刚生成的车至少存活minVehicleLifetime秒才允许销毁(Chaos载具的物理代理还没建好就销毁会断言崩溃)
		TWeakObjectPtr<AVehicleElement> element;
	};
	TArray<FCarEntry> carEntries;
	// 没有Actor的车在Core里推算行驶时的移交：进入生成距离后生成Actor交给Controller / 走远后把剩余路径交还Core并销毁Actor。
	void HandOffDriveToActor(FCarEntry& entry);
	bool HandBackDriveToCore(FCarEntry& entry);

	// 按登记项生成Actor并记进activeVehicles，失败返回nullptr。
	// bCheckSpace：生成点被别的车占着就不生成（返回nullptr，下次再试），流式生成用；脚本/上车等强制生成不检查。
	AVehicleElement* SpawnCar(FCarEntry& entry, bool bCheckSpace = false);

	// Vehicle::GetName()→当前活着的AVehicleElement(含物理段的线路公交)，上车按名字反查用。
	UPROPERTY()
	TMap<FString, TObjectPtr<AVehicleElement>> activeVehicles;

	// Vehicle::GetName()→对应的ATransitVehicleElement(线路上的运动学公共交通车辆)，持有引用防止被GC，
	// FindTransitElement按名字查。
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
