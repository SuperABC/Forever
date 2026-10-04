# route_mod.h / route_mod.cpp

## 职责

`RouteMod`：一类线路(公交/火车/飞机各一个具体子类)的连接逻辑——把Core收集到的、
`stationType`匹配的所有站点接口组织成一张有向图+若干条环线。Core(`Core/traffic/route.h`
的`Route`)只负责把这张图变成真实几何、按时刻表驱动车辆，完全不关心"具体怎么连"，这个
职责全部交给`LayoutRoute()`的具体实现决定。

## 关键设计

- **`RouteStationInfo`是只读、世界坐标的站点接口快照**——`Route::Build()`收集完所有
  `stationType`匹配的真实接口后，连同"本次调用之前其它`Route`已经请求创建的边缘站点"
  (`isEdge=true`)一起喂给`LayoutRoute()`；mod只能读，不持有所有权，也不能直接碰Core的
  任何对象(这是这次排查"公交车静止不动"/"公交车行驶方向有问题"系列bug之后格外要强调的一
  点——`RouteMod`本身是运行在mod DLL里的代码，绝对不能像`Lot::FindAdaptivePlacement`
  那次踩过的坑一样直接调用Core对象的非虚成员函数，见`Dependence/map/geometry.md`"跨DLL
  分配器"一节；这次`RouteMod`的输出渠道全部是"往`edgeStations`/`links`/`lines`这三个自己
  持有的`vector`里追加数据"，和`BuildingMod::floors`同一个安全模式)。
- **`RouteStop.dwell`(2026-10-04新增)**：`false`表示这一站是纯过路点，到达后不停留、直接
  接着跑下一腿；默认`true`保持老行为。公交每一站都要停是对的，但火车/飞机的入站/出站接口、
  地图边缘点都不该停——真实世界里乘客在跑道/站台**中段**上下客，不是在跑道两端分别停一次，
  PIE实测反馈"火车和飞机都是在入站接口停一下、再在出站接口停一下"之后才补上这个字段，
  具体用法见`Basic/traffic/route_basic.md`的`LayoutDualTrackStationLoop`一节。
- **`RouteLink.bidirectional`**：`true`表示同一份几何两个方向都能走(比如单线铁路)，
  Core建图时会额外登记一条反向边，倒序遍历同一份`segments`、切线取反，不复制几何(见
  `route.md`"有向图与双向边"一节)。这次三个内置线路(公交/火车/飞机)全部是单向环线，没有
  真正用到这个字段，留给以后"同一段轨道两个方向都要走"的线路类型用。
- **`speed`/`dwellSeconds`的单位是"地图单位/游戏内秒"、"游戏内秒"，不是真实秒**——
  `Traffic::Tick`喂给`Route::Update`的`elapsedSeconds`来自`Player::Tick()`里
  `time->AddMilliseconds(delta*60*1000*timeFlowRatio)`，默认`timeFlowRatio=2.0`，即1个
  真实秒=120个游戏内秒。继续用这个结构体的裸默认值(`speed=1.f`/`dwellSeconds=30.f`)会让
  车跑得飞快、停靠几乎看不见，PIE实测反馈"所有载具的移动速度都太快了，停站的时间也太短了"
  才发现——三个具体子类(`Basic/traffic/route_basic.cpp`)都在构造函数里按这个120倍换算
  重新校准过，新增线路类型时同样要留意这个换算，不能直接抄字面上看起来"合理"的数值。
- `useRoadnet`/`drawPath`/`trackMesh`/`trackUnit`/`controlLength`几个选项字段的具体用法
  见`route.md`"非路网边：三次贝塞尔"/"路网边：同车道特例"两节。

## 依赖关系

- 依赖：无(纯`std::string`/`std::vector`，不依赖`geometry.h`等其它Dependence头)。
- 被谁依赖：`Core/traffic/route.h`的`Route::Build()`读取这些结构、调`LayoutRoute()`；
  `Basic/traffic/route_basic.h`的`BusRoute`/`TrainRoute`/`AirRoute`三个具体实现扩展这个
  基类。

## 待办/后续阶段

- `trackMesh`/`trackUnit`铺真实轨道mesh的逻辑没有任何内置线路用到(三个都留空字符串)，
  `Forever/Framework/ForeverTrafficFrameworkComponent::BuildTracks()`目前只在
  `drawPath&&!trackMesh.empty()`时才会真正铺(这次走不到这个分支，只打一条Log)，留给以后
  有真实轨道资产的内容类型消费。
