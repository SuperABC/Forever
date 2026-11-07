# route_basic.h / route_basic.cpp

## 职责

三个内置`RouteMod`实现：`BusRoute`(`route_bus`，`stationType="bus"`、`useRoadnet=true`、`instantiate=false`，接入现有车道路网的环线分组)、`TrainRoute`(`route_train`，`stationType="train"`、`useRoadnet=false`走三次贝塞尔、`instantiate=true`沿线铺轨)、`AirRoute`(`route_air`，`stationType="plane"`、`instantiate=false`，边缘站点z取巡航高度`kCruiseHeight`=100地图单位，跑道接口/中点停靠点仍贴地)。后两者共用`LayoutDualTrackStationLoop`，拓扑相同。

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
  leftEdgeIn -> leftIn -(轨道1)-> stop1 -> rightOut -> rightEdgeOut -> rightEdgeMid -> rightEdgeIn -> rightIn -(轨道2)-> stop2 -> leftOut -> leftEdgeOut -> leftEdgeMid -> (回到leftEdgeIn)
  ```
  `interfaces`固定顺序`[leftIn, rightOut, rightIn, leftOut]`(`TrainStation`/
  `AirStation::Layout`保证，见`station_basic.md`)。`leftIn`/`rightOut`朝向相同，两点连线
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
- **`speed`/`dwellSeconds`的单位是游戏内秒，不是真实秒**：`Traffic::Tick`喂给`Route::Update`的`elapsedSeconds`来自`Player::Tick()`里`time->AddMilliseconds(delta*60*1000*timeFlowRatio)`，默认`timeFlowRatio=2.0`，即1真实秒=120游戏内秒。三个类型在`SetProperty`里按下表校准(基类默认值`speed=1`/`dwellSeconds=30`)；如果`time_flow_ratio`被剧情脚本改了，真实秒观感会等比例变化，要一起重新校准。背景见`route_mod.md`"speed/dwellSeconds的单位"一节。

  | 类型 | 环线长度(地图单位) | 目标 | speed | dwellSeconds | easeSeconds |
  |---|---|---|---|---|---|
  | 公交 | 约120~180 | 一圈40~60真实秒、停站约3真实秒 | 0.025(3单位/真实秒) | 360 | 20 |
  | 火车 | 约2000(大部分是站到地图边缘的长途) | 一圈约170真实秒、进站停约5真实秒 | 0.1(12单位/真实秒) | 600 | 40 |
  | 飞机 | 约2050 | 一圈约100真实秒、停约6真实秒 | 0.175(21单位/真实秒) | 720 | 50 |

- **`easeSeconds`(启停缓动)**：火车/飞机体型大，启动刹车更缓。`DriveVehicle()`会自动把加减速时长钳在单个行驶区段时长的一半以内，所以公交每站都停时区段退化成单条leg，站间距短的leg会变成全程加速+全程减速、没有匀速中段，这是"尽量缓"在站间距有限时的最大效果，直接给足够大的值即可。详见`route.md`"DriveVehicle的缓动"一节。
- **`LayoutDualTrackStationLoop`的边界节点**：左边界沿`leftIn`朝向的反方向射线出图得到(飞机/火车从图外来的方向)，右边界沿`rightOut`朝向射线出图；边界节点的到达/出发朝向直接复用相邻接口的朝向(同向的复用，掉头的取反)。`overrideEdgeZ`为true时边缘站点z取`edgeHeight`(AirRoute的巡航高度)，否则取接口原本的z。
- **`BusRoute`分组的推导**：一个点沿"绕形心转圈"的切线方向走时，它的朝向和"位置相对形心的半径向量"的2D叉积符号是固定的纯数学事实，与地图南北/Y轴朝向无关；因此无论Map层的左右手约定怎么变，都会按真实车道方向自动重新分组。

## 依赖关系

- 依赖：`Dependence/traffic/route_mod.h`。
- 被谁依赖：`Basic/basic.cpp`注册这三个类型进`config.json`的`"route_mods"`；
  `Core/traffic/traffic.cpp`的`InitRoutes`通过`RouteFactory`创建、调`Route::Build`
  (间接触发`LayoutRoute`)。

## TrainRoute::BuildTrack（过山爬坡/隧道）

`TrainRoute`是`instantiate=true`的线路：`trackMesh`=`SM_ufekaevdw_tier_3`（沿X轴延伸，轴心在起点边，长约250cm，
`trackUnit=0.25`），`trackWidth=0.8`。`BuildTrack`对每条link：
1. 把Core单段贝塞尔的平面曲线按弧长每`TRACK_SAMPLE_STEP`(1单位=10m)采样，记录每点地形类型和`getHeight`高度；
2. 把连续的`terrain_mountain`切成一座座山，规则照抄`JingRoadnet::buildArm`（常量`MOUNTAIN_MAX_SLOPE`/
   `TUNNEL_HEIGHT`/`TUNNEL_HATCH_LENGTH`/`TERRAIN_FIT_FALLOFF`在`route_basic.cpp`里重新定义，两边数值要保持
   一致，改动时一起看）：最大坡度<=0.5或山后是海→抬升（高度取山表面，请求`AddTerrainFit`）；否则→隧道（前后
   各8单位的S形坡道，坡道段`AddTunnelHatch`挖带侧壁的洞，中间水平隧道段`tunnel=true`），前后平地放不下
   坡道则退化为抬升；车站两端`TRACK_STATION_FLAT_SAMPLES`个采样点保持水平；
3. 每相邻两点输出一段三次贝塞尔（坡度夹到单调范围，同roadnet），完全水平且共线的连续点合并成一段。

## 待办/后续阶段

- `TrainRoute`/`AirRoute`假设全图只有一个真实站点(`LayoutDualTrackStationLoop`直接用
  `interfaces[0..3]`，不做分组)，多个同类型站点同时存在的情况不支持，真需要时要把
  `BusRoute`那套"不依赖假设、从真实数据反推"的思路搬过来用在"多个站点怎么分组连线"这一层。
- `TrainRoute`的隧道内轨道暂时复用`trackMesh`（没有专用资产），见`route_mod.md`。
