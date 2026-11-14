# station_bus.h / station_bus.cpp

## 职责

`BusStation`(`station_bus`)：station域的默认实现之一。

## 关键设计

- **同一条轨道的两个接口朝向必须相同**(沿frontage轴同一方向)，`Route::Build()`算出的贝塞尔控制点才会贴着两点连线、是一条直的"轨道"。
- **机场：楼体贴路、跑道接口留在远离道路一侧**——`AirportBuilding::Layout()`(`Basic/map/building_*.cpp`)用`NearRoadFootprint`把楼体压到靠近道路的一侧，跑道`depthRatio`(`0.65`/`0.85`)在楼体深度范围之外，保证连线不穿过楼体——两边的深度数值改动要互相对照。
- **火车站：楼体占满整个Building区域**(默认footprint，不贴路一侧)，所以轨道接口(`depthRatio` `0.45`/`0.55`)落在楼体范围内：楼体是1楼`preset_station_train_fg+`(大厅+4间`room_empty`，层高0.75)+2楼`preset_station_train_fl+-`(层高0.75)+3楼`preset_station_train_fu-`(层高1.0)，没有地下层，三层各4台电梯，端口 z=1.5(恰是3楼地面，0.75+0.75)。接口本身的世界坐标换算不受楼体footprint偏移影响(见`Core/traffic/station.md`"接口坐标换算"一节)。
- **`BusStation::AssignRoads`(棋盘格大路公交站)**：只在4车道大路(`GetVehicleLanes(0/1).size()>=2`)上、长度`CalcDistance()`在[24,96]的`Road`(跨1~2格，跨3格的会误加，不管)靠中点放一对站，两侧各一个(`rightSide`=side0)。井字路网没有4车道大路，所以没有公交。
  - 只放在平直地面段：资产名以`_T`结尾(隧道)、端点z<0、两端高度差>0.05(坡道)的不放。
  - 站所在的"被切开的路段"长度必须>2：每侧单独把路口缩进(`GetSetback`)和本侧车辆开口(`GetOpenings()`里`isVehicle`且`forwardSide`同侧，含小路接入和建筑出入口，`AssignRoads`在建筑/小路建好之后才跑)当堵点，补集里长度>2的路段中取离中点最近的，站落在该段内最靠近中点的位置(离段端>=1)；任一侧没有合格路段就整条路不放站(成对才能掉头)。
  - 站名=路名+序号(`RoadStationRequest::name`，如`东西第3路1`)，按路名计数，同名路(棋盘格里同一条横/竖线被路口切成多段`Road`)序号接着往下走。站在每侧最外侧车道上由Core的`ResolveAccessLane`保证，这里不处理。

## 依赖关系

- 依赖：`Dependence/traffic/station_mod.h`、`Dependence/map/general_geometry.h`
  (`FACE_DIRECTION`/`PointParams`/`Road`)。

## 待办/后续阶段

- 火车站楼的新布局(1~3楼、轨道接口落在楼体内)还没进游戏看过效果，见`map/building_*.md`。
- 只验证了"全图恰好一个火车站+一个机场"的场景——`TrainRoute`/`AirRoute`的`LayoutRoute`
  假设全图只有一个真实站点(见`route_*.md`)，多个同类型站点同时存在的情况没有验证过，
  暂不支持。
