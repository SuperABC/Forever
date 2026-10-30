# route_mod.h / route_mod.cpp

## 职责

`RouteMod`：一类线路（公交/火车/飞机各一个具体子类）的连接逻辑——把Core收集到的、
`stationType`匹配的所有站点接口组织成一张有向图+若干条环线。Core（`Core/traffic/route.h`
的`Route`）只负责把这张图变成真实几何、按时刻表驱动车辆，完全不关心"具体怎么连"，这个职责
全部交给`LayoutRoute()`的具体实现决定。

## 关键设计

- **`RouteStationInfo`是只读、世界坐标的站点接口快照**——`Route::Build()`收集完所有
  `stationType`匹配的真实接口后，连同"本次调用之前其它`Route`已经请求创建的边缘站点"
  （`isEdge=true`）一起喂给`LayoutRoute()`；mod只能读，不持有所有权，也不能直接碰Core的
  任何对象——`RouteMod`本身是运行在mod DLL里的代码，不能像Core对象的非虚成员函数那样直接
  调用（跨DLL分配器安全约束，见`Dependence/map/general_geometry.md`"跨DLL分配器"一节）。`RouteMod`
  的输出渠道全部是"往`edgeStations`/`links`/`lines`这三个自己持有的`vector`里追加数据"，
  和`BuildingMod::floors`同一个安全模式。
- **`RouteStop.dwell`**：`false`表示这一站是纯过路点，到达后不停留、直接接着跑下一腿；
  默认`true`保持原有行为。公交每一站都要停是对的，但火车/飞机的入站/出站接口、地图边缘点
  不该停——真实世界里乘客在跑道/站台**中段**上下客，不是在跑道两端分别停一次。具体用法见
  `Basic/traffic/route_basic.md`的`LayoutDualTrackStationLoop`一节。
- **`RouteLink.bidirectional`**：`true`表示同一份几何两个方向都能走（比如单线铁路），
  Core建图时会额外登记一条反向边，倒序遍历同一份`segments`、切线取反，不复制几何（见
  `route.md`"有向图与双向边"一节）。内置线路（公交/火车/飞机）全部是单向环线，没有实际
  用到这个字段，留给以后"同一段轨道两个方向都要走"的线路类型用。
- **`speed`/`dwellSeconds`的单位是"地图单位/游戏内秒"、"游戏内秒"，不是真实秒**——
  `Traffic::Tick`喂给`Route::Update`的`elapsedSeconds`来自`Player::Tick()`里
  `time->AddMilliseconds(delta*60*1000*timeFlowRatio)`，默认`timeFlowRatio=2.0`，即1个
  真实秒=120个游戏内秒。三个具体子类（`Basic/traffic/route_basic.cpp`）都在构造函数里按
  这个120倍换算校准过默认值，新增线路类型时同样要留意这个换算，不能直接抄字面上看起来
  "合理"的数值。
- `useRoadnet`/`instantiate`/`trackMesh`/`trackUnit`/`controlLength`几个选项字段的具体用法
  见`route.md`"非路网边：三次贝塞尔"/"路网边：同车道特例"两节。
- **`easeSeconds`的单位是真实秒，不是游戏内秒**——和`speed`/`dwellSeconds`刚好相反，因为
  这个字段只是`Route::DriveVehicle()`内部用来算梯形速度曲线的一个"缓动窗口"参数，不直接
  参与`Update()`按`length/speed`算的时刻表/`period`（那套仍然是游戏内秒），用真实秒更直观
  （策划/调参时想的是"进站前N秒开始减速"而不是"进站前N个游戏内秒"）。默认3秒（公交尺度），
  火车/飞机在各自`SetProperty()`里调大，见`route.md`"DriveVehicle的缓动"一节。

## 依赖关系

- 依赖：`general_geometry.h`（`Connection`/`TerrainHatch`/`TerrainFitRequest`及`BuildHatch`/`BuildTerrainFit`）。
- 被谁依赖：`Core/traffic/route.h`的`Route::Build()`读取这些结构、调`LayoutRoute()`；
  `Basic/traffic/route_basic.h`的`BusRoute`/`TrainRoute`/`AirRoute`三个具体实现扩展这个
  基类。

## 实例化路线（铺轨、过山爬坡/隧道）

- `instantiate`（替换了原来没用上的`drawPath`）：true表示这条线路要在世界里实例化成真实轨道——火车true，
  公交（走现有路网）/飞机false。`trackMesh`是轨道资产，`trackTunnelMesh`是隧道内用的资产（留空=复用
  `trackMesh`，目前还没有隧道专用资产），`trackUnit`是一块轨道沿弧长的长度，`trackWidth`是占地整宽
  （挖隧道洞/地形贴合用）。
- `BuildTrack(linkEnds, getTerrain, getWater, getHeight)`（虚，默认不处理）：`Route::Build`在`LayoutRoute`之后、
  构建边几何之前对instantiate线路调用一次。`linkEnds[i]`是`links[i]`两端的真实接口位置（Core按
  `links`顺序填）。做法和`RoadnetMod::DistributeRoadnet`一致：mod自己沿线采样地形，判断过山是抬升还是
  挖隧道，直接产出**分段几何**`trackEdges[i]`（`TrackSegment`：起点/两个控制点/终点/`tunnel`标记，纯值类型，
  不放`Node`/`Connection`，避免跨DLL的id/所有权问题），并用`AddTunnelHatch`/`AddTerrainFit`提交请求
  （`hatches`/`terrainFits`）。`trackEdges[i]`留空=沿用Core的单段贝塞尔。
- `AddHatch`/`AddTunnelHatch`/`AddTerrainFit`三个工具方法和`RoadnetMod`同名方法实现相同，都转调
  `general_geometry.h`里的`BuildHatch`/`BuildTerrainFit`；`TerrainFitSample`/`TerrainFitRequest`也在那里
  （原来在`roadnet_mod.h`），`RoadnetMod`的接口不变。

## 待办/后续阶段

- 暂时没有隧道专用的轨道资产，隧道内复用`trackMesh`；有了资产填`TrainRoute::SetProperty`里的
  `trackTunnelMesh`即可。
