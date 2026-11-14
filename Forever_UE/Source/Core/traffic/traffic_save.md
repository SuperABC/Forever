# traffic_save.h / traffic_save.cpp

## 职责

交通域的存档读写，类名 `TrafficSaveCodec`，写成二进制段 `world.traffic`。内容：
- 站点；
- 线路；
- 车辆；
- 公交计时 `elapsedSeconds`；
- 车辆和站点的命名计数器。

存档系统总说明见 `Source/Core/common/save.md`。

## 内容

- **站点**（按名字排序），分两类：
  - 建筑站点（`Traffic::InitStations`），记所在 Building 的名字；
  - 贴路站点（`InitRoadsideStations`），记所挂道路在 `map->GetRoads()` 里的下标。

  每个站点都记 mod 注册 id、mod 状态，以及世界坐标的 `inlets`/`outlets`/`stops` 和站内 `connections`（含每段 `TrackSegment` 的几何和 `mesh`/`meshUnit`）。
- **线路**（按名字排序），每条记：
  - mod 注册 id 和 mod 状态；
  - 站点表（`stopStations` 的站名，`RouteEndpoint::station` 下标指向它）；
  - `externs`（名字、位置、朝向）；
  - `links`（两个端点、`withinStation`、**最终几何**的 `TrackSegment` 列表）；
  - `lines`（每条线路的 `{name,stopIndex}` 条目）；
  - 每条线路上的车：车名，顺序决定发车相位。
- **车辆**（按名字排序），每辆记：mod 注册 id、mod 状态、交互选项、停车位（房间 + 局部坐标 + 朝向）、线路绑定、位置。

## 关键设计

- **桥墩**：`Route::piers`(选位结果)在每条 route 的 lines 之后、车辆名单之前写 `count × (x,y,topZ,radius,mesh)`；读档不重跑 `PlacePiers`。
- **线路不重新排，也不存寻路图/运动学**。`Route::Build` 按传入站点顺序给站点编号再交给 mod 的 `LayoutRoute`，站点顺序来自 `unordered_map` 的遍历顺序，读档后会变，所以不能重跑 `Build`。改为直接存 mod 的输出（站点表、externs、links、lines），读档把它们填回 `route->mod`，然后调 `Route::Assemble()` 重建寻路图和每条线路的运动学。这样不需要存逐边/逐腿数据，地图按存档原样恢复，规划结果一致。
- **instantiate 线路的 links 几何要存最终值**：地形已经按它抬升/挖洞，读档不能重新采样。公交的 links 为空。
- **站点用原来的构造函数重建**：mod 的创建和 `SetProperty` 和新开局一样，再用存档里的端口和连线覆盖，读档时不会重新调用 mod 的 `Layout` 换算。
- **公交位置**由 `elapsedSeconds` 推算，读档时恢复它，但不恢复 `lastTickTime`（`hasLastTickTime=false`）：读档后游戏时钟可能和存档时不一致，用旧值会算出负的时间差。
- **交互选项**只在存档带脚本状态段时恢复（这时读档跳过 game_start 广播），否则 game_start 会再加一遍。
- **不做版本迁移**：游戏尚未发布，旧存档（旧的节点/leg 格式）会读不了。

## 依赖关系

- 依赖：`traffic/*`、`common/save_refs.h`、`common/save.h`。
- 被谁依赖：`common/save.cpp`、`Forever/Framework/ForeverFrameworkActor.cpp`（`EnsureTrafficGenerated`）、`Forever/Mod/ForeverSaveSubsystem.cpp`（`Forever.SaveRoundTrip`）。

## 停站事件

读档创建线路后调用 `route->SilenceInitialStopEvents()`：读档后第一次 `Route::Update` 只记录各车停靠状态、不发 `enter_stop`（车辆 script 的变量/选项已随存档恢复，再发会重复加选项）。`on_route` 在恢复线路绑定时同步置 1。

## 待办 / 后续

- 玩家驾驶中的车速在 P6 处理，也可能不存。
