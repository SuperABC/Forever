# station_air.h / station_air.cpp

## 职责

`AirStation`(`station_air`)：station域的默认实现之一。

## 关键设计

- 接口点坐标/朝向的换算见`station_common.md`(`NearRoadPoint`/`AlongAxisHeadingDegrees`)。
- **`TrainStation`/`AirStation`的4接口固定顺序`[leftIn, rightOut, rightIn, leftOut]`**——
  `leftIn`/`rightOut`朝向相同(`headingToRight`)、深度相同(`kTrackDepth1`/`kRunwayDepth1`)，
  两点连线是"轨道/跑道1"；`rightIn`/`leftOut`朝向相同(`headingToLeft`)、深度相同
  (`kTrackDepth2`/`kRunwayDepth2`)，连线是"轨道/跑道2"。这个顺序被
  `Basic/traffic/route_*.cpp`的`LayoutDualTrackStationLoop`硬编码依赖(按下标
  `0/1/2/3`直接取，不是按名字查)，两边必须保持一致。深度分两档(火车`0.55`/`0.65`，飞机`0.65`/`0.85`)是为了让两条
  轨道/跑道在空间上分开，不重叠在同一条线上。
- **同一条轨道的两个接口朝向必须相同**(沿frontage轴同一方向)，`Route::Build()`算出的贝塞尔控制点才会贴着两点连线、是一条直的"轨道"。
- **接口`z`抬高到`0.001`(1cm)而不是贴地的`0`**——只是让调试连线不和地面/楼体网格糊在一起，
  `AirStation`的跑道本身仍然贴地，真正的"巡航高度"由`AirRoute::LayoutRoute`在生成地图边缘
  站点时额外叠加(`overrideEdgeZ`/`edgeHeight`参数，只覆盖边缘点)。这个`z`同时是车辆真正
  停靠在这个接口时的实际高度，不能设太大，否则飞机/火车停下来会明显飘在半空中。
- **机场：楼体贴路、跑道接口留在远离道路一侧**——`AirportBuilding::Layout()`(`Basic/map/building_*.cpp`)用`NearRoadFootprint`把楼体压到靠近道路的一侧，跑道`depthRatio`(`0.65`/`0.85`)在楼体深度范围之外，保证连线不穿过楼体——两边的深度数值改动要互相对照。
- **火车站：楼体占满整个Building区域**(默认footprint，不贴路一侧)，所以轨道接口(`depthRatio` `0.55`/`0.65`)落在楼体范围内：楼体是地下1层`preset_train_station_bg+`(4条楼梯通上来)+地面1层`preset_train_station_fg-`(大厅+左右两间`room_train_station`)，轨道在地面层之下/之中穿过。接口本身的世界坐标换算不受楼体footprint偏移影响(见`Core/traffic/station.md`"接口坐标换算"一节)。

## 依赖关系

- 依赖：`Dependence/traffic/station_mod.h`、`Dependence/map/general_geometry.h`
  (`FACE_DIRECTION`/`PointParams`/`Road`)。
- 被谁依赖：`Basic/map/building_*.cpp`的`TrainStationBuilding`/`AirportBuilding`
  (`stationMod`字段分别指向`"station_train"`/`"station_air"`，依赖表里是站点依赖楼：没有楼就不会生成站点)；
  `Basic/traffic/route_*.cpp`的`LayoutDualTrackStationLoop`硬编码依赖
  `TrainStation`/`AirStation`声明的4接口固定顺序；`Basic/basic.cpp`注册这三个类型进
  `config.json`的`"station_mods"`。

## 待办/后续阶段

- 火车站楼的新布局(地下层+地面层、轨道接口落在楼体内)还没进游戏看过效果，见`map/building_*.md`。
- 只验证了"全图恰好一个火车站+一个机场"的场景——`TrainRoute`/`AirRoute`的`LayoutRoute`
  假设全图只有一个真实站点(见`route_*.md`)，多个同类型站点同时存在的情况没有验证过，
  暂不支持。
