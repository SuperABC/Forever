# route.h / route.cpp

## 职责

`Route`：一类线路的实体，独占持有一个 `RouteMod` 实例（照抄 `Vehicle`/`Job` 持有 mod 的模式）。流程：

1. `Build()` 收集 `stationType` 匹配的 `Station`，把端口快照交给 `mod->LayoutRoute()` 得到 `externs`/`links`/`lines`（`route_mod.md`）；instantiate 线路再调 `BuildTrack`；
2. `Assemble()` 建**寻路图**，按每条线路的停靠顺序寻路，**预算好**每条线路的运动学（`RouteLine`）；不可达的线路写日志后丢弃；
3. `Update()` 只读预算好的 `RouteLine` 驱动载具，运行时不再碰寻路图。

Core 对站/线的类型完全不关心，也不计算任何连线几何：几何全来自站点 mod（站内连线）和路线 mod（站间连线）。

## 两种线路

- **非路网线路（火车/飞机）**：几何来自 mod，走下面的寻路图。
- **路网线路（`useRoadnet`，公交）**：mod 只给停靠顺序（每站 stop 0）。每个 run 用 `Station::EnsureRoadLink` + `Map::FindVehiclePath`（含**同车道特例**：两站在同一段原车道上且起点在上游时，直接用两个 `stationNode` 间的直线弦，不绕一圈路径搜索）拼 `TrackPath`，借用路网 `Connection`（`owned=false`）。不动态改路网。不建 Core 寻路图。

## 数据结构

- **`TrackPath`**：一条路径=按行驶顺序首尾相接的 `Connection` 链 + 每段长度 + 总长；`segments` 是 mod 给的几何值（存档/UE 铺轨用，借用路网的链为空）；`owned` 标记 `connections` 是不是自己 `new` 的（析构负责 `delete`）。站内连线、站间连线、公交借用的路网车道都是它，`shared_ptr` 共享。`Evaluate(arc)` 按弧长取点/偏航/俯仰。
- **寻路图（`graphNodes`/`graphEdges`，每个 Route 一份，建完保留）**：节点=站点端口 `(station,kind,index)` 或 extern（按下标）；边=站内连线（`EDGE_STATION_IN` 进站口→停靠位、`EDGE_STATION_OUT` 停靠位→出站口、`EDGE_THROUGH` 进站口→出站口直通）+ 站间连线（`EDGE_LINK`，带 `withinStation`）；边权=长度。保留图是为将来可能的动态增加线路，线路数据本身不依赖它。`trackPaths` 是图里全部路径（已去重，UE 铺轨/画图用）。
- **`RouteLine`**（每条线路一份，独立于图）：`runs` + `period`（一圈总时长，含每站 `dwellSeconds`）。`RouteRun`：起点/终点 `Station*`+停靠位下标、`pieces`（`RoutePiece{path,length,speedCap}`）、总长、`phases`（运动学相位表）、`travelTime`。

## 规划（`PlanGraphLine`）

线路里的每个 entry 是一个**必经点**：站 entry 固定为该站第 `stopIndex` 个停靠位节点（停靠），extern entry 固定为该 extern 节点（不停）。线路按停靠 entry 切成 run；相邻 entry 之间用 Dijkstra 求最短路（含首尾相接）。**除起点/终点外不能经过任何停靠位节点**，所以过站不停只能走该站的站内直通连线。整圈只有一个停靠位（起点=终点）时必须真的离开再回来。站名/停靠位下标/extern 名不存在或不可达时 `debugf` 说明原因，整条线路丢弃（"静默丢弃"：只写日志，不影响其它线路）。

## 速度与运动学

- **速度分类**：run 起止同站，且途经的站间连线全是 `withinStation` → 整段上限 `sameStationSpeed`；否则 `normalSpeed`，且穿过中间站的 `EDGE_THROUGH` 边上限降到 `min(base, passSpeed)`。extern 不限速、不停（run 内的途经点）。
- **预算（`ComputeRunPhases`）**：加速度 `a = normalSpeed / (easeSeconds*120)`（游戏秒）。run 两端速度 0；相邻 piece 的边界速度取两侧上限较小者，再按 `v²≤v0²+2aL` 前向/后向收紧，每个 piece 内得到 加速→匀速→减速 相位（`RunPhase{t0,s0,v0,acc,duration}`，`s=s0+v0·dt+½a·dt²`）。run 短到来不及加速到上限时自动变成三角形速度曲线（峰值低于上限，这时一圈时间会比 `长度/限速` 长，调 `easeSeconds` 控制）。
- **求值**：`RunArcAtTime`（二分取相位）→ 按 piece 累计长度定位 → `TrackPath::Evaluate`。反向 `RunTimeAtArc`（解二次方程）供 `SyncVehicleProgress` 把实车的路程进度换回时刻表时间。

## Update / 停站事件

- `Update(elapsedSeconds)` 是纯函数式：每辆车 `localTime = (elapsedSeconds + 均匀相位 + phaseAdjust) mod period`，逐 run 累减"行驶耗时+停靠耗时"，落在行驶区间用 `EvaluateRun`，落在停靠区间停在该 run 终点（`SetPitch(0)`），停靠事件报 `run.toStation`。俯仰角只在行驶时、instantiate 或 `tiltAlongPath` 的线路才非零。
- 停站/离站边沿检测：和 `lastStopStation` 比较，在→不在=leaving，不在→在=entering，A→B=先 A leaving 再 B entering；`ConsumeStopEvents()` 取走；读档后 `SilenceInitialStopEvents()` 让第一次 Update 只记状态不发事件。Core 不碰 script，由 `AForeverFrameworkActor::Tick` 转发为 `enter_stop`/`leave_stop`。
- **每条线路的车辆数**：`RouteLine::vehicleCount`(`Assemble` 从 `mod->lineVehicleCounts[线路下标]` 填，没给就是 0)；`GetLineVehicleCount(lineIdx)` 取它，<=0 按 1 辆。`Traffic::InitTransitVehicles` 按它给每条线路创建车。不存档(读档时车辆按名字恢复)。
- `Vehicle::IsPhysicsDriven()` 为真时 `Update` 跳过该车，时刻表相位照常累计。

## 物理接管支持

- `QueryVehicleState`：线路下标、`legIndex`（run 下标）、是否停靠、剩余停靠（真实秒）、巡航速度（`normalSpeed*120`，缓动段不精确，仅供给物理车初速度）。
- `GetLineStopNodeIds(line)`：每个 run 起点站的 `stationNode` id（Map 辅助导航节点），顺序即行驶顺序；任何一站没有 road link 返回空。
- `ResetStopState(vehicle)`：物理段交还时刻表时忘掉停靠状态。
- `SyncVehicleProgress(vehicle, legIndex, legFraction, dwelling, dwellFraction)`：给这辆车记一个运行时相位修正量 `phaseAdjust`（游戏秒，不存档），使"现在"对应的时刻表时间等于实车的路程进度（`legFraction` 是路程比例，经 `RunTimeAtArc` 换成时间）。UE 侧在 `BusToKinematic` 里、`ClearCommands` 之前用 `QueryLoopProgress` 取进度调用。

## 实例化线路（火车铺轨）

`mod->instantiate && !useRoadnet` 的线路，`Build()` 在 `LayoutRoute` 之后、建图之前调 `mod->BuildTrack()`（地形回调绑 `Map::GetTerrainRef/GetWater/SampleHeight`），把 `hatches`/`terrainFits` 转发给 `Map::AddHatch`/`ApplyTerrainFits`（烘焙进地形，所以 `Traffic` 必须在地形网格生成之前构建），`demolitions` 交给 `Map::DemolishAlongStrips`（`keep` = 建筑有 `stationMod`），见 `map.md`"拆除"。这些发生在存档之前，读档不重跑——读档用存档里的最终 `links` 几何走 `Assemble()`。

### 桥墩（`PlacePiers`）

选位算法和`Pier`结构已抽到`Core/map/pier.{h,cpp}`（公路桥共用，见`map/pier.md`），下面是火车这边的用法。

mod 在 `BuildTrack` 里用 `AddPier(mesh, chains, defaultSpacing, minSpacing, radius)` 提交 `PierRequest`(链=一根轨道的 `Connection` 序列，按值存)，`Traffic::InitRoutes` 在每条 `Route::Build` 之后调 `Route::PlacePiers(map, 已建好的 route 列表)` 选位，结果存进 `Route::piers`(`Pier{x,y,topZ,radius,mesh}`，随 traffic 存档，读档不重跑)。
- **选位**：每条链链首视为已有支撑，`s` 从 0 起；剩余长度 ≤ 默认墩距时收尾。每根墩先试默认墩距，会碰到障碍就每次缩短 `PIER_SPACING_STEP`(0.5=5m)直到最短墩距；都不行再从默认墩距每次加长 0.5 试，直到超出链尾(找不到就停止这条链)。
- **障碍**：路网每条路(半宽=`Road::GetTotalWidth()/2`)，加所有 `instantiate && !useRoadnet` 的 route 轨道(半宽=`trackWidth/2`，本 route 里中点离请求链不到 `trackWidth/2` 的 connection 排除——请求链是中轴线，两根铁轨就在它两侧，不排除会挡住每一根墩)。只看 xy(暂不考虑多层桥)，不判建筑，水面不影响。**限制**：后建的 route 避让先建的，先建的不会为后建的让位；本 route 之后新建的路网/建筑也不再检查。
- **放不放**：候选点 `topZ - SampleHeight < PIER_MIN_HEIGHT`(0.05=0.5m，贴地/隧道内/过山抬升段)不放墩，但 `s` 照样前进(此时不做障碍检测)。
- **墩底不存**：UE 画时取当时地形，所以地形贴合后高度变了也对得上；水面不影响，贴水底。

## 依赖关系

- 依赖：`Core/map/map.h`、`Core/traffic/station.h`、`Core/traffic/vehicle.h`、`traffic/route_mod.h`/`route_factory.h`。
- 被谁依赖：`Core/traffic/traffic.cpp`（`InitRoutes`/`InitTransitVehicles`/`Tick`）、`traffic_save.cpp`、`Forever/Framework/ForeverTrafficFrameworkComponent.cpp`（调试线/铺轨/公交物理接管）、`Forever/UI/MapWidget.cpp`。

## 待办/后续阶段

- "穿站不停"（`EDGE_THROUGH` 降到 `passSpeed`）目前内置线路都没用到（所有途经站都停），只在逻辑层实现，没有实测。
- 寻路图保留着但没有动态加线路的入口；路网线路不支持路网动态变化。
