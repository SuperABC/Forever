# route_basic.h / route_basic.cpp

## 职责

三个内置`RouteMod`实现：`BusRoute`(接入现有车道路网的环线分组)、`TrainRoute`/`AirRoute`
(共用`LayoutDualTrackStationLoop`，单站双跑道/轨道拓扑)。

## 关键设计

- **`BusRoute::LayoutRoute`不依赖任何"哪个方向是南/北/顺时针/逆时针"的假设**，直接从每个
  接口自己真实的`departDir`反推它天生属于哪个绕行方向：以所有接口的形心为参照，
  `radial x departDir`的2D叉积(`radial.x*dir.y - radial.y*dir.x`)符号是纯数学事实——同号
  的一组天然构成同一个绕行方向，叉积符号决定这组该按角度升序还是降序首尾相接(`cross>=0`
  时升序，`cross<0`时降序，才会贴着每个点自己的`departDir`走)。两组各自建一条环线，严格
  首尾相接。不要按"哪条路朝哪个方向"手工推导绕行方向(如假设固定的+Y朝向)——Map层的左右手
  公式一旦调整，手推的结论就会全部失效，必须从真实车道方向数据反推。
- **`LayoutDualTrackStationLoop`(TrainRoute/AirRoute共用)的拓扑**：
  ```
  左边界 -> leftIn -(跑道/轨道1)-> stop1 -> rightOut -> 右边界 -> rightIn -(跑道/轨道2)-> stop2 -> leftOut -> 左边界
  ```
  `interfaces`固定顺序`[leftIn, rightOut, rightIn, leftOut]`(`TrainStation`/
  `AirStation::Layout`保证，见`station_basic.md`)。`leftIn`/`rightOut`朝向相同，两点连线
  是跑道/轨道1；`rightIn`/`leftOut`同理是跑道/轨道2。左右边界沿各自接口的朝向射线求和地图
  边界的交点(`RayBoxIntersection`)，代表"飞机/火车从图外飞来/飞走"。
  - `stop1`/`stop2`是各自跑道/轨道两端的中点(位置、朝向、`z`都是简单平均，两端共线同向，
    中点自然也在同一条线上)，和`leftEdge`/`rightEdge`一样借用"没有building的合成站点"
    机制(`RouteStationInfo.isEdge=true`，只是复用`Route::Build()`已有的创建+收集逻辑，不
    代表真的是地图边缘)。`leftIn`/`rightOut`/`rightIn`/`leftOut`/`leftEdge`/`rightEdge`
    六个点全部标`dwell=false`(纯过路点)，只有`stop1`/`stop2`是`dwell=true`——真正的停靠
    动作只应该发生在跑道/轨道的中点，不是每个过路接口都停一次。
  - `outEdgeStations`的四个元素(`leftEdge/rightEdge/stop1/stop2`)顺序决定了它们在
    `resolvedStations`里的下标(`realStation+1`到`+4`)，这个假设建立在"全图只有这一个真实
    站点"(`realStation = leftIn.stationIndex`恒定不变)之上，新增元素/调整顺序时要同步检查
    这几个下标常量。
- **`speed`/`dwellSeconds`按`timeFlowRatio`换算**——三个具体类型的构造函数都在基类默认值
  (`speed=1.f`/`dwellSeconds=30.f`，对应120地图单位/真实秒、0.25真实秒停靠)之上重新校准，
  具体数值和推导见各自构造函数的注释，背景见`route_mod.md`"speed/dwellSeconds的单位"一节。
  公交环线短，火车/飞机环线长(大部分长度是"站点到地图边缘"的长途)，三者的`speed`/
  `dwellSeconds`因此不一样。

## 依赖关系

- 依赖：`Dependence/traffic/route_mod.h`。
- 被谁依赖：`Basic/basic.cpp`注册这三个类型进`config.json`的`"route_mods"`；
  `Core/traffic/traffic.cpp`的`InitRoutes`通过`RouteFactory`创建、调`Route::Build`
  (间接触发`LayoutRoute`)。

## 待办/后续阶段

- `TrainRoute`/`AirRoute`假设全图只有一个真实站点(`LayoutDualTrackStationLoop`直接用
  `interfaces[0..3]`，不做分组)，多个同类型站点同时存在的情况不支持，真需要时要把
  `BusRoute`那套"不依赖假设、从真实数据反推"的思路搬过来用在"多个站点怎么分组连线"这一层。
- `trackMesh`留空，铺真实轨道的逻辑没有实现，见`route_mod.md`"待办"一节。
