# station_air.h / station_air.cpp

## 职责

`AirStation`(`station_air`)：station域的默认实现之一。

## 关键设计

- 端口坐标/朝向的换算见`station_common.md`(`NearRoadPoint`/`AlongAxisHeadingDegrees`)。
- **端口模型**：同 `station_train`，两条跑道 t=0/1 各有 `inlet t`/`outlet t`/`stop t`。沿路比例 0.3/0.7、进深 0.7/0.9，stop 在 0.5。同一跑道三个端口朝向相同，连线是直线，**全部没有 mesh**(机场跑道由楼体表现)。每条跑道三条连线：inlet→stop、stop→outlet、inlet→outlet。
- **端口 `z`**：抬高到 `0.001`(1cm)，让调试连线不和地面/楼体糊在一起，同时是车辆停靠高度，不能设太大。巡航高度由 `AirRoute::LayoutRoute` 在 link 的段里直接写，不再有 `overrideEdgeZ`。
- **机场：楼体占满整个Building区域**(2026-11-22改，默认footprint，同火车站；原来`AirportBuilding::Layout()`用`NearRoadFootprint`把楼体压到靠路一侧、跑道留在楼体之外)——跑道深度(`0.7`/`0.9`)因此落在楼体范围之内，端口/连线的世界坐标只取决于占地矩形，不受footprint影响。

## 依赖关系

- 依赖：`Dependence/traffic/station_mod.h`、`Dependence/map/general_geometry.h`
  (`FACE_DIRECTION`/`PointParams`/`Road`)。
- 被谁依赖：`Basic/map/building_*.cpp`的`TrainStationBuilding`/`AirportBuilding`
  (`stationMod`字段分别指向`"station_train"`/`"station_air"`，依赖表里是站点依赖楼：没有楼就不会生成站点)；
  `TrainRoute::LayoutRoute`/`AirRoute::LayoutRoute`依赖"轨道/跑道 t 的端口下标=t"；`Basic/basic.cpp`注册这三个类型进
  `config.json`的`"station_mods"`。

## 待办/后续阶段

- 多机场只做过无头线路规划验证(见`route_air.md`)，**未 PIE**。
