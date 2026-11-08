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
  - 运动学→物理（`BusToPhysics`）：`Route::QueryVehicleState`取线路/腿/是否停靠/剩余停靠，先挂起运动学Actor（`SetSuspended`，撤掉它名下的选项），在Core位姿spawn物理公交并给初速度（`ResumePhysics`），
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

`StartTestNavigation(vehicleName)`（`test_navigate_vehicle` Change，和市民的`test_navigate_citizen`对称）：取`busStates`里这辆车`ABusController::GetRemainingWaypoints`，用`TrackTestNavigationLine`持续显示橙色折线。
`TrackTestNavigationLine(source)`：约每0.1秒重取剩余路径重画（越走越短），`source`返回false（任务完成/对象没了）就清线，没有计时器，不会自己消失。车和市民共用这一条线，同一时刻只显示最后一次。
不生成车、不发命令；车没有在被Controller驱动就什么都不做、也不提示。

### 线路调试画线与铺轨

- **`BuildRouteDebugMesh`**：每种`stationType`一个`UProceduralMeshComponent`+MID（bus绿/train蓝/plane红/其它默认色，`GetRouteDebugTarget`选），
  按`route->GetLines()`里每条腿的edge几何采样（`SampleRouteEdge`，每`ROUTE_DEBUG_SAMPLE_STEP`地图单位一个采样点，至少16段）连成ribbon，
  边中点画箭头指示方向（`reversed`时反向），站点接口画小方块。`bShowRouteDebug`（默认true）为false时`ClearMeshSection`而不是跳过，避免旧可视化残留。
- **`BuildTracks`**：对`ShouldInstantiate()`的线路（火车），遍历`GetLines()`的边（跳过`reversed`、按边指针去重），每个`Connection`段按`BuildRoadInstances`同样的平铺（块数夹在`[0.8,1.2]*trackUnit`，`scaleAlong`拉伸，
  `GetOrCreateTrackISM`每种mesh一个`RF_Transient`的ISM，重铺前先`ClearInstances`）。区别：轨道mesh轴心在起点边（沿+X延伸），所以每块摆在自己的**起点**，朝向取起点→终点的3D方向（含坡度）；
  `edge->track[i].tunnel`的段换用`GetTrackTunnelMesh()`（空则复用`GetTrackMesh()`）。读档时同样重铺（几何从存档来）。
- `GenerateVehicles`每个组件实例只调用一次：新开局/读档都经`LoadMap`重建`AForeverFrameworkActor`及其组件，所以不需要清`carEntries`/`activeVehicles`/`activeTransitVehicles`；同一蓝图路径在公交分支只`LoadClass`一次，transit类与物理表示类共用结果。

## 脚本驱动的车辆导航（vehicle_navigate / vehicle_arrive）

`NavigateVehicle(name, destination)`（`ForeverFrameworkActor::ApplyChange`转发`VehicleNavigateChange`）只入队，`TickComponent`每帧最多处理`maxVehicleSpawnsPerFrame`个：`FindOrSpawnVehicle`（必要时生成车辆Actor，线路公交/玩家正在开的车跳过）→ 选目的地（`"random"`=`PickRandomVehicleRoom`：车行导航图里有节点的房间，地下室优先，排除车当前房间；否则`Map::LocateRoom(地址)`）→ `GetOrCreateCarDriver`（通用`AVehicleCommandController`，经`SpawnDriver`，和公交共用）→ `ClearCommands`+`PushCommand(MakeNavigate(房间导航节点id))`。Controller的`SetNavigateFinishedCallback`回调只把结果入队，下一次Tick的`ProcessNavResults`里：到达则`SetCurrentRoom(目的房间)`并给这辆车自己的script发`VehicleArriveEvent`（规划失败只打警告，不重试）；回调不直接发事件，避免`vehicle_arrive`→`vehicle_navigate`→立刻到达的递归。`IsDrivenByController`也认`carDrivers`，所以导航中的车不会被流式销毁。压力测试脚本见`Resource/Story/vehicle_stress.script`（挂在`VehicleBasic::milestoneNames`上）。**没有Actor的车和市民一样由Core推算**：`StartVehicleNavigation`对没有Actor的车直接同步规划（`PlanVehicleRouteToNode`），路线（UE单位）交给`Vehicle::BeginDrive`（速度=Controller的`maxSpeed`×`kUnspawnedDriveSpeedScale`(10)，和市民一致），`TickComponent`每帧`AdvanceDrive`推进并写回Transform（`entry.location/yaw`同步），走完发抵达；进入生成距离才`SpawnCar`并`HandOffDriveToActor`（`EndDrive`+Controller重新规划导航到目的房间）；走远（超过销毁距离）`HandBackDriveToCore`把Controller的剩余路径交还Core并销毁Actor。开局不会批量生成Actor。Core推算期间不经过路口管理；推算状态不进存档。
`GenerateVehicles`不清请求队列（`game_start`比它先发生），`mapRef`没设时`ProcessPendingNavigations`不处理。注意：停在没有车行节点的房间里的车规划不出路线（只有工厂有车行节点）。

## 有序通行的基础设施

- `RegisterDriver/GetDrivers`：被 `AVehicleCommandController` 驱动的车辆登记表（`Setup` 时登记，弱引用，每帧清理失效项），跟车判断遍历它。
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
  `AVehicleElement::ExitVehicle`调用`ExitVehicle`；`UForeverPopulaceFrameworkComponent::StartTestNavigation`调用`TrackTestNavigationLine`；`ABusController`调用`FireStopEvent`。

## 待办/后续阶段

- 公共交通运动学车辆本身不支持驾驶，见`TransitVehicleElement.md`"待办"一节。
- 暂无隧道专用轨道资产，隧道内复用`trackMesh`。

## 跳时间后的公交
`ResyncBusesAfterTimeSkip()`：线路公交的物理车(实车位置)已跟时刻表脱节，全部`BusToKinematic(state, false)`(不对齐实车进度)，
靠近玩家的之后由`UpdateBusPhysicsHandoff`按时刻表位置重新切物理。玩家在开/搭乘的不动。
