# ForeverTrafficFrameworkComponent

## 职责

- **小汽车**：开局只登记停车位（`carEntries`），`TickComponent`按玩家距离流式生成/销毁`AVehicleElement`；
  玩家走近用MeetOption选"上车"（`UForeverStoryFrameworkComponent::ApplyEnterVehicle`→`ApplyEnterVehicle`），
  驾驶时按Q下车（`AVehicleElement::ExitVehicle`→`ExitVehicle`，带下车点碰撞检测）。
- **公共交通（公交/火车/飞机）**：`GenerateVehicles`给`GetRoute()`非空的`Vehicle`生成`ATransitVehicleElement`
  （运动学立方体占位，由Core时刻表驱动，全部常驻）；线路公交在玩家附近换成物理`AVehicleElement`+`ABusController`；
  `BuildRouteDebugMesh`/`BuildTracks`是线路调试画线/铺轨道。
- 搭乘、事件、导航测试、测试运行等入口见下。

## 关键设计

### 生成与流式


- `GenerateVehicles`按`vehicle->GetRoute()`是否为空分两条路径：
  - 非空（公共交通）：`SpawnActor`在原点占位（`blueprintPath`加载出来不是`ATransitVehicleElement`子类——公交带的是停车场驾驶用的BP——就忽略，仍用`ATransitVehicleElement`），
    `Init`后存进`activeTransitVehicles`；之后`Tick`直接读`vehicle->GetTransform()`驱动，第一帧就跳到`Route`算出的位置。
    `stationType=="bus"`且蓝图能加载成`AVehicleElement`子类的，另登记`FBusState`（线路公交，见下）。
  - 为空（小汽车）：用`TrafficComputeWorldPosition`（同`BuildingElement.cpp`的room-local→世界公式）把停车位局部坐标换成世界坐标，
    读档后`Vehicle::GetTransform`记录非零（`AVehicleElement::Tick`每帧写回）就按记录位置，否则放车位（新开局记录全为0）。只登记`FCarEntry`，不生成。
- **按距离流式**：`vehicleSpawnDistance`=4.0 / `vehicleDespawnDistance`=8.0（地图单位，是市民的两倍），Z差放大5倍（`TRAFFIC_VERTICAL_DISTANCE_WEIGHT`，和市民同一个度量），
  每帧最多生成`maxVehicleSpawnsPerFrame`=1辆（车是物理骨骼网格+Chaos组件，成本比市民还高）。不全城常驻是因为约200辆预置车加上其它开销曾让游戏线程碰撞重叠更新占了约16ms。
  - 销毁前位置已由`AVehicleElement::Tick`每帧写进`Vehicle::SetTransform`，下次生成用登记项里最新的位置，存档不受影响。
  - 正被玩家操控（`IsPlayerControlled()`）或被`ABusController`持续驱动（`IsDrivenByController`，有指令在跑）的车不销毁。
  - `SpawnCar`用`ESpawnActorCollisionHandlingMethod::AlwaysSpawn`：车身有真实碰撞，停车位离墙/地板很近，默认碰撞检测会因"生成点被占用"拒绝生成；
    生成高度加`TRAFFIC_HEIGHT_EPSILON`(50)，让刚体靠重力落地——刚体生成时哪怕嵌进地板1个单位，Chaos也会用很大的冲量把车弹飞。加载蓝图失败的登记项标`bBroken`，不再重试。
  - `FindOrSpawnVehicle`：按名字找活着的Actor，没有就不看距离强制生成（剧情`enter_vehicle`/读档恢复"正在开的车"指定的车不一定在附近）；`FindVehicleElement`只返回活着的。
- 公共交通车辆（`ATransitVehicleElement`）不受流式影响，常驻。

### 上车/搭乘/下车

- `ApplyEnterVehicle(name, controller, bSilent)`（驾驶）：隐藏原pawn并关碰撞，若是`ACitizenElement`标`SetDespawnExempt(true)`，`SetPreviousPawn`，`Possess`；`Vehicle::SetCurrentRoom(nullptr)`（不再算停在房间里）。
  线路公交若有`ABusController`先`ForcePhysicsMode`（运动学段车没有物理，先切回物理才能开）。
  **必须标despawn-exempt**：隐藏的pawn留在原地，玩家开车远离后会被Populace的距离流式销毁，`previousPawn`悬空，下车时人出不来；下车时对称解除。
- `ApplyBoardVehicle`（搭乘）：线路公交用物理公交`SetRideMode(true)`后Possess（`EnsureBusPhysicsElement`不看距离强制切换物理）；其余用`ATransitVehicleElement`（`CanBoard()`）。搭乘者只有鼠标视角，车自己跑。
- `ApplyLeaveVehicle(name, controller, bForce)`（leave_vehicle / force_leave）：`name`非空时只在玩家正在这辆里才执行（公交收班不能把坐在别的车里的玩家踢下来）。
- `ExitVehicle(controller, bForce)`：驾驶/搭乘共用，搭乘者不做遮挡判定，驾驶者除非`bForce`才判定。下车点 = `VehicleMod`的`exitOffsetX/Y/Z`经车身完整transform变换。
  遮挡检测用`SweepSingleByChannel`+`ECC_Pawn`（胶囊42x96）从车身中心（Z对齐下车点，整体抬高`kExitSweepLift`避开路面ISM）扫到下车点，并忽略载具自己：
  - 不能用`ECC_Visibility`：这个项目的Pawn碰撞预设忽略它；
  - 不能用Overlap类查询：建筑/房间/园区/市民/车辆边界盒都是Trigger档案，会被误判成挡路；
  - 必须忽略载具自己：车身"Vehicle"档案对`ECC_Pawn`是Block，否则永远下不了车。
  被挡住时同时写屏幕红字和`LogTemp` Warning（含挡住的Actor/组件、起点终点、命中点）。
  **恢复顺序不能变**：先恢复显示/碰撞+重新`Possess`，最后才移动位置——否则移动时碰撞还是disabled不触发`UpdateOverlaps`，车辆触发盒感知不到人回来；
  且`OnOverlapBegin`用`GetPlayerPawn(0)`判断是不是玩家，`Possess`晚于移动会判不通过。只取车身Yaw让人站直。下车后下一帧调`AVehicleElement::EnsureOptionsForPlayer`补弹选项（触发盒的进入事件不一定能带出选项）。
  车辆本身不销毁，原地留着。
- `FireVehicleEvent`：进出载具时给**这辆车自己的**script发`enter_vehicle`/`leave_vehicle`（mode=drive/ride）；`bSilent`用于读档恢复（script变量/选项已随存档恢复，不重复发）。
- 已知小瑕疵：上车后要走远一步才能再上车（`SetActorEnableCollision(false)`不保证同步清掉陈旧重叠记录）。试过`UpdateOverlaps()`强制清（没用）、先挪到远处再隐藏（会触发离开房间/建筑边界盒的误报提示），都不要再试。

### 公交 Controller 驱动与远近切换

线路公交（`FBusState::bRouteBus`）和跑过"测试运行"的停车场公交共用`busStates`（按车名）。
- **远近切换**（`UpdateBusPhysicsHandoff`，每帧最多切一辆）：地形`busPhysicsLodLevel`（默认4，LOD4范围外Chaos地形碰撞太粗糙，物理不可靠）级LOD范围外 = 运动学`ATransitVehicleElement` + Core时刻表；
  范围内 = 物理`AVehicleElement`（蓝图`GetBlueprintPath()`）+`ABusController`按站序循环。范围判定`UForeverTerrainFrameworkComponent::IsInsideLodLevel`，进入用`busPhysicsEnterScale`(0.9)缩小的范围、离开用完整范围（滞回）。
  - 运动学→物理（`BusToPhysics`）：`Route::QueryVehicleState`(传入当前实际的游戏秒/真实秒倍率)取线路/腿/是否停靠/剩余停靠，先挂起运动学Actor（`SetSuspended`，撤掉它名下的选项），在Core位姿spawn物理公交并给初速度（`ResumePhysics`），
    `Vehicle::SetPhysicsDriven(true)`（`Route::Update`跳过它），下发`MakeLoop`指令（停靠中带`bStartAtStop/startDwellSeconds/bStopAlreadyEntered`）。
  - 物理→运动学（`BusToKinematic`）：先用`ABusController::QueryLoopProgress`+`Route::SyncVehicleProgress`把时刻表进度对齐到实车位置（物理车比名义速度慢，不对齐会"瞬移"），再`ClearCommands`（停站补`leave_stop`）、销毁物理公交+Controller、`SetPhysicsDriven(false)`、`Route::ResetStopState`（下一次`Update`当第一次观察，正好停着就补发`enter_stop`）、恢复运动学Actor。玩家驾驶/搭乘中不收回。
  - `enter_stop/leave_stop`任何时刻只有一个发出方：运动学段 = Core时刻表（`AForeverFrameworkActor`转发），物理段 = `ABusController`（`FireStopEvent`，用`stationByNodeId`把辅助导航节点id反查回车站）。
  - `busPhysicsClasses`（UPROPERTY的`TSet`）保存`LoadClass`出来的蓝图类：`FBusState::physicsClass`是裸指针，GC后会悬空，`SpawnActor`时崩溃。
- **测试运行 / 停止运行**（`StartBusTestRun/StopBusRun`，`vehicle_command` Change触发）：`run` = 随机公交环线（`CollectBusLoops`）的随机站，先`NavigateToNode`，到站后接`LoopNodes`；`stop` = `StopNow`。
  线路公交被点过后`bDetached`（退出时刻表，`physicsDriven`锁定，之后由Controller持有，LOD外靠Controller自带的运动学模式）。停车场公交第一次被驱动时`FindOrAddBusState`登记状态（没有时刻表、没有运动学表示）。
- 下车后Controller下一帧自动收回；读档`controlledVehicle`是线路公交走`ApplyBoardVehicle`（物理公交不在就强制切换）。
- 不保存：测试运行状态不进存档，读档后公交回到停车场/默认状态（脚本变量`running`会随脚本存档，读档后可能与实际不一致）。
- 未在PIE验证。

### 导航测试折线

## 玩家导航线（visualize_navigation / T键）

`StartGuide(Room* target)`保存目标并`RefreshGuide()`：按玩家当前Pawn分三种——`ACharacter`（步行，含占有的市民）→`Map::PlanPedestrianRoute`终点是目标房间；非乘客模式的`AVehicleElement`→先`Map::PlanVehicleRoute`到房间，规划不出来（车到不了）就在`target->GetParentBuilding()->GetOutsideNodes()`（建筑init时记下、进存档的1楼行人外部连接点）里取离车最近且`Map::PlanVehicleRouteToPoint`可达的一个，线终止在那条车道上的垂足（`bEndsAtOutside`）；乘客模式/其它Pawn→只清线，目标保留。线复用`TrackTestNavigationLine`（间隔`guideRefreshInterval`=0.5s），`UpdateGuide`每次把玩家投影到折线上，裁掉已走前缀，偏离折线超过`guideReplanDistance`(2=20m)重新规划；`!bEndsAtOutside`且`Player::GetCurrentRoom()==target`时销毁；终止在外部的线永远不因靠近终点销毁（下车→`AForeverPlayerController::OnPossess`触发`RefreshGuide`→变成步行线）。`TrackTestNavigationLine`每次调用前拷贝`testNavSource`，所以source里可以安全地重新规划。T键入口见`ForeverPlayerController.md`。

**绿化区目标**：`StartGuideToGreen(Green*, circleIndex, endX, endY)`把`FGuide.green/greenCircle/greenEndX/Y`填上(`target`为空)，`HasGuide`两者任一非空即真。步行→`Map::PlanPedestrianRouteToGreen`(人行道走到圆心节点，末尾追加圆内终点)；开车→`PlanVehicleRouteToPoint`到圆心，线终止在车道上(`bEndsAtOutside`，车进不了绿化区，下车后刷新成步行线)。到达判定不看房间：`!bEndsAtOutside`且玩家和终点水平距离<`guideArriveDistance`(0.3=3m)就销毁导航。`visualize_navigation`对地址先试`LocateRoom`，找不到再试`LocateGreen`(圆序号省略=随机一个圆，终点圆内随机)。

### 线路调试画线与铺轨

- **`BuildRouteDebugMesh`**：每种`stationType`一个`UProceduralMeshComponent`+MID（bus绿/train蓝/plane红/其它默认色，`GetRouteDebugTarget`选），
  遍历`route->GetLines()`的`runs`→`pieces`，按每个`TrackPath`几何采样（`SampleRouteEdge(const TrackPath&)`，每`ROUTE_DEBUG_SAMPLE_STEP`地图单位一个采样点，至少16段，`TSet<const TrackPath*>`去重）连成ribbon，
  路径中点画箭头指示方向，站点的inlet/outlet/stop画小方块。`bShowRouteDebug`（默认false）为false时`ClearMeshSection`而不是跳过，避免旧可视化残留。`stationByNodeId`遍历`RouteLine::runs`。
- **`BuildPiers(Map*, Traffic*)`**：画 `Route::GetPiers()`(Core 选好的桥墩)。mesh 空串=`/Game/Asset/Common/Mesh/Cylinder`，用 `GetOrCreateTrackISM` 同一套 ISM 缓存，所以必须在 `BuildTracks`(开头统一清实例)之后调。墩底=`Map::SampleHeight(x,y)`(水面不管，取到的就是水底)，墩顶=`topZ`；缩放读 mesh `GetBounds()`(不假设资产原尺寸和轴心)：水平直径=2×radius，竖直撑满墩高，包围盒中心对齐墩中心。
- **`BuildTracks`**：对`ShouldInstantiate()`的线路（火车），遍历`route->GetTrackPaths()`（站内连线+站间连线，已去重），**每段用自己的`mesh`/`meshUnit`**（空mesh的段不铺），每个`Connection`段按`BuildRoadInstances`同样的平铺（块数夹在`[0.75,1.5]*unit`，`scaleAlong`拉伸，
  `GetOrCreateTrackISM`每种mesh一个`RF_Transient`的ISM，重铺前先`ClearInstances`）。区别：轨道mesh轴心在中心（沿+X延伸），所以每块摆在自己的**中点**，朝向取起点→终点的3D方向（含坡度）。隧道没有专用标志，由mod直接把段的`mesh`换成隧道资产。读档时同样重铺（几何从存档来）。
- `GenerateVehicles`每个组件实例只调用一次：新开局/读档都经`LoadMap`重建`AForeverFrameworkActor`及其组件，所以不需要清`carEntries`/`activeVehicles`/`activeTransitVehicles`；同一蓝图路径在公交分支只`LoadClass`一次，transit类与物理表示类共用结果。

## 脚本驱动的车辆导航（vehicle_navigate / vehicle_arrive）

`NavigateVehicle(name, destination)`（`ForeverFrameworkActor::ApplyChange`转发`VehicleNavigateChange`）只入队，`TickComponent`每帧最多处理`maxVehicleSpawnsPerFrame`个：`FindOrSpawnVehicle`（必要时生成车辆Actor，线路公交/玩家正在开的车跳过）→ 选目的地（`"random"`=`PickRandomVehicleRoom`：车行导航图里有节点的房间，地下室优先，排除车当前房间；否则`Map::LocateRoom(地址)`）→ `GetOrCreateCarDriver`（通用`AVehicleCommandController`，经`SpawnDriver`，和公交共用）→ `ClearCommands`+`PushCommand(MakeNavigate(房间导航节点id))`。Controller的`SetNavigateFinishedCallback`回调只把结果入队，下一次Tick的`ProcessNavResults`里：到达则`SetCurrentRoom(目的房间)`并给这辆车自己的script发`VehicleArriveEvent`（规划失败只打警告，不重试）；回调不直接发事件，避免`vehicle_arrive`→`vehicle_navigate`→立刻到达的递归。`IsDrivenByController`也认`carDrivers`，所以导航中的车不会被流式销毁。**没有Actor的车和市民一样由Core推算**：`StartVehicleNavigation`对没有Actor的车直接同步规划（`PlanVehicleRouteToNode`），路线（UE单位）交给`Vehicle::BeginDrive`（速度=Controller的`maxSpeed`×`kUnspawnedDriveSpeedScale`(10)，和市民一致），`TickComponent`每帧`AdvanceDrive`推进并写回Transform（`entry.location/yaw`同步），走完发抵达；进入生成距离才`SpawnCar`并`HandOffDriveToActor`（`EndDrive`+Controller重新规划导航到目的房间）；走远（超过销毁距离）`HandBackDriveToCore`把Controller的剩余路径交还Core并销毁Actor。开局不会批量生成Actor。Core推算期间不经过路口管理；推算状态不进存档。
`GenerateVehicles`不清请求队列（`game_start`比它先发生），`mapRef`没设时`ProcessPendingNavigations`不处理。注意：停在没有车行节点的房间里的车规划不出路线（只有工厂有车行节点）。

## 有序通行的基础设施

- `RegisterDriver/GetDrivers`：被 `AVehicleCommandController` 驱动的车辆登记表（`Setup` 时登记，弱引用，每帧清理失效项），跟车判断遍历它。
- `WarmVehicleClasses()`(2026-11-22)：加载界面"预热车辆"阶段(`AForeverFrameworkActor`)调用，对`carEntries`里每种蓝图路径`LoadClass`(存进`warmedVehicleClasses`防GC)并在高空临时生成销毁一次，免得玩家第一次走近时`SpawnCar`同步加载+建物理卡2-3秒(拖网渔船实测)。
- `GetJunctionTraffic()`：第一次用到时按 `mapRef` 的路口建 `JunctionTraffic`（`BuildFromMap`），`GenerateVehicles` 换地图时重置；不依赖 `Traffic::Init`（读档不会重跑它）。
- `GetTrafficClock()`：交通时钟，`TickComponent` 开头（所有早返回之前）累加 `min(DeltaTime, 0.25)`；暂停时 time dilation=0 所以不走。路口的 0.1s 请求间隔和 1s 租约都按它。
- 市民（`ACitizenController::GateCrosswalk`）也通过它拿路口管理器和时钟。

## 依赖关系

- 依赖：`ForeverFrameworkActor`（`GetOwner()`拿`GetStoryFramework()`/`GetTerrainFramework()`）、`ForeverTerrainFrameworkComponent`（`IsInsideLodLevel`）、`ForeverStoryFrameworkComponent`（`BroadcastToVehicle`）、
  `Source/Core`的`traffic/traffic.h`/`vehicle.h`/`route.h`/`station.h`、`map/map.h`/`building.h`/`room.h`（停车位世界坐标）、
  `Element/VehicleElement.h`、`Element/TransitVehicleElement.h`、`Controller/BusController.h`、`Element/CitizenElement.h`（despawn豁免）。
- 被依赖：`AForeverFrameworkActor::BeginPlay()`在`EnsureTrafficGenerated()`→`EnsureStoryGenerated()`之后依次调用`GenerateVehicles`/`BuildRouteDebugMesh`/`BuildTracks`
  （必须排在`EnsureStoryGenerated()`之后——车辆script的`game_start`要先广播完，生成`AVehicleElement`时建触发盒烘焙的选项快照才不会是空的）；
  `UForeverStoryFrameworkComponent`调用`ApplyEnterVehicle`/`ApplyBoardVehicle`/`ApplyLeaveVehicle`；`AForeverFrameworkActor`调用`StartBusTestRun`/`StopBusRun`/`StartTestNavigation`（以及读档恢复时的`ApplyBoardVehicle`）；
  `AVehicleElement::ExitVehicle`调用`ExitVehicle`；`AForeverFrameworkActor`的`visualize_navigation`和`AForeverPlayerController`（`TestNavigate`/`OnPossess`）调用`StartGuide`/`RefreshGuide`；`ABusController`调用`FireStopEvent`。

## 待办/后续阶段

- 公共交通运动学车辆本身不支持驾驶，见`TransitVehicleElement.md`"待办"一节。
- 暂无隧道专用轨道资产，隧道段目前用普通轨道mesh（改`TrainRoute::BuildTrack`里隧道段的`mesh`即可）。

## 跳时间后的公交
`ResyncBusesAfterTimeSkip()`：线路公交的物理车(实车位置)已跟时刻表脱节，全部`BusToKinematic(state, false)`(不对齐实车进度)，
靠近玩家的之后由`UpdateBusPhysicsHandoff`按时刻表位置重新切物理。玩家在开/搭乘的不动。

## 车位占用、进出库、玩家停车

- **寻路预定**：`StartVehicleNavigation`先`ClearReservation`，再在目的房间`FindFreeSlot(车的种类)`(`random`目的地由`PickRandomVehicleRoom`
  挑有空闲且沿车行图走得到的车位的房间)，`ReserveSlot`成功才继续，**没有空闲车位=拒绝**(记一次失败结果，随机目的地会换一个重试)。目标节点=车位的导航节点。
  到达(`ProcessNavResults`)时`ArriveAtReservedSlot`转成停靠；失败时`ClearReservation`。
- **出库**：车停在车位里时，Controller的`PlanAndFollow`/Core推算分支都走`Map::PlanVehicleRouteFromSlot`(倒车到与车位相连的connection另一端再远1个`VehicleMod::sizeX`)，规划成功才`ReleaseSlot`。
  Core推算(无Actor)的路径点`DrivePoint.reverse`标倒车段；`HandOffDriveToActor`遇到还在倒车就用`FVehicleCommand::MakeFollowPath`把剩余路径原样交给Controller
  (重新规划会丢倒车段)，`HandBackDriveToCore`把`GetRemainingWaypoints`返回的`bReverseFirst`写回`path[0].reverse`。
- **消失的车**(`Vehicle::ParkState::Vanished`)：`TickComponent`不为它生成Actor；下次`StartVehicleNavigation`直接瞬移：预定转停靠、`SetTransform`/登记项位姿=车位位姿、记一次到达。
- **玩家上车**(`ApplyEnterVehicle`)：`ReleaseSlot`+`ClearReservation`，清掉它的脚本导航指令。
- **玩家下车**(`ExitVehicle`→`ParkPlayerVehicle`)：取`Player::GetCurrentRoom()`，车位姿换建筑局部后`Room::FindSlotContaining`，且车位允许这辆车的种类，则`BindSlot`占上。
  该车位原本被在途车预定的，`RerouteOrVanishVehicle`：同房间另找空车位重新导航(有Actor→Controller重规划；Core推算→重新`BeginDrive`)，找不到就从地图消失(`SetVanished`，销毁Actor，视为已到达，补发到达事件；
  未到`minVehicleLifetime`的先隐藏、到点再销毁，避免Chaos物理代理未建好就销毁崩溃)。

## 预设车位导航与存档行驶状态
`NavigateVehicleToSlot(name, room, slot)`：目标车位已预订，直接规划；导航成功/失败都经`OnVehicleNavigationFinished(vehicle, room, bArrived)`回调(由ForeverFrameworkActor接到市民域)。`SyncDrivesToCore/ClearDriveSnapshots`在存档前后写/清行驶快照；`GenerateVehicles`尾部恢复快照并强制生成`wasSpawned`的车。

## 组织任务与跳时间虚拟时钟（2026-11-16）

- `TeleportVehicleToSlot(车,房间,车位)`：兜底瞬移停车(销毁Actor/Controller、结束推算、停靠)，组织任务的车开不回去时用。
- 跳时间：`BeginTimeSkip`把脚本/通勤/任务导航中的车(有navTargets、无路线)交回Core后按剩余路径转成`FSkipDrive`腿；
  跳期间`StartVehicleNavigation`改走Core规划+`RegisterSkipDrive`(先拆Actor)，不再`BeginDrive`；`CompleteSkipLegs`停靠到预订车位、
  `ProcessNavResults`触发`OnVehicleNavigationFinished`+`vehicle_arrive`，并当场处理由此发起的下一段导航(带多出来的时间)；
  `EndTimeSkip`把没走完的腿按进度插值后`BeginDrive`续跑。腿上的车不参与流式生成。

## 路线调试 show_route（2026-11-17）

`ToggleRouteWatch(name)`建一条`FRouteWatch`(ProceduralMesh+独立颜色MID)，`UpdateRouteWatches`在Tick里每0.2秒刷新；载具路径`GetVehicleRoutePath`(跳时间腿/Actor的VehicleCommandController/Core推算)，
市民路径`UForeverPopulaceFrameworkComponent::GetRoutePath`。和玩家导航线(`testNavMesh`)是两套，互不覆盖。

## 导航请求按毫秒预算(2026-11-16)

`ProcessPendingNavigations`不再按`maxVehicleSpawnsPerFrame`个数限制，改为每帧至少处理1个、之后用满8ms预算(最多64个)；超过30ms会打Warning。`StartVehicleNavigation`里单次路线规划超过20ms也会打Warning，用来在PIE里确认瓶颈。

## 脱困(2026-11-16)

被Controller驱动的物理车(`IsDrivenByController`，非玩家)位置超过`stuckTimeoutSeconds`(默认60，**现实时间**，暂停不计)没离开锚点`stuckMoveThreshold`(100UE单位)，就`HandBackDriveToCore`强制销毁Actor、剩余路径交回Core推算，并在`respawnBlockedUntil`(30游戏秒)内不在原地重新生成。公交等交不回去的失败后再等一个周期。日志`脱困：车辆 ...`。

脱困补充：线路公交(`bRouteBus`、没脱离时刻表)交不回Core导航，卡住后走`TeleportBusToNextStop`：读控制器`QueryLoopProgress`，`Route::SyncVehicleProgress(leg, 1.0)`把时刻表进度推到这一腿终点(下一站)，再`BusToKinematic(state, false)`收回物理Actor，之后`UpdateBusPhysicsHandoff`按时刻表位置重新切物理。停靠中/玩家在开的不处理。日志`脱困：公交 ...`。

- **Tick组 = `TG_PostPhysics`**：组件默认`TG_DuringPhysics`，和Chaos(含载具异步模拟线程)并行；本组件的`TickComponent`会销毁载具Actor，悬挂约束随之释放，物理线程可能正在`ApplySuspensionForces`里读它，打包版崩溃(访问`0xffffffffffffffff`)即此；放到物理之后tick避开。`traffic`测试套件(`Source/Test`)带玩家反复传到行驶中的车旁边来复现这类生成/销毁竞争。
