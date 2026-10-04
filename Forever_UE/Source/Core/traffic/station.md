# station.h / station.cpp

## 职责

`Station`：一个上下车点的实体。三种创建场景对应三个构造函数——挂在建筑上的真实站点(火车站/
机场)、没有建筑也没有mod的边缘站点(线路两头伸向地图边界用)、贴着道路摆的真实站点(公交站)。
`EnsureRoadLink`额外负责"公交接入现有车辆路网"这条旁路的惰性构造。

## 关键设计

- **必须在所有建筑落地之后创建**——`Traffic::InitStations()`遍历`map`所有`building`时，
  building的`Layout()`/导航图合并早已跑完，这里只读`Building`已经缓存好的数据
  (`GetDirection()`/`GetSizeX()`/`GetSizeY()`/`LocalToWorld()`)，不会触发building自己的
  二次布局。调用方(`Traffic::Init()`)的调用顺序因此是硬约束，不是随意的。
- **接口坐标换算要先抵消`bodyOffset`才能喂给`LocalToWorld`**：`StationInterfaceSpec.position`
  是"占地矩形局部坐标"(原点左下角，见`station_mod.md`)，但`Building::LocalToWorld`只认
  "楼体局部坐标"(原点也是左下角，但以楼体footprint而不是占地矩形为基准)。换算公式
  (`station.cpp`构造函数)：
  ```
  quadLocalX = position.ratioX * quadSizeX + position.offsetX        // 占地矩形局部
  bodyLocalX = quadLocalX - quadSizeX*0.5 + bodySizeX*0.5 - bodyOffsetX  // 楼体局部
  ```
  代入`Building::LocalToWorld`自己的公式(`relX = bodyLocalX - bodySizeX*0.5 + bodyOffsetX`)
  会发现`bodySizeX`/`bodyOffsetX`两项正好完全抵消，最终只剩`relX = quadLocalX - quadSizeX*0.5`
  ——**接口的世界坐标只取决于占地矩形局部坐标，和楼体footprint的偏移(`BuildingFootprintSpec`)
  完全无关**。这不是巧合，是这次把楼体"贴路"(`NearRoadFootprint`，见
  `Basic/map/building_basic.md`)时特意验证过的性质：楼体想往路边挪多远，站台/跑道接口的
  位置都不会跟着挪，不需要在改`footprint`的同时也改`StationInterfaceSpec`的数值。
- **贴路站点(公交站)的世界坐标必须精确落在车道中线上，不能是手搓的大概偏移**——最初的实现
  在`Traffic::InitRoadsideStations()`里自己算"路中心点+半路宽偏移"，这个偏移量和
  `EnsureRoadLink`后续用`Map::GetLaneSegmentAt`解出来的真实车道位置不是同一个点：公交车
  到站时（直接设成这个站点坐标）和行驶时（沿`EnsureRoadLink`解出来的车道几何走）于是
  在两个不同的点之间瞬移。修正后`Traffic::InitRoadsideStations()`直接调用
  `Map::GetLaneSegmentAt`拿真实车道坐标——和`EnsureRoadLink`走的是完全同一次查询逻辑，
  站点天生就"长在"车道上，不会有这个问题。`attachedRoad`字段记下贴的是哪条路，供没有
  `building`时`EnsureRoadLink`判断该查哪条路。
- **`StationRoadLink`是两点直连的弦，不是对原车道的真正切割**：`stationNode`新建、只属于
  这个`Station`，`inEdge`/`outEdge`是`laneFrom->stationNode->laneTo`两段直线`Connection`，
  都不登记进`Map`的`throughLines`/`vehicleNavGraph`，不影响原车道、不产生新的路口开口——
  和`Map::BreakThroughLine`给既有车道新增访问点时的精度一致(这个项目目前所有"车道中间插入
  节点"都是直线近似，不是真的切曲线，见`Dependence/map/geometry.md`"公交接入路网"一节的
  历史记录)，公交站的旁路照抄同一套精度，不需要发明贝塞尔细分算法。`t`记这个站点在
  `sourceEdge`上的弧长比例，供`Route::BuildEdgeGeometry`判断"两个站点是否在同一段原车道上
  且顺序对不对"这个特例(同车道直接走直线弦，不绕一圈`FindVehiclePath`)。
- `EnsureRoadLink`惰性构造+按`interfaceIndex`缓存在`roadLinks`里，重复调用直接返回缓存；
  两种查路方式(`building`存在查`GetBoundaryRoad(GetDirection())`，否则查`attachedRoad`)
  都没有可用的`Road`，或`Map::GetLaneSegmentAt`查不到车道(比如单行道只有一侧)时返回
  `nullptr`，调用方(`Route::BuildEdgeGeometry`)据此判定这条边构建失败。

## 依赖关系

- 依赖：`Core/map/building.h`(`LocalToWorld`/`GetBoundaryRoad`/`GetBodySizeX`等)、
  `Core/map/map.h`(`GetLaneSegmentAt`)、`traffic/station_mod.h`/`station_factory.h`。
- 被谁依赖：`Core/traffic/route.cpp`(`Route::BuildEdgeGeometry`读`GetInterfaces()`/调
  `EnsureRoadLink`)；`Core/traffic/traffic.cpp`(`InitStations`/`InitRoadsideStations`
  创建三种场景各自的`Station`)。

## 待办/后续阶段

- 无特别延后的逻辑——三种构造场景、`EnsureRoadLink`旁路都已经是这次验证通过的完整实现。
