# route_basic.h / route_basic.cpp

## 职责

三个内置`RouteMod`实现：`BusRoute`(接入现有车道路网的环线分组)、`TrainRoute`/`AirRoute`
(共用`LayoutDualTrackStationLoop`，单站双跑道/轨道拓扑)。

## 关键设计

- **`BusRoute::LayoutRoute`——不依赖任何"哪个方向是南/北/顺时针/逆时针"的假设**，直接从
  每个接口自己真实的`departDir`反推它天生属于哪个绕行方向：以所有接口的形心为参照，
  `radial x departDir`的2D叉积(`radial.x*dir.y - radial.y*dir.x`)符号是纯数学事实——同号
  的一组天然构成同一个绕行方向，叉积符号本身就决定了这组该按角度升序还是降序首尾相接
  (`cross>=0`意味着这个点的朝向和"角度增大方向"同向，必须升序才会贴着每个点自己的
  `departDir`走；`cross<0`则相反，必须降序)。两组各自建一条环线，严格首尾相接，不依赖
  "哪条路朝哪个方向"这类需要手工推导的假设。
  - **这是第三版了，前两版都踩过坑**：第一版是通用"贪心最近邻聚类"，不知道接口贴在路的
    哪一侧/哪个朝向，绕法随机(PIE反馈"不是按井字中心的正方形行驶的")。第二版手推了"中山X路
    Start->End方向+side0/side1物理位置"来判断外侧升序=逆时针、内侧降序=顺时针，但那次手推
    假设的是"+Y=北"，后来确认这个项目实际是"+Y=南"(排查公交车行驶方向bug时发现，见
    `Core/map/map.cpp`里`Map::ComputeLaneAnchorPosition`声明处的完整说明)——而且这种手推
    方式本身就很脆弱，Map层的左右手公式一旦订正，这里全部要跟着重新手推一遍，PIE实测反馈
    "原先靠左行驶的时候顺时针连接现在靠右行驶了需要逆时针连接"正是这个脆弱性的直接体现。
    现在这一版直接从真实数据反推，不管Map层左右手约定以后再怎么变，这里都会自动跟着真实
    车道方向重新分组。
- **`LayoutDualTrackStationLoop`(TrainRoute/AirRoute共用)的拓扑**：
  ```
  左边界 -> leftIn -(跑道/轨道1)-> stop1 -> rightOut -> 右边界 -> rightIn -(跑道/轨道2)-> stop2 -> leftOut -> 左边界
  ```
  `interfaces`固定顺序`[leftIn, rightOut, rightIn, leftOut]`(`TrainStation`/`AirStation::
  Layout`保证，见`station_basic.md`)。`leftIn`/`rightOut`朝向相同，两点连线是跑道/轨道1；
  `rightIn`/`leftOut`同理是跑道/轨道2。左右边界沿各自接口的朝向射线求和地图边界的交点
  (`RayBoxIntersection`)，代表"飞机/火车从图外飞来/飞走"。
  - **`stop1`/`stop2`是2026-10-04新增的真正停靠点**——取各自跑道/轨道两端的中点(位置、
    朝向、`z`都是简单平均，因为两端共线同向，中点自然也在同一条线上，不会在这一点产生
    拐弯)，和`leftEdge`/`rightEdge`一样借用"没有building的合成站点"机制
    (`RouteStationInfo.isEdge=true`只是为了复用`Route::Build()`已有的创建+收集逻辑，不
    代表真的是地图边缘)。`leftIn`/`rightOut`/`rightIn`/`leftOut`/`leftEdge`/`rightEdge`
    六个点全部标`dwell=false`(纯过路点)，只有`stop1`/`stop2`是`dwell=true`——PIE实测反馈
    "火车和飞机都是在入站接口停一下、再在出站接口停一下，这不合理"之前，整条环线8个站点
    全部会停一次，现在只在两条跑道/轨道各自的中点真正停一次。
  - `outEdgeStations`的四个元素(`leftEdge/rightEdge/stop1/stop2`)顺序决定了它们在
    `resolvedStations`里的下标(`realStation+1`到`+4`)——这个假设建立在"全图只有这一个真实
    站点"(`realStation = leftIn.stationIndex`恒定不变)之上，新增元素/调整顺序时要同步检查
    这几个下标常量。
- **`speed`/`dwellSeconds`按`timeFlowRatio`换算**——三个具体类型的构造函数都在基类默认值
  (`speed=1.f`/`dwellSeconds=30.f`，对应真实观感是120地图单位/真实秒、0.25真实秒停靠)之上
  重新校准，具体数值和推导见各自构造函数的注释，背景见`route_mod.md`"speed/dwellSeconds的
  单位"一节。公交环线短，火车/飞机环线长(大部分长度是"站点到地图边缘"的长途)，三者的
  `speed`/`dwellSeconds`因此不一样，不是随手抄同一组数字。

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
