# station_mod.h / station_mod.cpp

## 职责

`StationMod`：一类站点（公交站/火车站/机场...）的声明：**停靠位在哪、进站口/出站口在哪、站内怎么走**，供 Core（`Core/traffic/station.h` 的 `Station`）读取并换算成世界坐标。线路怎么跨站连接是 `route_mod.h`（`RouteMod`）的职责，两者靠 `stationType`（通用类别词，不是 mod 名）耦合。

## 站点模型

一个站点由三类端口和站内连线组成（占地矩形局部坐标，单向）：

- `stops`：**停靠位**（1~n）。车辆停下上下客的位置，由 mod 指定，不再默认是进出口的中点——现在的火车站/机场实现仍把它放在进出口中点，以后做航站楼、停机坪这类"连线之外"的停靠位时不用改 Core。
- `inlets`：**进站口**（1~n）。车辆从站外进入本站的接口。
- `outlets`：**出站口**（1~n）。车辆从本站离开的接口。进出口分开是因为所有连线都是单向的。
- `connections`：`StationConnectionSpec{fromKind,fromIndex,toKind,toIndex,curves}`，只允许三种：进站口→停靠位、停靠位→出站口、进站口→出站口（直通，**不停站穿过本站**时走它）。`curves` 是首尾相接的 `StationCurveSpec`（四个点都是占地矩形局部坐标+z，带 `mesh`/`meshUnit`），一条连线可以由多段贝塞尔组成，每段自带路线资产。

`StationPortSpec.position`/`StationCurveSpec` 的点都用建筑占地矩形（`Building` 自身的 `Quad`，不是楼体 footprint）的局部坐标，原点左下角、未旋转，ratio+offset 约定（`PointParams`，同 `NavigationNodeTemplate`）。`headingDegrees` 相对建筑旋转（度），`z` 相对地面。

## 关键设计

- **两种挂载方式二选一**：
  1. 挂在建筑上：`BuildingMod::stationMod` 单向声明"这栋楼配哪个站点"，`Traffic::InitStations()` 据此创建 `Station`、调一次 `Layout(direction, sizeX, sizeY)` 填好端口和连线。火车站/机场用这种方式。
  2. 直接贴着道路摆：实现 static `AssignRoads()` 声明想贴哪些路、哪一侧，`Traffic::InitRoadsideStations()` 据此创建 `Station`，不经过 `Layout()`；这种站点只有一个停靠位（下标 0），没有进出站口和站内连线，由路网车道代替。公交站用这种方式。
- **端口坐标换算**：`Building::LocalToWorld` 接的是"楼体局部坐标"，和占地矩形局部坐标之间的换算（抵消 `bodyOffset`）在 `Core/traffic/station.cpp` 里做，见 `station.md`——世界坐标和楼体 footprint 的偏移无关，楼体贴路、把深处让给跑道/站台不用改这里的数值。
- **`SetProperty()` 两段式**：`stationType` 必须在 `SetProperty()` 里填（`AssignRoads` 是 static，没有实例）；Core 创建实例后立刻调一次，再调 `Layout()`。`direction` 是 `Building::GetDirection()` 缓存值，`sizeX/sizeY` 是占地矩形尺寸。
- **`AssignRoads()` 是 static 方法**：和 `BuildingMod::Assign`/`ZoneMod` 那组"按类型固定、不需要实例"的函数指针约定同一个模式，基类不声明，通过 `StationFactory::RegisterStation` 的函数指针参数注册。`RoadStationRequest.rightSide` 是"沿 Road Start->End 方向的右手边"（依赖 `Map::ComputeLaneAnchorPosition` 的 `perp0` 公式；+Y=南的坐标系下和直觉相反）。`RoadStationRequest.name` 非空时 Core 直接用它当站名(mod 保证全局唯一，如公交站的"路名+序号")，空则按 `"Station"+计数器`。

- **可选的手动摆放函数（2026-11-23）**：想接编辑器手动站的 station mod 另外提供一个 static `AssignManualRoads(roads, const TransitData&, emit, context)`，通过 `StationFactory::RegisterManualAssign(id, fn)` 登记（没登记=不支持手动站，`AssignRoads` 和 `StationMod` 基类都不变）。理由和 `AssignRoads` 相同：它决定"哪里有站"，调用时还没有任何 `Station`/`StationMod` 实例。数据只读，mod 只处理 `stop.station==自己` 的站。`Traffic::InitRoadsideStations` 的防呆：数据里指向没登记手动函数的 station id 的站告警并忽略；站名重复丢弃后到的并告警（原来的 `insert_or_assign` 会静默覆盖并泄漏旧 `Station`）。
- 端口的朝向就是该端口的行驶方向；同一条轨道/跑道的进站口、停靠位、出站口朝向相同，连线退化成直线。

## 依赖关系

- 依赖：`Dependence/map/general_geometry.h`（`PointParams`/`Road`）。
- 被谁依赖：`Core/traffic/station.h`、`traffic.cpp`、`route_mod.h`（`TrackSegment`）、`Basic/traffic/station_*.h`。

## 待办/后续阶段

- 目前只有"挂建筑"和"贴路"两种挂载方式。
