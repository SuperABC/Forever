# route_air.h / route_air.cpp

## 职责

`AirRoute`(`route_air`)：route域的默认实现之一。

## 关键设计

- 单站双跑道四接口环线拓扑见`route_common.md`(`LayoutDualTrackStationLoop`)。
- **`LayoutRoute`(多机场，不再用`LayoutDualTrackStationLoop`)**：接口按`stationIndex`分组，每座机场接口按`[in0,out0,in1,out1,...]`成对排列，一对=一条跑道，朝向=in接口出发方向(目前只用前两条，没有"仅起飞/仅降落"标记，所有跑道视为可起降；专用跑道要给站点接口加可选用途字段，选跑道时过滤)。边缘站点全部接在真实站点之后，`edgeStations`按追加顺序编号。
  - **天空节点**：每座机场先给每个接口算好天空节点：out接口沿跑道方向**前方**、in接口沿跑道方向**后方**各`200`单位(=巡航高度`100`/爬升斜率`0.5`，即水平每出去20抬升10)，`z=100`，到达/出发朝向=跑道方向。接口<->天空节点、天空节点<->天空节点都是Core单段三次贝塞尔：两端水平切线(控制点`z`取端点`z`)，沿切线放控制点，臂长由`SetProperty`里的`controlRules`按端点站名(`_sky`/`_mid`/`@real`)定：天空节点<->天空节点两点水平距离的`1/3`；in/out<->跑道中点不加控制点，直线连接；in/out U形转场(两端都是真实站点)`1`单位；天空节点<->in/out(及连外界的extern)`16`单位(160m)；同一机场出/进extern之间直线，默认`controlLength=16`(所以接口到天空节点是一条S形爬升线)。
  - **连外界**：每个天空节点另建一个extern(站名`<机场>_ext_xxx`)，不把天空节点直接当extern——天空节点可能只是机场互联线路的节点，互联不算出城。天空节点在地图范围内：extern在沿跑道方向水平延长到地图边界处(`RayBoxIntersection`)；天空节点已在图外：extern在天空节点原位(沿飞行方向错开`0.01`单位避免零长度link)。link：`skyOut->extOut`、`extIn->skyIn`、同一座机场out侧extern到in侧extern(`extOut->extIn`)，这几条都是直线(`controlRules`里`_ext/_ext`、`_sky/_ext`、`_ext/_sky`，都要求`sameGroup`)。**extern处没有停靠站点**：飞机到out侧extern后直接直线飞到同一机场的in侧extern，再回城市；`leftOut/leftIn`(跑道2出、跑道1进)一对，`rightOut/rightIn`(跑道1出、跑道2进)一对。每座机场自成一圈环线：`skyIn0->in0->mid0->out0->skyOut0->rightExtOut->rightExtIn->skyIn1->in1->mid1->out1->skyOut1->leftExtOut->leftExtIn`。
  - **机场互联**(是否超出地图边界不影响)：每对机场(A,B)，A用朝向最接近A->B的跑道起飞、B用朝向最接近来向的跑道降落，`A.skyOut->B.skyIn`一条贝塞尔，B->A同理各自重选跑道；一对机场一条环线、一架飞机，不考虑调度/避让。每座机场停靠段：`skyIn(降落)->in->mid(停)->out`，降落跑道和起飞跑道不同时再加"滑行link"`out(降落跑道)->in(起飞跑道)`(同端头U形，每座机场每对跑道只加一次)，然后`in->mid(停)->out->skyOut(起飞)`。
  - **滑行降速**：`停靠点1->out1->in2->停靠点2`这一个行驶区段含同站转场腿(`from==to`的同一Station)，Core整段按`sameStationSpeedScale=0.1`降速，不只是中间那条link，见`route_mod.md`。
  - 天空节点<->天空节点的控制臂`32`单位，互联线路的`skyOut`出发朝向是跑道方向、`skyIn`到达朝向是降落跑道方向，机场对不在跑道正前方时转弯半径随两机场距离缩放；要更平滑改`controlRules`第一条的比例。
- **`tiltAlongPath`**：飞机俯仰角跟着路径切线的竖直分量走。
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

- 互联线路只用前两条跑道的中点停靠点；多于两条跑道、专用起飞/降落跑道未实现。
- 天空节点间距/高度/滑行倍率(0.1)是估值，待进游戏调。
