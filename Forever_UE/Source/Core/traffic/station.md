# station.h / station.cpp

## 职责

`Station`：一个上下车点的实体。两种创建场景对应两个构造函数——挂在建筑上的真实站点（火车站/机场）、贴着道路摆的真实站点（公交站）。一个站点永远只有一个 `Station`；线路上不属于任何站点的点（地图边界、天空点）是 `Route` 图里的 extern 节点，见 `route.md`。`EnsureRoadLink` 额外负责"公交接入现有车辆路网"这条旁路的惰性构造。

## 数据

世界坐标的 `inlets`/`outlets`/`stops`（`StationPort{x,y,z,dirX,dirY}`）和 `connections`（`StationConnection{fromKind,fromIndex,toKind,toIndex,segments}`，`segments` 是世界坐标的 `TrackSegment` 值）。来源是 `StationMod` 的端口/连线声明（`station_mod.md`），Core 只换算坐标，不改几何。贴路站点只有一个 stop，其余为空。

## 关键设计

- **必须在所有建筑落地之后创建**：`Traffic::InitStations()` 遍历所有 building 时，building 的 `Layout()`/导航图合并早已跑完，这里只读 `Building` 缓存好的数据（`GetDirection()`/`GetSizeX/Y()`/`LocalToWorld()`/`GetFloorBaseZ(0)`），不触发二次布局。
- **坐标换算要先抵消 `bodyOffset` 才能喂给 `LocalToWorld`**：`position` 是占地矩形局部坐标（原点左下角），`LocalToWorld` 只认楼体局部坐标。换算（`station.cpp` 构造函数里的 `toWorld`）：
  ```
  quadLocalX = ratioX * quadSizeX + offsetX                          // 占地矩形局部
  bodyLocalX = quadLocalX - quadSizeX*0.5 + bodySizeX*0.5 - bodyOffsetX  // 楼体局部
  ```
  代入 `LocalToWorld` 自己的公式会发现 `bodySizeX`/`bodyOffsetX` 两项抵消，世界坐标只取决于占地矩形局部坐标，和楼体 footprint 偏移无关——楼体贴路（`NearRoadFootprint`）能成立的前提。端口和每条连线的每个点（含控制点）都走同一个 `toWorld`，z = 一层地面高度 + 声明的 z，朝向 = 建筑旋转 + `headingDegrees`。
- **贴路站点（公交站）的世界坐标必须精确落在车道中线上**：`Traffic::InitRoadsideStations()` 直接用 `Map::GetLaneSegmentAt` 的真实车道坐标，和 `EnsureRoadLink` 查的是同一套逻辑，不会出现到站坐标和行驶坐标不是同一个点的瞬移。`attachedRoad` 记下贴的是哪条路。
- **`StationRoadLink` 是两点直连的弦，不是对原车道的真正切割**：`stationNode` 新建、只属于这个 `Station`，`inEdge`/`outEdge` 是 `laneFrom->stationNode->laneTo` 两段直线 `Connection`，不登记进 `Map` 的 `throughLines`/`vehicleNavGraph`，但通过 `Map::RegisterAuxNavNode` 登记成辅助导航节点。`t` 是站点在 `sourceEdge` 上的弧长比例，供 `Route` 判断同车道特例。
- `EnsureRoadLink(map, stopIndex)` 惰性构造并按停靠位下标缓存；查不到可用的 Road/车道（如单行道只有一侧）返回 `nullptr`，`Route::BuildRoadRun` 据此判定这一段不可达。

## 依赖关系

- 依赖：`Core/map/building.h`、`Core/map/map.h`、`traffic/station_mod.h`/`station_factory.h`。
- 被谁依赖：`Core/traffic/route.cpp`（读端口、调 `EnsureRoadLink`）、`traffic.cpp`（创建站点）、`traffic_save.cpp`、UE 侧调试绘制。
