# ForeverTrafficFrameworkComponent

## 职责

- **小汽车**：开局时按停车位预置生成(`GenerateVehicles`)，玩家走近用MeetOption选"上车"
  (`UForeverStoryFrameworkComponent::ApplyEnterVehicle`→`ApplyEnterVehicle`)，驾驶时按Q
  下车(`AVehicleElement::ExitVehicle`→`ExitVehicle`，带下车点碰撞检测)。
- **公共交通(公交/火车/飞机)**：`GenerateVehicles`额外给`GetRoute()`非空的`Vehicle`生成
  `ATransitVehicleElement`(运动学立方体占位，不能上下车)；`BuildRouteDebugMesh`/
  `BuildTracks`是公共交通专属的调试画线/铺轨道。

## 关键设计

- **`GenerateVehicles`按`vehicle->GetRoute()`是否为空分两条路径**：非空(公共交通车辆)——
  不需要换算停车位世界坐标，`SpawnActor<ATransitVehicleElement>`在原点占位即可，`Tick`里
  直接读`vehicle->GetTransform()`驱动，第一帧就会跳到`Route`算出来的真实位置；为空(普通
  小汽车)——照抄`BuildingElement.cpp::ComputeWorldPosition`的公式把停车位局部坐标换算成
  世界坐标，`LoadClass<AVehicleElement>(vehicle->GetBlueprintPath())`+`SpawnActor`+
  `Init`+建proximity box。普通小汽车的`SpawnActor`额外传了
  `ESpawnActorCollisionHandlingMethod::AlwaysSpawn`——车身有真实碰撞的骨骼网格，停在
  房间里这个点离墙/地板的碰撞体很容易贴得很近，默认的生成时碰撞检测会因为"生成点被占用"
  直接拒绝生成，实测PIE日志里会报"SpawnActor failed because of collision at the spawn
  location"。
- **`ApplyEnterVehicle`/`ExitVehicle`的上下车流程只针对普通小汽车**(`activeVehicles`)，
  公共交通车辆(`activeTransitVehicles`)只是纯粹持有引用防止被GC，不支持`Possess`。上车时
  先给当前pawn(如果是`ACitizenElement`)标`SetDespawnExempt(true)`——隐藏之后这个pawn会
  一直停在原地，但`UForeverPopulaceFrameworkComponent`的距离流式销毁只看"离当前玩家pawn
  多远"，玩家开车远离之后这个停在原地的市民会被判定为"走远的市民"直接销毁，
  `previousPawn`变成悬空指针，下车时车没了但人也出不来；下车时对称地解除这个豁免。
  `ExitVehicle`的下车点碰撞检测用`SweepTestByChannel`+`ECC_Pawn`(不能用
- `ExitVehicle`下车点被挡时同时写屏幕红字和`LogTemp` Warning(含挡住的Actor/组件、起点终点、命中点)，扫掠改用`SweepSingleByChannel`取命中信息；下车后下一帧调`AVehicleElement::EnsureOptionsForPlayer`补弹选项。
  `ECC_Visibility`，这个项目的Pawn预设碰撞档案会忽略它；也不能用`OverlapAnyTestByChannel`
  这类重叠类查询，建筑/房间/园区边界盒是Trigger档案，重叠类查询会把它们也算成"挡住")，
  并且必须忽略载具自己(`queryParams.AddIgnoredActor(vehicleElement)`)——车身用的"Vehicle"
  碰撞档案对`ECC_Pawn`默认是Block，不排除车身自己的话，sweep测到的其实是"车身挡住了自己
  旁边的下车点"。
- **`BuildRouteDebugMesh`每种`stationType`一个`UProceduralMeshComponent`+MID**(bus绿/
  train蓝/plane红/其它默认色，`GetRouteDebugTarget`按字符串选)，按`route->GetLines()`里
  每条腿的edge几何采样(`SampleRouteEdge`，每`ROUTE_DEBUG_SAMPLE_STEP`地图单位一个采样点，
  至少16段)连成ribbon(`AppendRouteEdgeRibbon`)，边中点画一个箭头指示行驶方向
  (`AppendRouteArrow`，`leg.edge->reversed`时指向相反)，站点接口画小方块
  (`AppendRouteStationBox`)。由`bShowRouteDebug`控制，关闭时`ClearMeshSection`而不是
  跳过构建——避免关闭调试开关之后旧的可视化网格残留在场景里。
- **`BuildTracks`铺轨**：对`ShouldInstantiate()`的线路（火车），遍历`GetLines()`的边（跳过`reversed`、按边指针
  去重），每个`Connection`段照抄`BuildRoadInstances`的平铺（块数夹在`[0.8,1.2]*trackUnit`，`scaleAlong`拉伸，
  `GetOrCreateTrackISM`每种mesh一个`RF_Transient`的ISM）。区别：轨道mesh轴心在起点边（沿+X延伸），所以每块
  摆在自己的**起点**，朝向取起点→终点的3D方向（含坡度）；`edge->track[i].tunnel`的段换用
  `GetTrackTunnelMesh()`（空则复用`GetTrackMesh()`）。读档时同样重铺（几何从存档来）。

- **读档后的车辆位置**：`AVehicleElement::Tick`每帧把位置写回`Vehicle::SetTransform`(进存档)。`GenerateVehicles`发现这份记录非零时按记录的位置生成，不放回车位(新开局时记录全为0，照常放车位)。

## 依赖关系

- 依赖：`Source/Forever/Framework/ForeverFrameworkActor.h`(`GetOwner()`拿`Traffic*`/
  `Map*`)、`Source/Core/traffic/traffic.h`/`vehicle.h`/`route.h`、
  `Source/Forever/Element/VehicleElement.h`(`AVehicleElement`)、
  `Source/Forever/Element/TransitVehicleElement.h`(`ATransitVehicleElement`)、
  `Source/Core/map/building.h`/`room.h`(停车位世界坐标换算)。
- 被谁依赖：`AForeverFrameworkActor::BeginPlay()`在`EnsureTrafficGenerated()`→
  `EnsureStoryGenerated()`之后依次调用`GenerateVehicles`/`BuildRouteDebugMesh`/
  `BuildTracks`(必须排在`EnsureStoryGenerated()`之后——车辆自己的Script的`game_start`
  milestone要先广播完，`GenerateVehicles`生成`AVehicleElement`时建proximity box烘焙的
  选项快照才不会是空的)；`UForeverStoryFrameworkComponent::ApplyEnterVehicle`调用
  `ApplyEnterVehicle`；`AVehicleElement::ExitVehicle`调用`ExitVehicle`。

## 待办/后续阶段

- 公共交通车辆不支持上下车/操控，见`TransitVehicleElement.md`"待办"一节。
- 暂无隧道专用轨道资产，隧道内复用`trackMesh`。

## 预置车辆按距离流式生成/销毁（2026-10-22）

非公共交通的车辆（`AVehicleElement`，带物理模拟的骨骼网格+Chaos车辆组件+触发盒）不再开局全城一次性生成：`GenerateVehicles` 只登记 `carEntries`（名字、蓝图路径、最后已知位置/朝向），`TickComponent` 按玩家距离生成/销毁，和市民同一个思路，但阈值是市民的两倍：`vehicleSpawnDistance`=4.0、`vehicleDespawnDistance`=8.0（地图单位），判断距离时 Z 差放大 5 倍（和市民同一个度量），每帧最多生成 `maxVehicleSpawnsPerFrame`=1 辆。

- 销毁前位置已由 `AVehicleElement::Tick` 每帧写进 Core 的 `Vehicle::SetTransform`，下次生成用登记项里最新的位置，存档不受影响。
- 正被玩家操控的车（`GetController()!=nullptr`）不销毁。
- `ApplyEnterVehicle`（剧情 enter_vehicle / 读档恢复正在开的车）改用 `FindOrSpawnVehicle`：找不到活着的 Actor 就按名字强制生成，不看距离。`FindVehicleElement` 仍只返回当前活着的。
- 公共交通车辆（`ATransitVehicleElement`）不受影响，仍然全部常驻。
- 起因：全城约 200 辆预置车一直常驻，加上近处建筑电梯轿厢每帧重复 `SetWorldLocation`，游戏线程碰撞重叠更新占了约 16ms，见 `BuildingElement::UpdateCabins`（轿厢没动就不再同步组件）。

## 公交搭乘/下车/事件
- `ApplyBoardVehicle(name, controller, bSilent)`：搭乘线路公交(隐藏原pawn、记previousPawn、Possess)。`ApplyLeaveVehicle(name, controller, bForce)`：leave_vehicle/force_leave执行入口，name非空时只在玩家正在这辆里才执行。`ExitVehicle(controller, bForce)`驾驶/搭乘共用：搭乘者不做遮挡判定，驾驶者除非force才判定。`FindTransitElement`按名字查线路公交Actor。
- `FireVehicleEvent`：进出载具时给**这辆车自己的**script发`enter_vehicle`/`leave_vehicle`事件(mode=drive/ride)；`ApplyEnterVehicle`/`ApplyBoardVehicle`的`bSilent`用于读档恢复(不发)。
- `GenerateVehicles`线路车分支：`blueprintPath`加载出来不是`ATransitVehicleElement`子类(公交带的是停车场驾驶用的BP_BusElement)就忽略，仍用`ATransitVehicleElement`。

## 车辆导航测试

`StartTestNavigation(vehicleName)`：目的地=`building_airport`的0层第一个有导航节点的房间；调
`Map::PlanVehicleRoute`，折线画成`testNavMesh`(橙色细带，到达/取消清除；连不到目的地时留15秒)，再交给
`AVehicleElement::StartTestNavigation`。接不上路时左上角(key 9100)+`UE_LOG`同时提示。

## 公交 Controller 驱动（2026-11-01）
- **线路公交远近切换**（`UpdateBusPhysicsHandoff`，每帧最多切一辆）：地形 `busPhysicsLodLevel`（默认 4）级 LOD 范围外 = 运动学 `ATransitVehicleElement` + Core 时刻表；范围内 = 物理 `AVehicleElement`（蓝图路径 `GetBlueprintPath()`）+ `ABusController` 按站序循环。范围判定 `UForeverTerrainFrameworkComponent::IsInsideLodLevel`，进入用 `busPhysicsEnterScale`(0.9) 缩小的范围、离开用完整范围（滞回）。
  - 运动学→物理（`BusToPhysics`）：`Route::QueryVehicleState` 取线路/腿/是否停靠/剩余停靠，在 Core 位姿 spawn 物理公交、给初速度、`Vehicle::SetPhysicsDriven(true)`（`Route::Update` 跳过它）、下发 `LoopNodes`（停靠中带 `bStartAtStop/startDwell/bStopAlreadyEntered`）。
  - 物理→运动学（`BusToKinematic`）：`ClearCommands`（停站补 `leave_stop`）、销毁物理公交+Controller、`SetPhysicsDriven(false)`、`Route::ResetStopState`（下一次 Update 当第一次观察，正好停着就补发 `enter_stop`）、恢复运动学 Actor。玩家驾驶/搭乘中不收回。
  - `enter_stop/leave_stop` 任何时刻只有一个发出方：运动学段 = Core 时刻表（`AForeverFrameworkActor` 转发），物理段 = `ABusController`（`FireStopEvent`，用 `stationByNodeId` 把辅助节点 id 反查回车站）。
- **测试运行 / 停止运行**（`StartBusTestRun/StopBusRun`，由 `vehicle_command` Change 触发）：测试运行 = 随机公交环线的随机站，先 `NavigateToNode`，到站后接 `LoopNodes`；停止运行 = `StopNow`。线路公交被点过测试运行/停止运行后 `bDetached`（退出时刻表，`physicsDriven` 锁定，之后由 Controller 持有，LOD 外靠 Controller 自带的运动学模式）。
- 搭乘：`ApplyBoardVehicle` 对线路公交用物理公交 `SetRideMode(true)` 后 Possess；`ExitVehicle` 用 `IsRideMode()` 判搭乘（不做遮挡判定）；下车后 Controller 下一帧自动收回。读档 `controlledVehicle` 是线路公交走 `ApplyBoardVehicle`（物理公交不在就强制切换）。
- 流式销毁：停车场车辆"玩家占有或 Controller 有指令在跑"时不销毁（原来的 `GetController()==nullptr` 判断会被 AI Controller 误伤）。
- 不保存：测试运行状态不进存档，读档后公交回到停车场/默认状态（脚本变量 `running` 会随脚本存档，读档后可能与实际不一致）。
- 未在 PIE 验证。
