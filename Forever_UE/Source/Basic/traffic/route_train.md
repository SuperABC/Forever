# route_train.h / route_train.cpp

## 职责

`TrainRoute`(`route_train`)：route域的默认实现之一。`instantiate=true`，在地图上真正铺轨（桥/隧道/拆迁）。

## 关键设计

- **模型**：每站两条轨道 t=0/1(航向相反)，每条轨道 t 有 `inlet t`、`outlet t`、`stop t`(同下标)，由 `station_train` 声明(`station_train.md`)。`LayoutRoute` 只输出站间 `links`、`externs`、`lines`，**不再有 rise/mid 之类的节点**，也没有控制臂规则/`controlRules`。
- **站间铁轨 = 中轴线 + 两侧偏移**(`kTrackGauge=0.5`，半轨距 0.25)：
  - **车站的"端 e"**=轨道 e 的出站口 `Out(e)` 与轨道 1-e 的进站口 `In(1-e)`(同一侧，相距=楼体进深×0.1，不是定值)。每端在车站外沿航向直线延伸 `kStationExtend=2` 取**轴点 P**=(Out+In)/2+h×2；出站轨连接点 O'=P+右手侧×0.25，进站轨连接点 I'=P−右手侧×0.25(右手侧=(−hy,hx)，+Y 向南；`station_train` 保证出站轨在右手边)。
  - **站端连接(每个车站每端一份，`ensureEnd`)**：`Outlet(e)→Extern <站>_e<e>_out`(Out→O')、`Extern <站>_e<e>_in→Inlet(1-e)`(I'→In)，各一段 Bezier，两端臂长=弦长/3、方向都沿航向，扁 S 形，`terrainFollow=false`。后面不管这一端连出几条轨道都共用这份。extern 只是 Core 图里不停靠、不限速的过路节点(Outlet→Extern→Extern→Inlet 本来就是合法组合)。
  - **站间(`connectEnds`，每对车站端一份)**：中轴线 P_A→P_B 一段 Bezier(臂=两点距离/3)，按弧长 `kOffsetStep=4` 采样，两条铁轨(A→B 偏右、B→A 偏左，倒着走)**共用同一组采样点**偏移，所以永远平行(harness：轨距 0.5，不交叉)；相邻采样点之间 Hermite 三次贝塞尔(`OffsetRail`)，`terrainFollow=true`。links：`Extern A_e_out→Extern B_e_in`、`Extern B_e_out→Extern A_e_in`，两个方向一起生成(环线两方向、快线往返本来就成对使用)。
  - 桥高就是两端口的 z(火车站端口 z=1.5，3 楼地面)，**没有站内抬升/下降段**。每段都带 `mesh=kTrainTrackMesh`、`meshUnit=kTrainTrackUnit`(0.25)。同一对车站端只生成一次(`pairKeys` 去重)。
- **线路拓扑：1 座中央车站 + 东南西北各 1 座**(车站由 `building_train_station` 的 `Assign` 放，见其 md)。`LayoutRoute` 里中央车站=到其它站距离之和最小的站，其它站按相对中央的主方向分到东/南/西/北(每个方向取离中央最近的一座)。
  - **连线几何**：每个外围站朝向中央的那一端 ↔ 中央车站的一端(`connectEnds`)，外围站远离中央的那一端 ↔ 对应方向的地图边界 extern(`boundaryExtern`，半圆掉头)。
  - **线路(2 条)**：北-南线 `北边界→北站→中央→南站→南边界→南站→中央→北站→北边界`(首尾相接)，东-西线同理。每条线路 4 辆车。
  - **中央车站的端**：穿过中央是一端进、另一端出，所以一条线上的 A、B 两个外站必须接在中央的相反两端；中央每端因此会同时连出两个方向(例如东端接东站和北站)，共用同一份站端连接。分配用航向和方向的点积之和最大选。
  - **缺方向**：某个方向没有外站时，中央车站对应的那一端直接连到该方向的地图边界 extern(`boundaryExtern`，label=该方向)，线路照常成环；南北/东西两个方向都缺时那条线就是中央车站两端各连一个边界 extern。
  - **只有 1 座站**(中央车站放不了时 `Assign` 退化出来的单站；若中央在但四个方向全缺，route 里看起来也一样，同样按单站处理)：两端各沿自己的航向**直线**射到地图边界外(`boundaryExtern` 的 straight 模式，extern 名 `bd_<站>_<端>_*`)，在边界半圆掉头，一条线。
  - **边界回转(`boundaryExtern`)**：同样是中轴线方案。设 D=轴点 P 到该边界的垂直距离：从端实际连出来的航向向前延伸 D/3 得到控制点 C，**C 到边界的垂足**就是 extern 的位置(沿边界方向夹在地图内)，轨道从航向平滑拐向边界。中轴线 P→(垂足往里缩半轨距)，控制臂=D/3；去程铁轨(右手侧)走到终点，**半径=半轨距(0.25)的半圆掉头**(两段 1/4 圆弧，臂=0.5523×半径)，回程铁轨(左手侧)沿同一组采样点回到 P。半圆顶点正好在边界上，就是 extern `bd_<站>_<轨道>_<方位>`(线路里串入的必经点)。links：`Extern <站>_e_out→bd`(去程铁轨+前半圆)、`bd→Extern <站>_e_in`(后半圆+回程铁轨)。原来的 90° 拐角没有了(harness 接缝航向突变>20° 的数量 0)，代价是地图边缘的急掉头(半径 2.5m)。extern 不停靠、不触发事件、不限速；原来的 600 秒静默停靠取消。
- **车辆数**：每条火车线路 4 辆(`LayoutRoute` 给每条线路往 `lineVehicleCounts` 填 4)。
- **速度**(`SetProperty`，地图单位/游戏秒；1 真实秒=120 游戏秒)：`normalSpeed=0.1`、`sameStationSpeed=0.1`(火车不用)、`passSpeed=0.05`、`dwellSeconds=600`、`easeSeconds=10`(真实秒，从 0 加速到 `normalSpeed` 的时间，Core 据此得固定加速度，见 `route.md`)。`trackWidth=0.8`。

  | 类型 | 环线长度(地图单位) | speed | dwellSeconds | easeSeconds |
  |---|---|---|---|---|
  | 公交 | 约120~180 | normal 0.025 | 360 | 3 |
  | 火车 | 约2000 | normal 0.1 / pass 0.05 | 600 | 10 |
  | 飞机 | 约2050 | normal 0.175 / sameStation 0.0175 / pass 0.0875 | 720 | 6 |

  固定加速度的曲线比原来"时间保持的梯形"更慢，`easeSeconds` 已为此降低，需 PIE 观感调。

## BuildTrack（过山爬坡/隧道）

`Route::Build` 在 `LayoutRoute` 之后调用一次。只对 `terrainFollow` 的段处理，其余段(站端连接、半圆弧)原样保留。**同一条 link 里连续的 `terrainFollow` 段合并成一条链整体处理**(偏移铁轨是很多个 4 单位的小段，必须当一整条轨道，不然每个小段两头都会被压平、山和隧道会被切碎)：
1. 把整条链的平面曲线按弧长每 `TRACK_SAMPLE_STEP`(1 单位=10m)采样，记录地形类型和 `getHeight`；
2. 连续的 `terrain_mountain` 切成一座座山，规则照抄 `JingRoadnet::buildArm`（常量 `MOUNTAIN_MAX_SLOPE`/`TUNNEL_HEIGHT`/`TUNNEL_HATCH_LENGTH`/`TERRAIN_FIT_FALLOFF` 在 `route_*.cpp` 里重新定义，两边数值要一致）：坡度<=0.5 或山后是海→抬升(`AddTerrainFit`)；否则→隧道（前后各 8 单位 S 形坡道，`AddTunnelHatch` 挖洞，中间水平隧道段），放不下坡道则退化为抬升；整条链的链首/链尾各 `TRACK_STATION_FLAT_SAMPLES` 个采样点保持水平；两条铁轨各自独立处理，山脚地形判定可能差一个采样点，轨距 0.5 远小于采样步长，接受；
3. 每相邻两点输出一段三次贝塞尔，完全水平共线的合并。**平面控制点用 Catmull-Rom 切线**(每个采样点的切线=前后相邻采样点连线方向，两端取基础曲线的出发/到达方向，控制臂=该段弦长/3)，相邻段在接缝处共用同一条切线、航向连续；以前控制点取弦的三等分点，每 1 单位一段，接缝处航向会突变几度(火车约每秒一次)；每段 `mesh`/`meshUnit` 由原段复制（**隧道段目前仍用同一个轨道 mesh**，没有隧道标志，将来要换资产只需在这里改 `mesh`）。
- **拆除**：每条 link 的采样折线+`trackWidth` 提交 `AddDemolition`，Core 拆相交的楼体/园区(带 `stationMod` 的不拆)，见 `map.md`。

## 依赖关系

- 依赖：`Dependence/traffic/route_mod.h`、`station_train.h`(mesh 常量)、`route_common.h`(`MakeBezierSegment`)。
- 被谁依赖：`Basic/basic.cpp` 注册进 `config.json` 的 `"route_mods"`；`Core/traffic/route.cpp`。

## 待办/后续阶段

- 轨道与轨道、轨道与机场天空线交叉不处理。
- 无头 harness 验证过(随机地图 4 线路/连续/靠右/导航无悬空/速度表合法)；**未在 PIE 验证**：轨道/桥/拆迁视觉、实际行驶/停靠事件、读档重建、`passSpeed` 穿站路径(默认线路每站都停，没用到 Through 连线)。
- 隧道内暂无专用轨道资产。
