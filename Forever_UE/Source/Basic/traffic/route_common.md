# route_common.h / route_common.cpp

## 职责

`TrainRoute`/`AirRoute`共用的小工具。`RayBoxIntersection`(射线与地图边界的交点，`AirRoute`算extern用)、`MakeBezierSegment`(造带端点 z 的 `TrackSegment`)，以及火车站间铁轨用的 `SampleBezier`/`OffsetRail`。

## 关键设计

- 飞机和火车各自布线，不共用线路布局函数：飞机按天空节点自己布线(见`route_air.md`)，火车多站绕地图中心成环(见`route_train.md`)。

- **`MakeBezierSegment(x1,y1,z1,h1x,h1y,x2,y2,z2,h2x,h2y,arm,terrainFollow=false)`**：由两端点和水平朝向(h1 出发方向、h2 到达方向)造一段三次贝塞尔。`arm<=0` 时控制点取三等分点(直线)，否则沿朝向放 `arm`；控制点 z 取端点 z(两端水平切线)。`mesh`/`meshUnit` 由调用方事后填。
- `route_train`/`route_air` 的 `LayoutRoute` 用它拼 link 段。
- **`SampleBezier(curve, step, fallbackX, fallbackY)`**：把一段三次贝塞尔(只看平面)按弧长大约每 `step` 采样，返回含两端的 `CurveSample`(位置+单位切线)；导数退化成零向量时切线取 fallback。
- **`OffsetRail(samples, offset, reverse, z1, z2, terrainFollow)`**：把采样点沿右手法线(`(-ty,tx)`，+Y 向南)偏移 `offset`(正=右、负=左)，相邻点之间用 Hermite 三次贝塞尔连起来(臂=偏移弦长/3)，z 线性过渡。`reverse=true` 倒着走(切线取反)，同一组采样点的两个方向共用偏移基准所以永远平行。`route_train` 的站间铁轨用它(`route_train.md`)。

## 依赖关系

- 依赖：`traffic/station_mod.h`(`TrackSegment`)。
- 被谁依赖：`route_train.cpp`、`route_air.cpp`。

## 手动线路规划（2026-11-23）

`TrainRoute`/`AirRoute` 在 `--skip` 时共用的规划，不碰 Core 容器，纯函数：

- **`PlanManualRoute(data, routeId, stations, allowTrackChange)`**：对 `data.loops` 里 `route==routeId` 的每个环：预设项用 `RouteStationInfo::presetIndex` 找站（找不到/端口不齐的项丢弃并写 `warnings`）；整环没有停靠站或可用项<2 → 丢弃整环并写 `warnings`。相邻两项若都是节点，在本 route 的连线图上按弧长求最短路：有 → "沿实体线路"（记用到的连线、途经节点的转向 `{node,到达连线,离开连线}`）；没有 → 一个**缺口**（`ManualGap`，编辑器里的虚线）。含站的相邻对一律是缺口。
- **站轨道选择**：站项按 `(本站-前一项)+(后一项-本站)` 的方向选航向最接近的那条轨道/跑道（`BestPort`）；`allowTrackChange=true`（飞机）时降落和起飞跑道分别按各自方向选，不同时在站内加一个 `withinStation` 缺口（滑行）并停两条跑道。不停的站不进线路条目，靠两侧缺口穿过站（`EDGE_THROUGH`）。
- **节点端口 `ManualPort`**：`NodeIn/NodeOut`=某连线在某节点的到达端/离开端，`Hub`=前后都是缺口时节点自己的中心点（朝向=前→后）。前后项在节点同一侧折回（如终点节点夹在同一个站两次之间，`dot(前→节点,节点→后)<-0.5`）时是**掉头节点**：`Hub`=到达端（朝向=前→节点）、`HubOut`=离开端（朝向相反），两个都进 `hubs`，火车侧在节点处各建一个 extern 并连半圆。
- **同一项在环里出现多次**：规划按环里的位置逐项处理，同一个站出现两次时轨道各按自己的前后方向选（往返→两条不同轨道），各自的停靠标志独立。线路条目里的节点项用它的"进入端口"当必经点（extern）。
- **几何小工具**：`ManualAxis`（连线的中轴线：二次 Bezier 升成三次，控制点 z 取端点 z → z 剖面是 smoothstep 扁 S，和编辑器一致）、`CubicPoint/CubicTangent`、`SubCubic`（de Casteljau 子段）、`ManualEndDirection`（连线在某节点向外的单位切线）。
