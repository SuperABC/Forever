# traffic_save.h / traffic_save.cpp

## 职责

交通域的存档读写，类名 `TrafficSaveCodec`，写成二进制段 `world.traffic`。内容：
- 站点；
- 线路；
- 车辆；
- 公交计时 `elapsedSeconds`；
- 车辆和站点的命名计数器。

整体方案见 `SAVE_PLAN.md`。

## 内容

- **站点**（按名字排序），分三类：
  - 建筑站点（`Traffic::InitStations`），记所在 Building 的名字；
  - 贴路站点（`InitRoadsideStations`），记所挂道路在 `map->GetRoads()` 里的下标；
  - 边缘站点（`Route::Build` 创建）。

  每个站点都记 mod 注册 id、mod 状态和全部接口（世界坐标和方向）。
- **线路**（按名字排序），每条记：
  - 站点表 `Route::stopStations`，即 `Build` 时交给 mod 的站点顺序，graph 的 key 里的站点下标就是这张表的下标；
  - graph 里的每条连线，包括正向和反向；
  - 各条线路的停靠顺序：每一段的起点站、终点站、接口、是否停靠；
  - 每条线路上的车：车名，顺序决定发车相位。
- **车辆**（按名字排序），每辆记：mod 注册 id、mod 状态、交互选项、停车位（房间 + 局部坐标 + 朝向）、线路绑定、位置。

## 关键设计

- **线路不重新排**。`Route::Build` 会按传入站点的顺序给站点编号，再交给 mod 的 `LayoutRoute` 排线路。站点顺序来自 `unordered_map` 的遍历顺序，读档后会变，所以不能重跑 `Build`。改为直接存它的结果（站点表、连线、停靠顺序）。
- **连线几何不存**。连线的几何（`RouteEdge`）由 `Route::BuildEdgeGeometry` 按两端站点接口重新计算：
  - 走路网的线路借用地图导航图的边，再加站点的旁路（`Station::EnsureRoadLink`，按需创建）；
  - 不走路网的线路是贝塞尔曲线。

  地图按存档原样恢复，所以算出来的结果一致。反向连线和正向共用同一份几何（`owned=false`）。
- **站点用原来的构造函数重建**。mod 的创建和 `SetProperty` 和新开局一样，再用存档里的接口覆盖。读档时不会重新调用 mod 的 Layout 去换算站点接口。
- **公交位置**。公交位置由 `elapsedSeconds` 推算，读档时恢复它，但不恢复 `lastTickTime`（`hasLastTickTime=false`）。读档后游戏时钟可能和存档时不一致，用旧值会算出负的时间差，公交停住不动。
- **交互选项**。车辆的交互选项只在存档带脚本状态段时恢复（这时读档跳过 game_start 广播），否则 game_start 会再加一遍。
- **可以缺省**：`required=false`，没有这一段时交通在恢复出来的地图上重新生成。

## 依赖关系

- 依赖：`traffic/*`、`common/save_refs.h`、`common/save.h`。`Route` 需要在 `Build` 里记下 `stopStations`。
- 被谁依赖：
  - `common/save.cpp`；
  - `Forever/Framework/ForeverFrameworkActor.cpp`（`EnsureTrafficGenerated`）；
  - `Forever/Mod/ForeverSaveSubsystem.cpp`（`Forever.SaveRoundTrip`）。

## 待办 / 后续

- 玩家驾驶中的车速在 P6 处理，也可能不存。
