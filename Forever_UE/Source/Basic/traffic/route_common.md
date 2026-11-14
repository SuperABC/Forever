# route_common.h / route_common.cpp

## 职责

`TrainRoute`/`AirRoute`共用的小工具。`RayBoxIntersection`(射线与地图边界的交点，`AirRoute`算extern用)、`MakeBezierSegment`(造带端点 z 的 `TrackSegment`)，以及火车站间铁轨用的 `SampleBezier`/`OffsetRail`。

## 关键设计

- 原来的单站双跑道环线`LayoutDualTrackStationLoop`已删除：飞机改成按天空节点自己布线(见`route_air.md`)，火车改成多站绕地图中心成环(见`route_train.md`)，都不再共用它。

- **`MakeBezierSegment(x1,y1,z1,h1x,h1y,x2,y2,z2,h2x,h2y,arm,terrainFollow=false)`**：由两端点和水平朝向(h1 出发方向、h2 到达方向)造一段三次贝塞尔。`arm<=0` 时控制点取三等分点(直线)，否则沿朝向放 `arm`；控制点 z 取端点 z(两端水平切线)。`mesh`/`meshUnit` 由调用方事后填。
- `route_train`/`route_air` 的 `LayoutRoute` 用它拼 link 段。
- **`SampleBezier(curve, step, fallbackX, fallbackY)`**：把一段三次贝塞尔(只看平面)按弧长大约每 `step` 采样，返回含两端的 `CurveSample`(位置+单位切线)；导数退化成零向量时切线取 fallback。
- **`OffsetRail(samples, offset, reverse, z1, z2, terrainFollow)`**：把采样点沿右手法线(`(-ty,tx)`，+Y 向南)偏移 `offset`(正=右、负=左)，相邻点之间用 Hermite 三次贝塞尔连起来(臂=偏移弦长/3)，z 线性过渡。`reverse=true` 倒着走(切线取反)，同一组采样点的两个方向共用偏移基准所以永远平行。`route_train` 的站间铁轨用它(`route_train.md`)。

## 依赖关系

- 依赖：`traffic/station_mod.h`(`TrackSegment`)。
- 被谁依赖：`route_train.cpp`、`route_air.cpp`。
