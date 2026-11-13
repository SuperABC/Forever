# route_common.h / route_common.cpp

## 职责

TrainRoute/AirRoute共用的单站双跑道(轨道)四接口环线拓扑`LayoutDualTrackStationLoop`。

## 关键设计

- **`LayoutDualTrackStationLoop`(TrainRoute/AirRoute共用)的拓扑**：
  ```
  leftEdgeIn -> leftIn -(轨道1)-> stop1 -> rightOut -> rightEdgeOut -> rightEdgeMid -> rightEdgeIn -> rightIn -(轨道2)-> stop2 -> leftOut -> leftEdgeOut -> leftEdgeMid -> (回到leftEdgeIn)
  ```
  `interfaces`固定顺序`[leftIn, rightOut, rightIn, leftOut]`(`TrainStation`/
  `AirStation::Layout`保证，见`station_*.md`)。`leftIn`/`rightOut`朝向相同，两点连线
  是跑道/轨道1；`rightIn`/`leftOut`同理是跑道/轨道2。每个地图边界有两条线：出地图的线（`xxOut`端点，
  沿出站接口的出发方向射线求和地图边界的交点`RayBoxIntersection`）和进地图的线（`xxIn`端点，沿进站接口
  出发方向的反向回溯到边界），两个端点之间插一个虚拟车站`xxMid`（位置、`z`取平均）。`Out->Mid->In`是贴着边界的
  一小段横向连线（Out出发/Mid两头/In到达都朝侧向，三点直线连接）。`Mid`是`dwell=true, silent=true`——火车
  会在那里停靠`dwellSeconds`，但不触发`enter_stop`/`leave_stop`事件（`Route::DriveVehicle`不把它报给边沿检测，
  `RouteLeg::silentDwell`，存档里用dwell字节的值2表示）。
  - `stop1`/`stop2`是各自跑道/轨道两端的中点(位置、朝向、`z`都是简单平均，两端共线同向，
    中点自然也在同一条线上)，和边缘站点一样借用"没有building的合成站点"
    机制(`RouteStationInfo.isEdge=true`，只是复用`Route::Build()`已有的创建+收集逻辑，不
    代表真的是地图边缘)。`leftIn`/`rightOut`/`rightIn`/`leftOut`和边缘的Out/In端点
    全部标`dwell=false`(纯过路点)，只有`stop1`/`stop2`是`dwell=true`——真正的停靠
    动作只应该发生在跑道/轨道的中点，不是每个过路接口都停一次。
  - `outEdgeStations`的八个元素(`leftEdgeOut/Mid/In`、`rightEdgeOut/Mid/In`、`stop1/stop2`)顺序决定了它们在
    `resolvedStations`里的下标(`realStation+1`到`+8`)，这个假设建立在"全图只有这一个真实
    站点"(`realStation = leftIn.stationIndex`恒定不变)之上，新增元素/调整顺序时要同步检查
    这几个下标常量。
- **`LayoutDualTrackStationLoop`的边界节点**：左边界沿`leftIn`朝向的反方向射线出图得到(飞机/火车从图外来的方向)，右边界沿`rightOut`朝向射线出图；边界节点的到达/出发朝向直接复用相邻接口的朝向(同向的复用，掉头的取反)。`overrideEdgeZ`为true时边缘站点z取`edgeHeight`(AirRoute的巡航高度)，否则取接口原本的z。

## 依赖关系

- 被谁依赖：`route_train`、`route_air`。
