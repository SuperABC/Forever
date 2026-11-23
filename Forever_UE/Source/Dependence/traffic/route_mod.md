# route_mod.h / route_mod.cpp

## 职责

`RouteMod`：一类线路（公交/火车/飞机各一个具体子类）的连接逻辑。Core 把 `stationType` 匹配的所有站点（位置+进站口/出站口/停靠位）交给它，它只回答两件事：

1. **站间连线的几何**：哪些端点之间怎么连、每条连线由哪些带路线资产的贝塞尔段组成；
2. **每条运行线路的停靠顺序**：按顺序经过哪些站的哪个停靠位、哪些 extern 点。

两个站之间实际怎么走由 Core 在寻路图里寻路完成（`route.md`）。Core 完全不关心站/线是什么类型，也不做任何连线计算。

## 数据流

```
Station mod(站内：进站口/出站口/停靠位/站内连线)  ──┐
                                                    ├─> LayoutRoute(stations) ──> externs / links / lines
Core: RouteStationInfo(站点位置+世界坐标端口)  ───┘
```

- **`RouteStationInfo`**（Core→mod，只读快照，世界坐标）：`name`、`stationIndex`（站点在 Core 侧列表的下标，`RouteEndpoint::station` 引用它）、站点中心`x/y`、`inlets/outlets/stops`（`RoutePort{x,y,z,dirX,dirY}`，下标就是站内端口下标）、`roadName`（贴路站点所在道路的名字，其余为空；棋盘格里同名=同一条横/竖线，公交按它分组成线）。
- **输出三个 `vector`**（和 `BuildingMod::floors` 同一个跨 DLL 安全模式：mod 往自己持有的容器里追加，Core 只读）：
  - `externs`：`RouteExternNode{name,x,y,z,dirX,dirY}`，**route mod 告诉 Core 在哪放 extern node**（地图边界、天空点…）。extern 是没有 `Station` 的纯路径点：不停靠、不触发事件、不限速，只能被 links 当端点、被 lines 当必经点。名字在本 route 内唯一。
  - `links`：`RouteLinkSpec{from,to,withinStation,segments}`，**单向**站间连线。端点 `RouteEndpoint{kind(Outlet/Inlet/Extern),station,portIndex,externName}`；合法组合 Outlet→Inlet、Outlet→Extern、Extern→Inlet、Extern→Extern。`segments` 是按行驶顺序首尾相接的 `TrackSegment`（三次贝塞尔+`mesh`/`meshUnit`）。`withinStation=true` 表示同站内转场（如飞机从降落跑道滑行到起飞跑道），所在 run 按 `sameStationSpeed`。
  - `lines`：每条线路是成环的 `RouteLineEntry{name,stopIndex}` 数组。`stopIndex>=0`=在站 `name` 的第 `stopIndex` 个停靠位**停靠**；`stopIndex<0`=`name` 是 extern 名，只经过不停。**没有"相同站名相邻=折返"之类的隐含规则**：出图再回来/边界回转由 route 提供 extern 节点 + links 显式表达。线路可以跨过某站不停（不写那个站的 entry，Core 寻路时走它的站内直通连线）。
- **`TrackSegment`**（定义在 `station_mod.h`，站内/站间连线共用）：`mesh` 是这一段铺的路线资产（空=不铺），`meshUnit` 是 mesh 沿弧长的重复单位，`terrainFollow` 仅给 mod 自己的 `BuildTrack` 看（Core 不读不存）。隧道段由 mod 直接把 `mesh` 换成隧道资产，没有 `tunnel` 标志。

## 选项（SetProperty 里填，Core 只读）

- `stationType`（接哪类站点）、`vehicleType`、`useRoadnet`（公交：不给 links/externs，Core 用路网车辆寻路图拼路线，每站只有 stop 0）、`instantiate`（火车：Core 调 `BuildTrack` 并转发 hatch/terrainFit/拆除、UE 侧铺轨）、`trackWidth`、`lineVehicleCounts`(`LayoutRoute` 里按 `lines` 的下标给每条线路指定车辆数，缺项或<=0时按 1 辆)、`tiltAlongPath`、`dwellSeconds`。
- **三个绝对速度**（地图单位/游戏秒，非路网线路用）：`normalSpeed` 正常运行；`sameStationSpeed` 同站内运行（从降落跑道滑到起飞跑道这种同站 run）；`passSpeed` 通过但不停站（穿过中间站的站内直通连线时的上限）。公交只用 `normalSpeed`。
- **速度单位是游戏内秒，不是真实秒**：`Traffic::Tick` 喂给 `Route::Update` 的 `elapsedSeconds` 来自 `Player::Tick()` 里 `time->AddMilliseconds(delta*60*1000*timeFlowRatio)`，默认 `timeFlowRatio=2.0`，1 真实秒=120 游戏内秒。新增线路类型要按这个换算校准。
- **`easeSeconds`（真实秒）**：从 0 加速到 `normalSpeed` 用多久，Core 据此得到统一加速度 `a = normalSpeed / (easeSeconds*120)`，三个速度共用。run 太短来不及加速到限速时 Core 自动退化成三角形速度曲线（峰值低于限速）。

## 运动学（Core 预算，mod 只给选项）

run 两端（停靠位）速度为 0；中间按各段速度上限做前向/后向约束，得到加速/匀速/减速相位表。进站/出站、同站内、穿站不停三种速度变化都由同一套约束自然得到：0→加速→normal→减速→0；0→sameStation→0；normal→减速→pass→加速→normal。见 `route.md`。

## 实例化路线（铺轨、过山爬坡/隧道）

- `BuildTrack(getTerrain, getWater, getHeight)`（虚，默认不处理）：`Route::Build` 在 `LayoutRoute` 之后、建图之前对 instantiate 线路调用一次。mod 把 `links[i].segments` 改成最终几何（沿线采样地形、过山抬升/挖隧道），用 `AddTunnelHatch`/`AddTerrainFit`/`AddDemolition` 提交请求，和 `RoadnetMod::DistributeRoadnet` 同一个模式。
- `AddDemolition(centerline, width)`：折线扫过 width 宽的条带，Core（`Map::DemolishAlongStrips`）拆掉所有相交的楼体及其园区，带 `stationMod` 的建筑除外。
- `AddPier(mesh, chains, defaultSpacing, minSpacing, radius)`：桥墩请求(`PierRequest`，`general_geometry.h`)。`chains` 每条是一根轨道的 `Connection` 链(按值存，按行驶顺序首尾相接)，请求链附近(不到 `trackWidth/2`)的本 route 轨道不算障碍；`mesh` 空串=Asset/Common 默认圆柱；`radius` 是墩半径。Core 的 `Route::PlacePiers` 沿链选位(默认墩距→缩短到最短→再加长，避开路网和已有 route 线路)，墩顶贴链上该点高度，墩底贴地形，见 `Core/traffic/route.md`"桥墩"。`BuildTrack` 里调用，结果在 `piers`。
- `AddHatch`/`AddTunnelHatch`/`AddTerrainFit` 转调 `general_geometry.h` 的 `BuildHatch`/`BuildTerrainFit`。

## 存档

`SaveState`/`LoadState` 只管 mod 自己的状态；`externs`/`links`（最终几何）/`lines` 由 `TrafficSaveCodec` 存读，读档后 `Route::Assemble` 重建寻路图和运动学，见 `traffic_save.md`。

## 依赖关系

- 依赖：`station_mod.h`（`TrackSegment`/`PortKind`）、`general_geometry.h`。
- 被谁依赖：`Core/traffic/route.h`；`Basic/traffic/route_*.h` 扩展这个基类。

## 待办/后续阶段

- 暂时没有隧道专用的轨道资产，隧道内复用火车轨道 mesh。

## 端墙（2026-11-20）
`AddTunnelHatch`末尾两个可选参数`startWallBottom/endWallBottom`(默认`kNoEndWall`=不加)，同RoadnetMod，见Dependence/map/roadnet_mod.md。火车`hatchRange`只在最靠隧道的那一片的隧道端加(入口侧t2端、出口侧t1端)，墙底=隧道端路面高度+`TUNNEL_ROOF_CLEARANCE`(1)。

## 手动线路 `--skip`（2026-11-23）

MapEditor 的"线路"工具可以手动设计火车/飞机线路：**实体线路**（节点 + 连线，每个节点有高度 z，连线有类型 ground/hug/mouth/tunnel/bridge 和所属 route id = route mod id）和**线路环**（预设建筑 + 实体线路节点依次连成的闭环，预设可设"停靠"，节点只表示"经过"）。数据在 `.roadnet` 的 `output.transit`，格式见 `map/transit_data.h`、`roadnet_file.md`。

- **`TransitData`**（`map/transit_data.h`）：纯值类型，`RoadnetMod::transit` 持有，Core 在 `Roadnet::Init` 里拷一份（`Map::GetTransit()`）。route mod 只读，**不要往里写**（跨 DLL 堆规则）。
- **`RouteMod::skipAuto`**：mod 在自己的构造函数里按参数设置（config 写 `"route_train --skip"` / `"route_air --skip"`，creator 把参数串交给 `ModArgs(args).GetBool("skip")`）。`true`=跳过自己的自动几何/连环逻辑，`LayoutRoute` 改走 `LayoutManual`；没写就和以前完全一样，手动数据被忽略。
- **`RouteMod::SetManualTransit(const TransitData*)`**（非虚）：`Route::Build` 在 `LayoutRoute` 之前给，`BuildTrack`（instantiate 线路）之后清空；指针指向 Core 持有的数据，生命周期覆盖 `Build`。`manual` 为空或没有数据时 `LayoutManual` 只打一行警告、不出任何线路。
- **`RouteStationInfo::presetIndex`**：站所在建筑是路网预设放出来的，则为该预设在 `.roadnet output.presets` 里的原下标（`PlacementPreset::outputIndex`→`Map::GetPresetIndexOfBuilding`），否则 -1。线路环里的预设项靠它找站（站名是 `"Station"+计数器`，哈希序，不稳定，不能用来引用）。
- 规划部分（线路环→用到的连线、节点转向、缺口、停靠线路）是火车/飞机共用的纯函数 `PlanManualRoute`（`Basic/traffic/route_common.h`），见 `route_common.md`；两个 mod 各自把规划落成自己的几何，见 `route_train.md`/`route_air.md`。
- Core 一侧**不需要新逻辑**：extern 途经点、站停靠、站穿过不停（`EDGE_THROUGH`，按 `passSpeed`）原本就支持。
