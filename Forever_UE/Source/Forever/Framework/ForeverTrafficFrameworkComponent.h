#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include <string>
#include <vector>
#include "traffic/vehicle.h" // Vehicle::DrivePoint(跳时间的车辆腿路径)
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverTrafficFrameworkComponent.generated.h"

class APlayerController;
class AVehicleElement;
class ATransitVehicleElement;
class Station;
class Green;
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
// 线路车的Controller驱动与远近切换、线路调试画线与铺轨。详见ForeverTrafficFrameworkComponent.md。
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

	// board_vehicle Change的真正执行：搭乘线路上的车(ATransitVehicleElement)。和ApplyEnterVehicle
	// 同一套(隐藏原pawn、记previousPawn、Possess)，区别是搭乘者只有鼠标视角、车自己跑。bSilent同上。
	void ApplyBoardVehicle(const FString& vehicleName, APlayerController* controller, bool bSilent = false);

	// leave_vehicle / force_leave Change的真正执行：玩家下车(驾驶和搭乘都行)。vehicleName为空=玩家当前
	// 所在的任何载具，非空=只在玩家正在这辆里才执行。bForce=true(force_leave)时驾驶者也不做遮挡判定。
	void ApplyLeaveVehicle(const FString& vehicleName, APlayerController* controller, bool bForce);

	// 线路车的运动学Actor(按Vehicle::GetName()查)，找不到返回nullptr。
	ATransitVehicleElement* FindTransitElement(const FString& vehicleName) const;

	// ---- 线路车的远近切换（见 ForeverTrafficFrameworkComponent.md） ----
	// 载具蓝图指定了线路控制器(AVehicleElement::lineControllerClass)的线路车：远处（地形 linePhysicsLodLevel 级 LOD
	// 范围外）是运动学的 ATransitVehicleElement（Core 时刻表），范围内换成物理 AVehicleElement + 那个控制器按站序循环；
	// 停着的这种车由"测试运行"交给同一种控制器。没指定控制器的线路车一直是运动学的。本组件不认识任何具体的控制器类。

	// 线路控制器的进站/出站（物理段由 Controller 判定，运动学段由 Core 时刻表发，同一时刻只有一个发出方）。
	// stationNodeId 是 Map 辅助导航节点 id（Station 的 stationNode）。
	void FireStopEvent(Vehicle* vehicle, int32 stationNodeId, bool entering);

	// vehicle_command Change 的执行(run/stop)：run = 随机选一条环线的随机站，先导航过去再沿环线循环；
	// stop = 刹停并保持（本来挂在线路上的载具同时退出时刻表，由 Controller 继续持有）。
	// 谁来开 = 这辆车的蓝图指定的线路控制器(没指定就拒绝并打警告)；跑哪些环线 = 使用这种载具的线路(CollectLineLoops)。
	// 环线目前用站点的路网节点表示，站点没接上路网的线路给不出环线。
	void StartVehicleLineRun(const FString& vehicleName);
	void StopVehicleLineRun(const FString& vehicleName);

	// 线路车处于物理段时的物理 Actor，没有返回 nullptr。
	AVehicleElement* FindLinePhysicsElement(const FString& vehicleName) const;
	// 线路车是否当前是物理段 Actor；搭乘需要。不在物理段时强制切换（不看距离），失败返回 nullptr。
	AVehicleElement* EnsureLinePhysicsElement(const FString& vehicleName);
	// 这辆车是不是线路车（有物理类、能做远近切换）。
	bool IsSwitchableRouteVehicle(const FString& vehicleName) const;

	// 线路车在这个地形 LOD 级别内用物理（默认 4：LOD4 范围外 Chaos 地形碰撞太粗糙，物理不可靠）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Line")
	int32 linePhysicsLodLevel = 4;
	// 进入物理的范围系数（<1，滞回：进入用缩小的范围，离开用完整范围，避免边界抖动）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Line")
	float linePhysicsEnterScale = 0.9f;

	// 这辆车当前在场景里的Actor(停着/私家车；线路车没有交互选项，不在这张表里)，找不到返回nullptr。
	AVehicleElement* FindVehicleElement(const FString& vehicleName) const;

	// 按名字找这辆车的Actor，当前没有生成(离玩家太远)就不看距离强制生成一个——剧情的
	// enter_vehicle/读档恢复"正在开的车"指定的车不一定在玩家附近。找不到这辆车返回nullptr。
	AVehicleElement* FindOrSpawnVehicle(const FString& vehicleName);

	// 强制下车的落点：只用车身yaw(翻车的pitch/roll不管)，在车身包围盒外一圈取候选点(先下车点那侧、另一侧、车尾、车头、四角)，
	// 每点向下贴地后要求胶囊体站得下(不忽略车本身)；第一轮还要求从车身中心扫过去不被挡(不隔墙)，第二轮不要求，最后试车顶。
	// preferredSide=下车点在车身哪侧(exitOffsetY的符号)。都不行返回false。
	bool FindForceExitLocation(AVehicleElement* vehicleElement, APawn* previousPawn, float preferredSide, FVector& outLocation);

	// AVehicleElement::ExitVehicle按Q调用：按车身transform+VehicleMod的exitOffset算下车点，ECC_Pawn胶囊体
	// sweep检测遮挡(坑见.cpp)，被挡住就打印拒绝提示、不下车；否则恢复previousPawn，车辆本身不销毁。
	// 搭乘者(ATransitVehicleElement/搭乘模式的物理线路车)也走这条流程但不做遮挡判定；bForce=true(长按Q/force_leave)时
	// 驾驶者也不做，改用FindForceExitLocation在车周围找一个站得下人的空位。
	void ExitVehicle(APlayerController* controller, bool bForce = false);

	// ---- 玩家导航线（script的visualize_navigation Change / T键测试入口） ----
	// 给玩家显示"到target房间怎么走"：玩家步行按人行道画；开车按车道画，车到不了的房间画到target所在建筑1楼外部
	// 连接点附近的车道（线终止在外部，车到了也不消失，下车后刷新成步行线）；乘客/其它Pawn不画。
	// target留着，切换操控（AForeverPlayerController::OnPossess）时RefreshGuide重新规划。
	void StartGuide(Room* target);
	// 导航到绿化区的第circleIndex个活动圆里的(endX,endY)(地图单位，圆内随机点，调用方用Green::RandomPointInCircle取)。
	// 步行：人行道到圆心再走到终点；开车：车到不了绿化区，画到圆心附近的车道(同房间"终止在外部"的处理)。
	// 到达判定按和终点的水平距离(guideArriveDistance)，不依赖房间。
	void StartGuideToGreen(Green* green, int circleIndex, float endX, float endY);
	// 按玩家当前Pawn重新规划并画线；没有活动导航时什么都不做。
	void RefreshGuide();
	// 取消导航并清线。
	void ClearGuide();
	bool HasGuide() const { return guide.target != nullptr || guide.green != nullptr; }

	// 导航线刷新间隔(秒)：每次裁剪玩家已经走过的前缀、判断偏离/到达。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic|Guide")
	float guideRefreshInterval = 0.5f;
	// 玩家偏离折线超过这个距离(地图单位，1=10m，默认2=20m)就重新规划。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic|Guide")
	float guideReplanDistance = 2.f;
	// 绿化区导航：玩家离圆内终点水平距离小于这个(地图单位，1=10m)就算到达，导航结束。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic|Guide")
	float guideArriveDistance = 0.3f;

	// 清掉导航调试折线并停止跟踪。
	void ClearTestNavigationLine();

	// 跟踪显示一条路线（同一时刻只显示最后一次）：每interval秒调一次source取路径重画，source返回false
	// （任务完成/对象没了）就清线并停止。没有计时器，不会自己消失。
	void TrackTestNavigationLine(TFunction<bool(TArray<FVector>&)> source, float interval = 0.1f);

	// 公共交通线路调试画线(每种stationType一个mesh+MID，详见.md)，bShowRouteDebug为false时清空已有section。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traffic|Debug")
	bool bShowRouteDebug = false;

	void BuildRouteDebugMesh(Traffic* traffic);

	// 铺instantiate线路(火车)的轨道ISM，只处理ShouldInstantiate()且有trackMesh的线路，详见.md。
	void BuildTracks(Traffic* traffic);
	// 画桥墩(Route::GetPiers)：ISM，墩底贴地形、墩顶贴桥面。必须在BuildTracks之后调(它负责清实例)。
	void BuildPiers(Map* map, Traffic* traffic);

	// 跳时间(jump_time)结束后调用：线路车的物理表示(实车位置)已经跟时刻表脱节，全部收回运动学、
	// 以时刻表为准(不对齐实车进度)，靠近玩家的之后会被UpdatePhysicsHandoff重新切回物理。
	void ResyncLineVehiclesAfterTimeSkip();

	// ---- 有序通行（路口管理 + 跟车，见Controller/VehicleCommandController.md"有序通行"） ----
	// 被Controller驱动的车辆登记表（跟车判断遍历它）；Controller在Setup时登记，Actor销毁后弱引用自然失效。
	void RegisterDriver(AVehicleCommandController* driver);
	const TArray<TWeakObjectPtr<AVehicleCommandController>>& GetDrivers() const { return drivers; }
	// 路口通行管理器，第一次要用时按Map的路口建（读档/新开局都走这条，不依赖Traffic::Init）；没有Map时返回nullptr。
	JunctionTraffic* GetJunctionTraffic();
	// 预热停车位车辆(carEntries)的蓝图：每种蓝图提前同步加载(骨骼网格/物理资产/材质/贴图)并临时生成销毁一次(建好物理/组件)，
	// 免得玩家第一次走近时SpawnCar里现加载卡几秒(大模型如拖网渔船实测2-3秒)。要在GenerateVehicles之后、加载界面里调。
	void WarmVehicleClasses();
	// 交通时钟（秒）：只在游戏未暂停时累加（暂停=time dilation 0，DeltaTime为0），单帧增量夹到0.25s；
	// 路口请求的0.1s间隔和1s租约都按它计，不随游戏内时间加速。
	double GetTrafficClock() const { return trafficClock; }

	// ---- 脚本驱动的车辆导航（vehicle_navigate change / vehicle_arrive 事件） ----
	// 请求入队，TickComponent里每帧最多处理maxVehicleSpawnsPerFrame个（必要时生成车辆Actor）。
	// destination：房间地址，或"random"=随机一个有车行节点的地下室房间。
	void NavigateVehicle(const FString& vehicleName, const FString& destination);
	// 开到调用方已经预订好的车位(市民开车通勤用)：不再自己挑车位/不取消预订；任何一步失败(找不到车/玩家在开/
	// 车位节点不在车行图/规划失败)都会产生bArrived=false的结果，保证OnVehicleNavigationFinished一定被调用。
	void NavigateVehicleToSlot(const FString& vehicleName, Room* room, int32 slotIndex);

	// 兜底：把车瞬移停进room的第slotIndex个车位(销毁Actor/Controller、结束Core推算、取消导航，再停靠)。
	// 组织任务的车开不回去时用，保证车位不泄漏。
	void TeleportVehicleToSlot(Vehicle* vehicle, Room* room, int32 slotIndex);

	// 开发调试(show_route Change)：切换显示/隐藏某个载具(按载具名)或市民(按姓名)的寻路轨迹，剩余路径折线持续刷新；
	// 没在走/没路径时线隐藏，再点一次选项关掉。载具橙色，市民绿色。
	void ToggleRouteWatch(const FString& name);

	// 跳时间虚拟时钟(见.md"跳时间")：在途/新发起的车辆导航按路径长度换算成游戏秒，不生成Actor；分步推进，走完才触发真正的
	// 到达回调(OnVehicleNavigationFinished + vehicle_arrive)并接着下一段。Begin把在途的导航转成腿，End把没走完的腿
	// 按进度插值后交给Core推算续跑。
	void BeginTimeSkip();
	void EndTimeSkip();
	void DecrementSkipLegs(double stepGameSeconds);
	bool CompleteSkipLegs();

	// 车辆导航结束(到达或失败)后调用，参数：车、目的房间、是否到达。在ProcessNavResults里、车位预定转成停靠之后、
	// 发vehicle_arrive事件之前调用(不依赖storyFramework)。随机目的地的重试不算结束。由AForeverFrameworkActor接给市民域。
	TFunction<void(Vehicle*, Room*, bool)> OnVehicleNavigationFinished;

	// 存档前/后：把正在行驶的车(无论Core推算还是Controller在开)的剩余路径/目的房间/速度写成Vehicle上的行驶快照，
	// 并记下每辆车存档时是否有Actor；先把已入队未处理的导航请求处理完。存完调ClearDriveSnapshots清掉。
	void SyncDrivesToCore();
	void ClearDriveSnapshots();

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

	// 脱困：物理车(被Controller驱动)超过这么多现实秒没前进就强制流式销毁，剩余路径交回Core推算；<=0关闭。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	float stuckTimeoutSeconds = 60.f;
	// 排队等灯/被前车挡着(控制器IsTrafficStopping)时脱困时限放宽到 stuckTimeoutSeconds×这个倍数：
	// 红灯和长队不该被当成卡住交还Core(交回去就不守红绿灯了)，但真堵死的路口还是要能解开。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	float trafficWaitStuckFactor = 5.f;
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	float stuckMoveThreshold = 100.f; // UE单位：离锚点超过这么远才算前进

	// 行进中的车(Core推算中)生成时离玩家至少这个距离(地图单位，默认4=40m)，免得在玩家眼前凭空冒出来。<=0关闭。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	float minSpawnDistanceFromPlayer = 4.f;

	// 场景里同时存在(有Actor)的行进中车辆上限(被Controller驱动的车，含线路车)；满了之后Core推算的车继续推算，不生成Actor。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	int32 maxMovingVehicles = 16;

	// 每帧最多生成几辆车(车是物理模拟的骨骼网格+Chaos组件，生成成本比市民还高)，超过就等下一帧。
	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	int32 maxVehicleSpawnsPerFrame = 1;

private:
	// 被线路控制器开的车的运行状态：线路车（bRouteVehicle）和跑过"测试运行"的停着的车共用。element/controller 是弱引用，
	// Actor 的生命周期归世界。
	struct FLineVehicleState {
		Vehicle* vehicle = nullptr;
		FString name;
		bool bRouteVehicle = false;             // 有 Core 时刻表的线路车
		bool bDetached = false;                 // 线路车已退出时刻表（停止运行/测试运行过），由 Controller 持有
		bool bNoPhysics = false;                // 没有物理类/没有站序，永远运动学
		UClass* physicsClass = nullptr;         // AVehicleElement 蓝图子类，线路车的物理表示
		TWeakObjectPtr<ATransitVehicleElement> kinematic;
		TWeakObjectPtr<AVehicleElement> element;
		TWeakObjectPtr<AVehicleCommandController> controller; // 这辆车的蓝图指定的线路控制器
	};
	TMap<FString, FLineVehicleState> lineVehicleStates;
	// FLineVehicleState::physicsClass 是裸指针，LoadClass 出来的蓝图类没有别的引用，GC 后会悬空（SpawnActor 时崩溃），
	// 所以另存一份 UPROPERTY 让 GC 看得见。
	UPROPERTY()
	TSet<TObjectPtr<UClass>> linePhysicsClasses;
	// WarmVehicleClasses加载出来的停车位车辆蓝图类，另存UPROPERTY防止GC回收(回收了第一次生成又要重新加载)。
	UPROPERTY()
	TSet<TObjectPtr<UClass>> warmedVehicleClasses;
	TMap<int32, Station*> stationByNodeId; // 辅助导航节点 id -> 车站，FireStopEvent 用

	void UpdatePhysicsHandoff();
	// 脱困：线路车卡住时，把Core时刻表进度推到当前这一腿的终点(下一站)并收回物理Actor，之后按时刻表位置重新切物理。
	// 停靠中/脱离时刻表/玩家在开的不处理，返回false。
	bool TeleportLineVehicleToNextStop(const FString& vehicleName);
	bool HandoffToPhysics(FLineVehicleState& state);
	// bSyncProgress=false：不把时刻表对齐到实车位置(跳时间之后实车位置已作废，以时刻表为准)。
	void HandoffToKinematic(FLineVehicleState& state, bool bSyncProgress = true);
	// 按这辆车的蓝图指定的线路控制器类生成控制器并接管它；蓝图没指定返回 nullptr。
	AVehicleCommandController* SpawnLineController(AVehicleElement* element);
	AVehicleCommandController* GetOrCreateLineController(FLineVehicleState& state);
	FLineVehicleState* FindOrAddLineVehicleState(const FString& vehicleName);
	// 停着的车/已脱离时刻表的线路车是否正被线路控制器持续驱动（流式销毁时不能销毁）。
	bool IsDrivenByController(const AVehicleElement* element) const;
	// 正在驱动这辆车(有指令)的控制器(普通车的司机或线路控制器)，没有返回空。
	const AVehicleCommandController* FindActiveDriver(const AVehicleElement* element) const;
	// 收集使用这种载具(vehicleType = VehicleMod id)的线路的环线：每条 line 的站序节点 id。
	void CollectLineLoops(const std::string& vehicleType, TArray<TArray<int32>>& outLoops) const;

	Map* mapRef = nullptr;
	Traffic* trafficRef = nullptr;

	// ---- 脚本驱动的车辆导航 ----
	struct FNavRequest {
		FString vehicleName;
		FString destination;
		Room* presetRoom = nullptr; // 非空=NavigateVehicleToSlot：目的车位已由调用方预订
		int32 presetSlot = -1;
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
	TMap<FString, TWeakObjectPtr<AVehicleCommandController>> carDrivers; // 脚本导航用的通用Controller(线路控制器在lineVehicleStates里)
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

	// 随机挑一个有空闲车位(允许这辆车的种类、从车当前位置沿车行图走得到)的房间，outSlotIndex是挑中的车位下标；没有返回nullptr。
	Room* PickRandomVehicleRoom(Vehicle* core, int32& outSlotIndex);
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
		// 脱困：Controller在开(有命令)的车超过stuckTimeoutSeconds(现实时间)没前进，强制销毁交回Core推算。
		FVector stuckAnchor = FVector::ZeroVector;
		double stuckSinceReal = 0.0;
		double respawnBlockedUntil = 0.0; // 脱困后到这个世界时间前不在原地重新生成
	};
	TArray<FCarEntry> carEntries;
	// 读档后要强制生成回来的车(存档时有Actor的非线路车)：TickComponent每帧限量处理，不看距离。
	TArray<FString> pendingRestoreVehicleSpawns;
	// 没有Actor的车在Core里推算行驶时的移交：进入生成距离后生成Actor交给Controller / 走远后把剩余路径交还Core并销毁Actor。
	void HandOffDriveToActor(FCarEntry& entry);
	bool HandBackDriveToCore(FCarEntry& entry);

	FCarEntry* FindCarEntry(const Vehicle* vehicle);

	struct FSkipDrive {
		Vehicle* vehicle = nullptr;
		Room* destination = nullptr;
		std::vector<Vehicle::DrivePoint> path; // UE单位
		bool bRandom = false;
		double totalSeconds = 0.0;
		double remainingSeconds = 0.0;
	};
	TMap<FString, FSkipDrive> skipDrives;

	struct FRouteWatch {
		bool bCitizen = false;
		UProceduralMeshComponent* mesh = nullptr;
		UMaterialInstanceDynamic* material = nullptr;
	};
	TMap<FString, FRouteWatch> routeWatches;
	float routeWatchCountdown = 0.f;
	void UpdateRouteWatches(float deltaTime);
	bool GetVehicleRoutePath(Vehicle* vehicle, TArray<FVector>& outPath);
	bool bTimeSkipping = false;
	void RegisterSkipDrive(Vehicle* core, Room* destination, const std::vector<Vehicle::DrivePoint>& path, bool bRandom);
	void TeardownVehicleActor(FCarEntry* entry, const FString& name); // 销毁Actor/Controller并摘出activeVehicles
	// 停在车位里的车的世界位姿(UE单位)：房间中心+车位局部偏移，朝向=建筑朝向+车位朝向。车没有停靠房间返回false。
	bool GetParkedPose(Vehicle* vehicle, FVector& outLocation, float& outYaw) const;
	// 玩家下车后：车头朝里停进了玩家所在房间的某个空车位就占上它(见ExitVehicle)；原本预定了这个车位的在途车改去同房间别的空车位，没有就消失。
	void ParkPlayerVehicle(Vehicle* vehicle, const AActor* vehicleActor);
	// other预定的车位被占了：在room里另找一个空车位重新导航；找不到就让它从地图上消失(视为已到达room)。
	void RerouteOrVanishVehicle(Vehicle* other, Room* room);

	// 按登记项生成Actor并记进activeVehicles，失败返回nullptr。
	// bCheckSpace：生成点被别的车占着就不生成（返回nullptr，下次再试），流式生成用；脚本/上车等强制生成不检查。
	AVehicleElement* SpawnCar(FCarEntry& entry, bool bCheckSpace = false);

	// Vehicle::GetName()→当前活着的AVehicleElement(含物理段的线路车)，上车按名字反查用。
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
	float testNavRefreshInterval = 0.1f;

	struct FGuide {
		Room* target = nullptr;
		// 目标是绿化区时用这组(target为空)：圆序号和圆内终点(地图单位)。
		Green* green = nullptr;
		int greenCircle = -1;
		float greenEndX = 0.f, greenEndY = 0.f;
		bool bEndsAtOutside = false; // 线终止在建筑外部连接点/绿化圆附近的车道，不是目标本身
		TArray<FVector> path;        // 剩余路径（UE单位），随玩家前进被裁剪
	};
	FGuide guide;
	void HideGuideLine();
	bool UpdateGuide(TArray<FVector>& outPath); // TrackTestNavigationLine的source

	// 线路调试网格：每种stationType一个mesh+一个MID，按地图上实际出现的类型现建，颜色按类型名哈希取。
	UPROPERTY()
	TMap<FString, TObjectPtr<UProceduralMeshComponent>> routeDebugMeshes;
	UPROPERTY()
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> routeDebugMaterials;
};
