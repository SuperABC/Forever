# route.h / route.cpp

## 职责

`Route`：一类线路的实体，独占持有一个`RouteMod`实例（照抄`Vehicle`/`Job`持有mod的模式）。
`Build()`收集`stationType`匹配的`Station`接口、调`mod->LayoutRoute()`拿到拓扑描述，自己把
每条拓扑边变成真实几何（有向图）+组装成若干条环线；`Update()`按时刻表驱动绑定在这条线路上的
载具。

## 关键设计

- **两段式：mod只给拓扑，Route自己把拓扑变成几何**——`mod->LayoutRoute()`只产出
  `(station,interfaceIndex)`下标对组成的`links`/`lines`，不碰任何真实坐标/`Connection`；
  `Route::Build()`拿到这份拓扑后逐条调`BuildEdgeGeometry()`才真正构造几何。几何构造涉及
  `new`/`delete`/容器增长，必须发生在Core编译的代码里，不能让运行在mod DLL里的
  `RouteMod::LayoutRoute()`直接碰（跨DLL分配器安全约束，见`[[memory:cross_dll_allocator_crash]]`）。
- **`BuildEdgeGeometry`两种边的构造方式**：
  - `useRoadnet==true`（公交）：两端都成功`Station::EnsureRoadLink()`之后，**同车道特例**——
    若两端落在同一段原车道上（`sourceEdge`相同）且起点在上游（`t`更小），直接用两个
    `stationNode`间的一条直线弦，不绕一圈路径搜索；否则用起点的`outEdge`+
    `Map::FindVehiclePath`（起点车道下游锚点→终点车道上游锚点）+终点的`inEdge`拼起来——这三段
    全部是"借用"Station/Map已有的`Connection`（`owned=false`），`Route`自己不负责delete。
  - `useRoadnet==false`（火车/飞机）：三次贝塞尔曲线，起止点就是两个接口的世界坐标，两个控制
    点分别沿起点的`departDir`/终点的`arriveDir`伸出`controlLength`——这样相邻两条边在同一个
    站点接口处的切线必然连续（同一个`departDir`/`arriveDir`同时决定了上一条边的终点切线方向
    和下一条边的起点切线方向），不会出现尖角。控制点`z`分别取两端自己的`z`，支持飞机这类有
    高度变化的线路。四个接口共线、朝向相同时（比如`TrainStation`/`AirStation`同一条跑道的
    两个接口），两个控制点天然落在同一条直线上，贝塞尔曲线退化成直线，不需要额外判断。
- **有向图+双向边**：`graph[fromKey][toKey]`是单向边；`RouteLink.bidirectional==true`时
  额外登记一条`graph[toKey][fromKey]`，**复用同一份`segments`**（`reversed=true`，
  `owned=false`，只有正向entry负责delete），`EvaluateEdge`/驾驶逻辑按`reversed`决定是否
  倒序遍历`segments`+取`1-f`+切线取反，不复制几何。内置线路目前全部是单向环线，没有实际用到
  这条路径，但接口已经打好。
- **`RouteLeg.dwellAtDestination`**：`Build()`组装`legs`时直接搬运目的地那一站的
  `RouteStop::dwell`过来；`false`表示到达`toStation`后不停留，`DriveVehicle`/`Update`的
  耗时计算都要跳过这一腿的`dwellSeconds`（不只是跳过停靠时的渲染，否则时刻表总周期`period`
  会被白白算长，车辆相位会跟着漂移）。目的是让火车/飞机只在跑道/轨道中点真正停靠一次，入站/
  出站接口和地图边缘点全部是纯过路点，见`route_mod.md`"RouteStop.dwell"一节。
- **`Update()`是纯函数式实现**——只读`elapsedSeconds`（`Traffic::Tick`按
  `Time::DifferenceInSeconds`累加的游戏总秒数，单调递增），不在`Route`自己身上累积任何状态；
  `DriveVehicle`内部用`localTime`（`elapsedSeconds`按整条线路的`period`取模，每辆车再加一个
  均匀分布的`phase`）逐腿累减"行驶耗时（`edge->length/speed`）+停靠耗时"直到落在某一段区间，
  同一个`elapsedSeconds`重复调用结果完全一致，不依赖调用顺序/调用次数。`speed`/
  `dwellSeconds`的单位是游戏内秒不是真实秒，具体换算见`route_mod.md`。
- **`DriveVehicle`的缓动按"行驶区段"（两次真正停靠之间的若干条leg合起来）整体套梯形速度
  曲线，不是按单条leg自己的`[0,1]`区间**——载具运行时完全不感知接口本身的存在，只认"是不是
  一次真正的停靠"（`RouteLeg::dwellAtDestination`）；按单条leg各自做缓动会在纯过路接口
  （见`station_basic.cpp` `Layout()`的"顺序必须是`[leftIn, rightOut, rightIn, leftOut]`"
  一节）处产生肉眼可见的减速/卡顿，这是不要按单leg做缓动的原因。做法：`DriveVehicle`开头往回
  扫`dwellAtDestination`找到当前leg所在区段的起点`segStart`，累计这整段的总时长
  `segTotalTime`/总长度`segTotalLength`，套梯形速度曲线（匀加速`ta`→匀速→匀减速`td`，
  `ta`/`td`钳在`mod->easeSeconds`和区段总时长一半之间取小）算出区段内的累计弧长
  `segArcLength`；定位`segArcLength`落在区段内哪条leg上时，**按累计长度（不是累计时间）
  重新从`segStart`走一遍**，和`EvaluateEdge`内部按弧长在多段`segments`里定位同一个道理，
  确保缓动曲线和"用哪条leg的几何算世界坐标"用的是同一套长度基准，接口处不会对不上。匀速段
  速度`cruiseSpeed`比配置的`mod->speed`略高一点（补偿两头比匀速慢的那部分距离），确保
  `segTotalTime`（=`Σlength/speed`，和不缓动时完全一样）内还是正好走完`segTotalLength`，
  `Update()`按这个值算的时刻表/`period`不受影响。公交这类"每一站都要停"（`dwell`恒为
  `true`）的线路，区段退化成单条leg，效果等价于那条leg自己两端各缓动`ta`/`td`。
  - **`easeSeconds`是真实秒单位，`segTotalTime`/`segElapsed`是游戏内秒单位**（和
    `speed`/`dwellSeconds`同一套单位）——`DriveVehicle`里换算成
    `kEaseSeconds = mod->easeSeconds * 120.f`（1真实秒=120游戏内秒，对应默认
    `timeFlowRatio=2.0`），如果以后`timeFlowRatio`被剧情脚本改了要跟着重新校准（和
    `speed`/`dwellSeconds`的既有校准方式一样）。`easeSeconds`是`RouteMod`的字段（默认3秒，
    公交尺度），不是硬编码常量，`TrainRoute`/`AirRoute`在`SetProperty()`里按手感调大，见
    `route_basic.cpp`。

- **`stopStations`**：`Build()` 末尾记下交给 mod 的站点表(graph key里的站点下标就是它的下标)，存档要靠它把key换成站点名，见`traffic_save.md`。读档时`TrafficSaveCodec`直接按存档填graph/lines，不再调用`Build()`。

## 依赖关系

- 依赖：`Core/map/map.h`（`FindVehiclePath`）、`Core/traffic/station.h`（`EnsureRoadLink`/
  `GetInterfaces`）、`Core/traffic/vehicle.h`（`SetTransform`）、`traffic/route_mod.h`/
  `route_factory.h`。
- 被谁依赖：`Core/traffic/traffic.cpp`的`InitRoutes`（创建`Route`、调`Build`）/
  `InitTransitVehicles`（按`GetVehiclesPerLine()`创建车辆、`AddVehicleToLine`）/`Tick`
  （调`Update`）；`Forever/Framework/ForeverTrafficFrameworkComponent.cpp`的
  `BuildRouteDebugMesh`（读`GetLines()`画调试线）/`BuildTracks`（读`ShouldDrawPath()`/
  `GetTrackMesh()`）。

## 待办/后续阶段

- `trackMesh`铺真实轨道mesh的逻辑没有任何内置线路用到，见`route_mod.md`"待办"一节。
- `bidirectional`双向边的实际驾驶路径（`reversed`分支）没有被任何内置线路触发过，只验证过
  代码逻辑自洽，没有PIE实测——以后真的有双向线路类型时要重新验证一遍。

## 停站/离站事件
`Route::Update`每次算完一辆车的相位后，和`lastStopStation`(上次停靠的站，nullptr=在行驶)比较：不在→在站=entering，在站→不在=leaving，站A→站B(两次Update之间跨过整个停站)=先A的leaving再B的entering。事件进`pendingStopEvents`，`ConsumeStopEvents()`取走(`Traffic::ConsumeStopEvents`汇总所有线路)。`DriveVehicle`通过`outDwellStation`带出当前停靠的站。读档后`SilenceInitialStopEvents()`让第一次Update只记状态不发事件(`traffic_save.cpp`加载线路时调用)；新开局第一次Update正常发。Core不碰script，由`AForeverFrameworkActor::Tick`转发为`enter_stop`/`leave_stop`事件。
