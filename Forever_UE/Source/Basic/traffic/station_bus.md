# station_bus.h / station_bus.cpp

## 职责

`BusStation`(`station_bus`)：station域的默认实现之一。

## 关键设计

- **同一条轨道的两个接口朝向必须相同**(沿frontage轴同一方向)，`Route::Build()`算出的贝塞尔控制点才会贴着两点连线、是一条直的"轨道"。
- **机场：楼体贴路、跑道接口留在远离道路一侧**——`AirportBuilding::Layout()`(`Basic/map/building_*.cpp`)用`NearRoadFootprint`把楼体压到靠近道路的一侧，跑道`depthRatio`(`0.65`/`0.85`)在楼体深度范围之外，保证连线不穿过楼体——两边的深度数值改动要互相对照。
- **火车站：楼体占满整个Building区域**(默认footprint，不贴路一侧)，所以轨道接口(`depthRatio` `0.55`/`0.65`)落在楼体范围内：楼体是地下1层`preset_train_station_bg+`(4条楼梯通上来)+地面1层`preset_train_station_fg-`(大厅+左右两间`room_train_station`)，轨道在地面层之下/之中穿过。接口本身的世界坐标换算不受楼体footprint偏移影响(见`Core/traffic/station.md`"接口坐标换算"一节)。
- `BusStation::AssignRoads`只贴井字路网中心正方形的四条边道路(`JingRoadnet::
  DistributeRoadnet`里固定这几个名字)，每条路取中点(`t=0.5`)、两侧(`rightSide=true/false`)
  各摆一个——只验证这一个测试场景，不遍历全图所有路。

## 依赖关系

- 依赖：`Dependence/traffic/station_mod.h`、`Dependence/map/general_geometry.h`
  (`FACE_DIRECTION`/`PointParams`/`Road`)。

## 待办/后续阶段

- 火车站楼的新布局(地下层+地面层、轨道接口落在楼体内)还没进游戏看过效果，见`map/building_*.md`。
- 只验证了"全图恰好一个火车站+一个机场"的场景——`TrainRoute`/`AirRoute`的`LayoutRoute`
  假设全图只有一个真实站点(见`route_*.md`)，多个同类型站点同时存在的情况没有验证过，
  暂不支持。
