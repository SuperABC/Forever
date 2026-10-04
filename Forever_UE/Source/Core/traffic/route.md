# route.h / route.cpp

## 职责

`Route`：一类线路的实体，独占持有一个`RouteMod`实例(照抄`Vehicle`/`Job`持有mod的模式)。
`Build()`收集`stationType`匹配的`Station`接口、调`mod->LayoutRoute()`拿到拓扑描述，自己
把每条拓扑边变成真实几何(有向图)+组装成若干条环线；`Update()`按时刻表驱动绑定在这条线路
上的载具。

## 关键设计

- **两段式：mod只给拓扑，Route自己把拓扑变成几何**——`mod->LayoutRoute()`只产出
  `(station,interfaceIndex)`下标对组成的`links`/`lines`，不碰任何真实坐标/`Connection`；
  `Route::Build()`拿到这份拓扑后逐条调`BuildEdgeGeometry()`才真正构造几何。这个分工和
  `cross-dll-allocator-crash`那次教训同源：几何构造涉及`new`/`delete`/容器增长，必须发生
  在Core编译的代码里，不能让运行在mod DLL里的`RouteMod::LayoutRoute()`直接碰。
- **`BuildEdgeGeometry`两种边的构造方式**：
  - `useRoadnet==true`(公交)：两端都成功`Station::EnsureRoadLink()`之后，**同车道特例**——
    若两端落在同一段原车道上(`sourceEdge`相同)且起点在上游(`t`更小)，直接用两个
    `stationNode`间的一条直线弦，不绕一圈路径搜索；否则用起点的`outEdge`+
    `Map::FindVehiclePath`(起点车道下游锚点→终点车道上游锚点)+终点的`inEdge`拼起来——这三段
    全部是"借用"Station/Map已有的`Connection`(`owned=false`)，`Route`自己不负责delete。
  - `useRoadnet==false`(火车/飞机)：三次贝塞尔曲线，起止点就是两个接口的世界坐标，两个控制
    点分别沿起点的`departDir`/终点的`arriveDir`伸出`controlLength`——这样相邻两条边在同一个
    站点接口处的切线必然连续(同一个`departDir`/`arriveDir`同时决定了上一条边的终点切线方向
    和下一条边的起点切线方向)，不会出现尖角。控制点`z`分别取两端自己的`z`，支持飞机这类
    有高度变化的线路。四个接口共线、朝向相同时(比如`TrainStation`/`AirStation`同一条跑道的
    两个接口)，两个控制点天然落在同一条直线上，贝塞尔曲线退化成直线，不需要额外判断。
- **有向图+双向边**：`graph[fromKey][toKey]`是单向边；`RouteLink.bidirectional==true`时
  额外登记一条`graph[toKey][fromKey]`，**复用同一份`segments`**(`reversed=true`，
  `owned=false`，只有正向entry负责delete)，`EvaluateEdge`/驾驶逻辑按`reversed`决定是否
  倒序遍历`segments`+取`1-f`+切线取反，不复制几何。这次三个内置线路都是单向环线，没有真正
  用到这条路径，但接口已经打好。
- **`RouteLeg.dwellAtDestination`(2026-10-04新增)**——`Build()`组装`legs`时直接搬运目的地
  那一站的`RouteStop::dwell`过来；`false`表示到达`toStation`后不停留，`DriveVehicle`/
  `Update`的耗时计算都要跳过这一腿的`dwellSeconds`(不只是跳过停靠时的渲染，否则时刻表总
  周期`period`会被白白算长，车辆相位会跟着漂移)。背景见`route_mod.md`"RouteStop.dwell"
  一节——这是为了让火车/飞机只在跑道/轨道中点真正停靠一次，入站/出站接口和地图边缘点全部
  变成纯过路点。
- **`Update()`是纯函数式实现**——只读`elapsedSeconds`(`Traffic::Tick`按
  `Time::DifferenceInSeconds`累加的游戏总秒数，单调递增)，不在`Route`自己身上累积任何状态；
  `DriveVehicle`内部用`localTime`(`elapsedSeconds`按整条线路的`period`取模，每辆车再加一个
  均匀分布的`phase`)逐腿累减"行驶耗时(`edge->length/speed`)+停靠耗时"直到落在某一段区间，
  同一个`elapsedSeconds`重复调用结果完全一致，不依赖调用顺序/调用次数。`speed`/
  `dwellSeconds`的单位是游戏内秒不是真实秒，具体换算见`route_mod.md`。
  - **这是2026-10-04才修好的**：`Time::DifferenceInSeconds`原来的符号写反了(`this`比
    `other`早时返回负数，`Traffic::Tick`的`if (delta > 0.0)`因此每一帧都被跳过)，
    `elapsedSeconds`从头到尾停在初始值不动，表现为"所有公共交通车辆生成之后位置再也不变"。
    排查过程见`Dependence/common/utility.cpp`里`Time::DifferenceInSeconds`声明处的完整
    记录；这个符号bug全局只有`Traffic::Tick`这一处调用方，修好之后不需要再额外处理。

## 依赖关系

- 依赖：`Core/map/map.h`(`FindVehiclePath`)、`Core/traffic/station.h`(`EnsureRoadLink`/
  `GetInterfaces`)、`Core/traffic/vehicle.h`(`SetTransform`)、`traffic/route_mod.h`/
  `route_factory.h`。
- 被谁依赖：`Core/traffic/traffic.cpp`的`InitRoutes`(创建`Route`、调`Build`)/
  `InitTransitVehicles`(按`GetVehiclesPerLine()`创建车辆、`AddVehicleToLine`)/`Tick`
  (调`Update`)；`Forever/Framework/ForeverTrafficFrameworkComponent.cpp`的
  `BuildRouteDebugMesh`(读`GetLines()`画调试线)/`BuildTracks`(读`ShouldDrawPath()`/
  `GetTrackMesh()`)。

## 待办/后续阶段

- `trackMesh`铺真实轨道mesh的逻辑没有任何内置线路用到，见`route_mod.md`"待办"一节。
- `bidirectional`双向边的实际驾驶路径(`reversed`分支)没有被任何内置线路触发过，只验证过
  代码逻辑自洽，没有PIE实测——以后真的有双向线路类型时要重新验证一遍。
