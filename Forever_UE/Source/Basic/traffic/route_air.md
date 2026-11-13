# route_air.h / route_air.cpp

## 职责

`AirRoute`(`route_air`)：route域的默认实现之一。

## 关键设计

- 单站双跑道四接口环线拓扑见`route_common.md`(`LayoutDualTrackStationLoop`)。
- **`speed`/`dwellSeconds`的单位是游戏内秒，不是真实秒**：`Traffic::Tick`喂给`Route::Update`的`elapsedSeconds`来自`Player::Tick()`里`time->AddMilliseconds(delta*60*1000*timeFlowRatio)`，默认`timeFlowRatio=2.0`，即1真实秒=120游戏内秒。三个类型在`SetProperty`里按下表校准(基类默认值`speed=1`/`dwellSeconds=30`)；如果`time_flow_ratio`被剧情脚本改了，真实秒观感会等比例变化，要一起重新校准。背景见`route_mod.md`"speed/dwellSeconds的单位"一节。

  | 类型 | 环线长度(地图单位) | 目标 | speed | dwellSeconds | easeSeconds |
  |---|---|---|---|---|---|
  | 公交 | 约120~180 | 一圈40~60真实秒、停站约3真实秒 | 0.025(3单位/真实秒) | 360 | 20 |
  | 火车 | 约2000(大部分是站到地图边缘的长途) | 一圈约170真实秒、进站停约5真实秒 | 0.1(12单位/真实秒) | 600 | 40 |
  | 飞机 | 约2050 | 一圈约100真实秒、停约6真实秒 | 0.175(21单位/真实秒) | 720 | 50 |

- **`easeSeconds`(启停缓动)**：火车/飞机体型大，启动刹车更缓。`DriveVehicle()`会自动把加减速时长钳在单个行驶区段时长的一半以内，所以公交每站都停时区段退化成单条leg，站间距短的leg会变成全程加速+全程减速、没有匀速中段，这是"尽量缓"在站间距有限时的最大效果，直接给足够大的值即可。详见`route.md`"DriveVehicle的缓动"一节。

## 依赖关系

- 依赖：`Dependence/traffic/route_mod.h`。
- 被谁依赖：`Basic/basic.cpp`注册这三个类型进`config.json`的`"route_mods"`；
  `Core/traffic/traffic.cpp`的`InitRoutes`通过`RouteFactory`创建、调`Route::Build`
  (间接触发`LayoutRoute`)。

## 待办/后续阶段

- `TrainRoute`/`AirRoute`假设全图只有一个真实站点(`LayoutDualTrackStationLoop`直接用
  `interfaces[0..3]`，不做分组)，多个同类型站点同时存在的情况不支持，真需要时要把
  `BusRoute`那套"不依赖假设、从真实数据反推"的思路搬过来用在"多个站点怎么分组连线"这一层。
