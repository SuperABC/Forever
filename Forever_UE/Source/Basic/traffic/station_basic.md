# station_basic.h / station_basic.cpp

## 职责

三个内置`StationMod`实现：`BusStation`(贴路，不占Lot面积)、`TrainStation`/`AirStation`
(挂建筑，各4个接口/两条轨道或跑道)。

## 关键设计

- **两个坐标换算helper**(匿名namespace，三个类共用)：
  - `NearRoadPoint(direction, alongRatio, depthRatio)`——`alongRatio`是沿道路方向(frontage
    轴)的位置，`depthRatio`是垂直道路方向(0=贴着道路那条边，1=远离道路那条边)，按
    `direction`换算成quad局部坐标系下的`PointParams`。`depthRatio`在`FACE_EAST`/
    `FACE_NORTH`要翻转(`1-depthRatio`)，`alongRatio`任何方向都不翻转——这个不对称是因为
    `depthRatio`的"0=贴路"要跟着`direction`贴的是哪条边走，`alongRatio`只是沿同一条边
    平移，不需要跟着翻。
  - `AlongAxisHeadingDegrees(direction, towardIncreasing)`——沿frontage轴(`alongRatio`
    增大方向)的朝向角度。因为`alongRatio`从不翻转，它始终映射到quad局部坐标系固定的一根轴
    (`FACE_WEST`/`FACE_EAST`时是Y轴，`FACE_NORTH`/`FACE_SOUTH`时是X轴)，所以"朝
    `alongRatio`增大方向"永远只有两个可能角度(90°或0°)，和`direction`贴的是哪条边无关。
    这个函数2026-10-03原来是按`FaceNormalDegrees(direction)±90`算的，那套公式对
    `EAST`/`NORTH`两个方向符号会算反(`FaceNormalDegrees`本身对这两个方向不对称)，PIE实测
    反馈"公交车的行驶方向有点问题"排查到车道左右手公式的bug时，连带发现这个±90的写法也不
    对，换成现在这个直接从`alongRatio`映射到的轴反推的版本，不再绕`FaceNormalDegrees`。
- **`TrainStation`/`AirStation`的4接口固定顺序`[leftIn, rightOut, rightIn, leftOut]`**——
  `leftIn`/`rightOut`朝向相同(`headingToRight`)、深度相同(`kTrackDepth1`/`kRunwayDepth1`)，
  两点连线是"轨道/跑道1"；`rightIn`/`leftOut`朝向相同(`headingToLeft`)、深度相同
  (`kTrackDepth2`/`kRunwayDepth2`)，连线是"轨道/跑道2"。这个顺序被
  `Basic/traffic/route_basic.cpp`的`LayoutDualTrackStationLoop`硬编码依赖(按下标
  `0/1/2/3`直接取，不是按名字查)，两边必须保持一致——改这边的顺序要连带检查那边。
  深度分两档(`0.65`/`0.85`)是为了让两条轨道/跑道在空间上分开，不会重叠在同一条线上。
- **接口`z`抬高到`0.1`而不是贴地的`0`**——PIE实测反馈接口太低时调试连线和地面/楼体网格
  糊在一起看不清楚；`AirStation`的跑道本身仍然贴地(`0.1`只是让调试线可见，不代表真实海拔)，
  真正的"巡航高度"是`AirRoute::LayoutRoute`在生成地图边缘站点时额外叠加的
  (`LayoutDualTrackStationLoop`的`overrideEdgeZ`/`edgeHeight`参数，只覆盖边缘点，不影响
  跑道本身的接口)。
- **楼体footprint贴路、接口留在远离道路一侧**——`TrainStationBuilding`/`AirportBuilding`的
  `Layout()`(`Basic/map/building_basic.cpp`)用`NearRoadFootprint`把楼体压到靠近道路的一侧
  (深度0~0.37左右)，这两个`Layout()`的`depthRatio`(`0.65`/`0.85`)都在楼体深度范围之外，
  保证公共交通连线不穿过楼体实体——这是PIE实测反馈"公共交通路线穿过楼体"之后定下的数值
  配合关系，两边的深度数值改动要互相对照，不能只改一边。接口本身的世界坐标换算不受楼体
  footprint偏移影响(见`Core/traffic/station.md`"接口坐标换算"一节)，这里的"配合"纯粹是
  "两个深度区间不要重叠"这个几何约束，不是数值上的强耦合。
- `BusStation::AssignRoads`只贴井字路网中心正方形的四条边道路(`JingRoadnet::DistributeRoadnet`
  里固定这几个名字)，每条路取中点(`t=0.5`)、两侧(`rightSide=true/false`)各摆一个——这次
  只验证这一个测试场景，不是遍历全图所有路。

## 依赖关系

- 依赖：`Dependence/traffic/station_mod.h`、`Dependence/map/geometry.h`
  (`FACE_DIRECTION`/`PointParams`/`Road`)。
- 被谁依赖：`Basic/map/building_basic.cpp`的`TrainStationBuilding`/`AirportBuilding`
  (`stationMod`字段分别指向`"station_train"`/`"station_air"`)；
  `Basic/traffic/route_basic.cpp`的`LayoutDualTrackStationLoop`硬编码依赖
  `TrainStation`/`AirStation`声明的4接口固定顺序；`Basic/basic.cpp`注册这三个类型进
  `config.json`的`"station_mods"`。

## 待办/后续阶段

- 这次只验证了"全图恰好一个火车站+一个机场"的场景——`TrainRoute`/`AirRoute`的
  `LayoutRoute`假设全图只有一个真实站点(见`route_basic.md`)，多个同类型站点同时存在的情况
  没有验证过，暂不支持。
