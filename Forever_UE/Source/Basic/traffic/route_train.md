# route_train.h / route_train.cpp

## 职责

`TrainRoute`(`route_train`)：route域的默认实现之一。`instantiate=true`，在地图上真正铺轨（桥/隧道/拆迁）。

## 关键设计

- **模型**：每站两条轨道 t=0/1(航向相反)，每条轨道 t 有 `inlet t`、`outlet t`、`stop t`(同下标)，由 `station_train` 声明(`station_train.md`)。`LayoutRoute` 只输出站间 `links`、`externs`、`lines`，**不再有 rise/mid 之类的节点**，也没有控制臂规则/`controlRules`。
- **站间铁轨 = 中轴线 + 两侧偏移**(`kTrackGauge=0.5`，半轨距 0.25)：
  - **车站的"端 e"**=轨道 e 的出站口 `Out(e)` 与轨道 1-e 的进站口 `In(1-e)`(同一侧，相距=楼体进深×0.1，不是定值)。每端在车站外沿航向直线延伸 `kStationExtend=2` 取**轴点 P**=(Out+In)/2+h×2；出站轨连接点 O'=P+右手侧×0.25，进站轨连接点 I'=P−右手侧×0.25(右手侧=(−hy,hx)，+Y 向南；`station_train` 保证出站轨在右手边)。
  - **站端连接(每个车站每端一份，`ensureEnd`)**：`Outlet(e)→Extern <站>_e<e>_out`(Out→O')、`Extern <站>_e<e>_in→Inlet(1-e)`(I'→In)，各一段 Bezier，两端臂长=弦长/3、方向都沿航向，扁 S 形，`terrainFollow=false`。后面不管这一端连出几条轨道都共用这份。extern 只是 Core 图里不停靠、不限速的过路节点(Outlet→Extern→Extern→Inlet 本来就是合法组合)。
  - **站间(`connectEnds`，每对车站端一份)**：中轴线 P_A→P_B 一段 Bezier(臂=两点距离/3)，按弧长 `kOffsetStep=4` 采样，两条铁轨(A→B 偏右、B→A 偏左，倒着走)**共用同一组采样点**偏移，所以永远平行(harness：轨距 0.5，不交叉)；相邻采样点之间 Hermite 三次贝塞尔(`OffsetRail`)，`terrainFollow=true`。links：`Extern A_e_out→Extern B_e_in`、`Extern B_e_out→Extern A_e_in`，两个方向一起生成(环线两方向、快线往返本来就成对使用)。
  - 桥高就是两端口的 z(火车站端口 z=1.5，3 楼地面)，**没有站内抬升/下降段**。每段都带 `mesh=kTrainBridgeMesh`、`meshUnit=kTrainBridgeUnit`(0.25，定义在 `route_train.cpp`，站外轨道用带底座的 `TrackBridge`，站内仍用 `station_train.h` 的 `kTrainTrackMesh`；`meshUnit` 必须等于模型沿 X 的实际长度)。同一对车站端只生成一次(`pairKeys` 去重)。
- **线路拓扑：1 座中央车站 + 东南西北最多各 1 座**(车站由 `building_train_station` 的 `Assign` 放，某方向没有合适位置就不放，见其 md)。`LayoutRoute` 里中央车站=到其它站距离之和最小的站，其它站按相对中央的主方向分到东/南/西/北(每个方向取离中央最近的一座)。
  - **连线几何**：每个外围站朝向中央的那一端 ↔ 中央车站的一端(`connectEnds`)，外围站远离中央的那一端 ↔ 对应方向的地图边界 extern(`boundaryExtern`，半圆掉头)。
  - **中央车站的端**：穿过中央是一端进、另一端出，所以一条线上的 A、B 两个外站必须接在中央的相反两端(`pickHubEnd`：航向和方向的点积之和最大选)；单站朝向哪一端用 `hubEndFacing`(点积大的端)。每条线都是 `throughHub`：`A边界→A站→中央→B站→B边界→B站→中央→A站→A边界`(首尾相接)，某一侧没有外站就由中央对应端直接连到指定边界。
  - **没有车站的方向不再连到该方向的边界**，按外站数量 n 分情形(每条线 4 辆车)：
    - n=4：北-南线、东-西线，各一条。
    - n=3(缺方向 m)：缺的方向的对向站 o；另一轴的 p、q 照常成线(p-中央-q)；原来经过缺失方向的线改成 o-中央-X，X 是 p、q 里接在中央**另一端**(不是 o 所在端)的那座，X 远端的边界回转点与 p-q 线共用。共 2 条线。
    - n=2 对向(只有南北或只有东西)：只建这一条线，另一条轴线不建。
    - n=2 一座在东西轴、一座在南北轴：两座在中央两端 → 一条线连三站和两侧边界(各接自己方向的边界)；在同一侧 → 看中央车站中轴线偏南北还是偏东西(`dominantLabel`)，中央空闲端连到对应方向(南/北或东/西)的边界，两座外站各一条线(共用同一个空闲端回转点)。
    - n=1：外站-中央，外站远端连自己方向的边界，中央空闲端连到**对向**边界，一条线。
    - n=0(只有中央车站，含 `Assign` 退化出的单站)：按中轴线偏南北/偏东西，两端各连一条对向边界(`boundaryExtern(hub, e, dominantLabel(航向))`)，一条线 `hub(1)→bd1→hub(0)→bd0`。
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

`Route::Build` 在 `LayoutRoute` 之后调用一次。**只按高度判定，与地形类型无关**(没有 `getTerrain`/坡度/海的判断)。1 单位=10m：铁轨平时在 z=1.5(15m)，隧道内 z=-1.5(-15m)。

- **配对**：`LayoutRoute` 每生成一对站间铁轨(`connectEnds`、`boundaryExtern`)，就把两条 link 的下标和它们共用的**中轴线**(按 1 单位弧长采样的 `CurveSample`)存进私有成员 `railPairs`；`BuildTrack` 按 pair 逐对处理，不在铁轨上反推中轴线。不在 pair 里的 link(站端连接、半圆)原样保留。
- **判定(沿中轴线，每对只判一次)**：高度 `getHeight` ≥ 阈值(`TUNNEL_THRESHOLD_Z`=1.5)的连续采样点是一座"山"，第一个/最后一个点是阈值点 sIn/sOut。间隙(低于阈值的地面段)弧长 < `TUNNEL_MIN_OPEN_LENGTH`(32) 的两座山合并成一座(两座山之间，不出地面，一直在隧道里；32 正好是两侧各 16 的坡道)；第一座山之前/最后一座山之后的段连着车站(链首/链尾)，不合并。
- **隧道 vs 爬升**：入口阈值点距链首(车站端中轴线起点 P)< `TUNNEL_RAMP_LENGTH`(16) 或出口阈值点距链尾 < 16 → 坡道放不下，这座山**沿山体爬升**(`z=max(地形高度, 基准)`，`AddTerrainFit` 让地形贴合轨道，两轨各自提交，宽 `trackWidth`)；否则钻隧道：阈值点前 16 单位内 `SmoothStep` 从 1.5 降到 -1.5，山内保持 -1.5，出山后 16 单位升回(`TUNNEL_DROP`=-3 相对基准)。
- **开洞(中轴线统一)**：只在两个隧道口各请求靠山的 `TUNNEL_HATCH_LENGTH`(8) 单位，沿中轴线每 1 单位一小段(z 取下沉剖面，洞底高度正确)提交 `AddTunnelHatch`，宽 = 2×`kTrackGauge` = 1.0(两条铁轨合起来)。两条铁轨自己**不再**提交 `AddTunnelHatch`。`AddTunnelHatch` 产出的是 `wall=true` 的 hatch，Core(`Map::AddRouteHatch`)会把它记进 roadnet 的 hatches，UE 侧 `BuildTunnelWalls` 据此在开口两条长边做竖直侧壁(和路网隧道口一样)。
- **两轨同步**：两条铁轨的下沉/爬升剖面都由中轴线上的 zone 按弧长比例换算(右轨 A→B 按比例，左轨倒着走取 1-比例)，同进同出；每条铁轨自己的采样点自己生成几何(同一条 link 里连续的 `terrainFollow` 段合并成一条链整体处理，只有链首/链尾各 `TRACK_STATION_FLAT_SAMPLES` 个采样点保持水平)。
- **几何**：平面控制点用 Catmull-Rom 切线(相邻段共用切线、航向连续，控制臂=弦长/3)，Z 用端点坡度 Hermite 并夹到单调范围；完全水平共线的连续采样合并成一段；每段 `mesh`/`meshUnit` 由原段复制(**隧道段目前仍用同一个轨道 mesh**)。
- **桥墩**：按**中轴线**画，两条铁轨共用一排墩(不是每根铁轨各一排)。`BuildTrack` 末尾沿中轴线每 `TRACK_SAMPLE_STEP`(1) 单位一段造 `Connection`，z=桥面高度(基准线性过渡 + 隧道下沉 + 抬升段 `max(地形,基准)` + 链首尾水平，和铁轨同一套剖面)，一条链，`AddPier("", chains, PIER_DEFAULT_SPACING=4, PIER_MIN_SPACING=2, PIER_RADIUS=0.15)` 提交(资产留空=默认圆柱，墩距 40m/最短 20m/半径 1.5m 是估值)。选位和贴地/隧道内不放墩全在 Core，见 `Core/traffic/route.md`"桥墩"。
- **拆除**：每条 link 的采样折线+`trackWidth` 提交 `AddDemolition`，Core 拆相交的楼体/园区(带 `stationMod` 的不拆)，见 `map.md`。

## 依赖关系

- 依赖：`Dependence/traffic/route_mod.h`、`station_train.h`(mesh 常量)、`route_common.h`(`MakeBezierSegment`)。
- 被谁依赖：`Basic/basic.cpp` 注册进 `config.json` 的 `"route_mods"`；`Core/traffic/route.cpp`。

## 手动线路 `--skip`（2026-11-23）

`"route_train --skip"`：`LayoutRoute` 走 `LayoutManual`，`BuildTrack` 走 `BuildManualTrack`；数据与规划见 `route_mod.md`"手动线路"、`route_common.md`。

- **铁轨**：每条用到的连线 = 中轴线（`ManualAxis`）+ 两侧各偏半个轨距（0.25）的两条铁轨，沿中轴线 `kOffsetStep=4` 弧长采样、`OffsetRail` 偏移（右轨 A→B 偏 +0.25 右手边，左轨 B→A 偏 -0.25 且倒着走），z 用 smoothstep（`ManualRail`，两端=节点高度）。中轴线 z 剖面和编辑器里画的一致。
- **extern**：每个 (节点,连线,到达/离开) 的铁轨端点一个 extern，**按位置去重**（同节点内相差 <0.02 视为同一个）——直线通过的节点，到达端和离开端正好重合，不需要转向 link；有转角的节点补一段转向 link（`MakeBezierSegment`，臂=弦长/3，转角>60° 打警告）；掉头（到达==离开同一条连线）用以两端中点为圆心的半圆（两个 1/4 圆弧，臂 0.5523·半径）。
- **缺口**（编辑器虚线）：站端口/节点端口/Hub 之间用一段扁 S（`MakeBezierSegment`，两端取端口朝向，臂=弦长/3）；弦与端口朝向夹角>60° 打警告（让最近的节点和站的轨道对齐）。**火车站两侧都要有节点**，否则要大回头。
- **线路**：停靠站 `{站名, 轨道下标}`；节点项 `{extern名, -1}`；每条线 `kVehiclesPerTrainLine` 辆；少于 2 个条目丢弃并警告。
- **BuildManualTrack**（按连线类型，显式不做自动判定）：`ground` = 只拆迁（中轴线条带，宽 `trackWidth`），**不改地形**（和路网平地一致，轨面取节点高度，节点高度和地形差多了会悬空/埋地）；`hug` = 铁轨 z 重铺成 `地形高度 + 两端偏差线性过渡`（端点仍是节点高度，接缝连续）+ 拆迁 + 贴合；`mouth` = 拆迁 + 每段 `AddTunnelHatch`（宽 `2*kTrackGauge`，端墙只加在相邻节点连着 `tunnel` 连线的那端，墙底 = 该端路面 + `TUNNEL_ROOF_CLEARANCE`）；`tunnel` = 什么都不做（在山体里）；`bridge` = 拆迁 + 沿中轴线 `AddPier` 一排墩（两轨共用）。缺口 link 由 mod 自己决定，火车 route 一律按桥处理（拆迁 + 沿线 `AddPier` 放桥墩，桥面高度=站端口 z 到节点 z 的扁 S；墩由 Core 选位，地面以下/贴地的位置不放）；转向 link 不处理。
- **掉头节点**（`Hub`+`HubOut`）：节点处到达端/离开端 extern 靠右错开半个轨距，之间一个半圆（和铁轨掉头共用 `addUTurn`），缺口连线分别接到它们上面。
- 没数据 / `manual` 为空：`LayoutManual` 只警告，不出线路。
- 自动布局的 `railPairs` 在手动模式下为空，另有 `manualPairs`/`manualGapLinks`。
