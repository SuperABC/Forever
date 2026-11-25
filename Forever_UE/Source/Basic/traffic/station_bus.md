# station_bus.h / station_bus.cpp

## 职责

`BusStation`(`station_bus`)：station域的默认实现之一。

## 关键设计

- **同一条轨道的两个接口朝向必须相同**(沿frontage轴同一方向)，`Route::Build()`算出的贝塞尔控制点才会贴着两点连线、是一条直的"轨道"。
- **机场：楼体占满整个Building区域**(默认footprint，同火车站)，跑道接口`depthRatio`(`0.7`/`0.9`)落在楼体范围内。
- **火车站：楼体占满整个Building区域**(默认footprint，不贴路一侧)，所以轨道接口(`depthRatio` `0.45`/`0.55`)落在楼体范围内：楼体是1楼`preset_station_train_fg+`(大厅+4间`room_empty`，层高0.75)+2楼`preset_station_train_fl+-`(层高0.75)+3楼`preset_station_train_fu-`(层高1.0)，没有地下层，三层各4台电梯，端口 z=1.5(恰是3楼地面，0.75+0.75)。接口本身的世界坐标换算不受楼体footprint偏移影响(见`Core/traffic/station.md`"接口坐标换算"一节)。
- **`BusStation::AssignRoads`(棋盘格大路公交站)**：只在4车道大路(`GetVehicleLanes(0/1).size()>=2`)上、长度`CalcDistance()`在[24,96]的`Road`(跨1~2格，跨3格的会误加，不管)靠中点放一对站，两侧各一个(`rightSide`=side0)。井字路网没有4车道大路，所以没有公交。
  - 只放在平直地面段：资产名以`_T`结尾(隧道)、端点z<0、两端高度差>0.05(坡道)的不放。
  - 站所在的"被切开的路段"长度必须>2：每侧单独把路口缩进(`GetSetback`)和本侧车辆开口(`GetOpenings()`里`isVehicle`且`forwardSide`同侧，含小路接入和建筑出入口，`AssignRoads`在建筑/小路建好之后才跑)当堵点，补集里长度>2的路段中取离中点最近的，站落在该段内最靠近中点的位置(离段端>=1)；任一侧没有合格路段就整条路不放站(成对才能掉头)。
  - 站名=路名+序号(`RoadStationRequest::name`，如`东西第3路1`)，按路名计数，同名路(棋盘格里同一条横/竖线被路口切成多段`Road`)序号接着往下走。站在每侧最外侧车道上由Core的`ResolveAccessLane`保证，这里不处理。
- **手动站 `AssignManualRoads`（2026-11-23）**：编辑器"路网路线"里 `station=="station_bus"` 的站由它摆放（`StationFactory::RegisterManualAssign` 登记；Core `InitRoadsideStations` 发现数据里有本 mod 的站且登记了手动函数时**只**调它、跳过自动 `AssignRoads`——站按数据切换）。每站：`roads[stop.road]` → `BusProjectToRoad`（64 段采样找 (x,y) 的垂足，线性插值得弧长 t）→ `BusFindStopPosition(road, side, length, target)`（和自动站共用：路口缩进+本侧车辆开口当堵点，补集里长度>2 的段内取离 target 最近、离段端>=1 的位置，即**从目标 t 往两侧找最近的合格位置**）；整条路都没有合格段则**硬放在 t 上**并告警。单行道空侧兜底换到有车行道的一侧（两侧都没有则丢弃）。站名=`BusStopName(线下标,站下标)`（`bus_stop_name.h`，`BusStop_L<线>_S<站>`，下标是 `roadLines` 全局下标），route_bus 用同一个名字连线。

## 依赖关系

- 依赖：`Dependence/traffic/station_mod.h`、`Dependence/map/general_geometry.h`
  (`FACE_DIRECTION`/`PointParams`/`Road`)。

## 待办/后续阶段

- 火车站楼的新布局(1~3楼、轨道接口落在楼体内)还没进游戏看过效果，见`map/building_*.md`。
- 只验证了"全图恰好一个火车站+一个机场"的场景——`TrainRoute`/`AirRoute`的`LayoutRoute`
  假设全图只有一个真实站点(见`route_*.md`)，多个同类型站点同时存在的情况没有验证过，
  暂不支持。
