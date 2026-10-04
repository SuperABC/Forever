# ForeverTrafficFrameworkComponent

> 本文件2026-10-04全文重写——旧版本记录的是更早一轮"T键上下车"的实现(`RequestToggleVehicle`/
> `ToggleVehicle`)，那套API已经被"停车位预置生成+MeetOption上下车"取代，当时忘了同步更新
> 这份文档。现在的职责分两部分：小汽车的停车位生成/上下车，公共交通(公交/火车/飞机)的生成+
> 调试画线。

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
- **`BuildTracks`这次三种内置线路的`trackMesh`都是空字符串**，只在
  `ShouldDrawPath()&&GetTrackMesh().empty()`时打一条Log，不实际铺——真正"沿弧长重复摆放
  `InstancedStaticMeshComponent`"的逻辑(思路同`ForeverRoadnetFrameworkComponent::
  BuildRoadInstances`)留给以后有真实轨道资产的内容类型消费，见
  `Dependence/traffic/route_mod.md`"待办"一节。

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
- `BuildTracks`真正铺轨道mesh的逻辑没有实现，等有真实轨道资产的内容类型再补。
