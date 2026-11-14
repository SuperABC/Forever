# route_train.h / route_train.cpp

## 职责

`TrainRoute`(`route_train`)：route域的默认实现之一。`instantiate=true`，在地图上真正铺轨（桥/隧道/拆迁）。

## 关键设计

- **模型**：每站两条轨道 t=0/1(航向相反)，每条轨道 t 有 `inlet t`、`outlet t`、`stop t`(同下标)，由 `station_train` 声明(`station_train.md`)。`LayoutRoute` 只输出站间 `links`、`externs`、`lines`，**不再有 rise/mid 之类的节点**，也没有控制臂规则/`controlRules`。
- **站间连线(`addLink`)**：outlet→(抬升段：4 水平/1.5 高，臂 0.5)→沿地形的桥(`terrainFollow`，臂=两点距离 1/3)→(下降段)→下一站 inlet。每段都带 `mesh=kTrainTrackMesh`、`meshUnit=kTrainTrackUnit`(0.25)。同一对端点只加一次(`set<pair<string,string>>` 去重)。
- **线路拓扑**（沿用旧规则，只是改成 `{站名,轨道}` + extern 名）：
  - **>=3站**：按站中心相对地图中心 `atan2` 排序(+Y 向南，升序=屏幕顺时针)。**顺时针环**和**逆时针环**各一条，每站都停。**靠右行驶**：两条轨道的左右由 `TrainStation::Layout` 保证。**南北线**：北 extern→最北站→顺时针弧(经东侧)途经站→最南站→南 extern，往返；**东西线**同理(经北侧)。共 4 条线。
  - **2站**：一条线 A↔B，两端伸到边界回转。**1站**：两端伸到边界回转，一条线。
  - **边界回转(`boundaryExtern`)**：extern `bd_<站>_<轨道>_<方位>`(z=1.5，朝向=边界外法向)。设 D=桥高节点到边界的距离：边界正对航向时出图点=正前方的垂足；边界在侧面(南北线/东西线)时，出图点=沿航向再前进 D 后向边界做垂线的垂足(沿边界方向夹在地图内)，轨道从航向平滑拐成约 1/4 弧。**从车站出来/回到车站的控制臂=D/3**(不再是固定 4 单位，原来的固定臂在侧向边界上是近似直角的急转弯)。links：outlet→extern、extern→另一条轨道的 inlet。extern 处沿边界掉头的那一小段仍是 90° 拐角(在地图边缘)。extern 不停靠、不触发事件、不限速；原来的 600 秒静默停靠取消。
- **车辆数**：每条火车线路 4 辆(`LayoutRoute` 给每条线路往 `lineVehicleCounts` 填 4)。
- **速度**(`SetProperty`，地图单位/游戏秒；1 真实秒=120 游戏秒)：`normalSpeed=0.1`、`sameStationSpeed=0.1`(火车不用)、`passSpeed=0.05`、`dwellSeconds=600`、`easeSeconds=10`(真实秒，从 0 加速到 `normalSpeed` 的时间，Core 据此得固定加速度，见 `route.md`)。`trackWidth=0.8`。

  | 类型 | 环线长度(地图单位) | speed | dwellSeconds | easeSeconds |
  |---|---|---|---|---|
  | 公交 | 约120~180 | normal 0.025 | 360 | 3 |
  | 火车 | 约2000 | normal 0.1 / pass 0.05 | 600 | 10 |
  | 飞机 | 约2050 | normal 0.175 / sameStation 0.0175 / pass 0.0875 | 720 | 6 |

  固定加速度的曲线比原来"时间保持的梯形"更慢，`easeSeconds` 已为此降低，需 PIE 观感调。

## BuildTrack（过山爬坡/隧道）

`Route::Build` 在 `LayoutRoute` 之后调用一次。只对 `terrainFollow` 的段处理，其余段原样保留：
1. 把平面曲线按弧长每 `TRACK_SAMPLE_STEP`(1 单位=10m)采样，记录地形类型和 `getHeight`；
2. 连续的 `terrain_mountain` 切成一座座山，规则照抄 `JingRoadnet::buildArm`（常量 `MOUNTAIN_MAX_SLOPE`/`TUNNEL_HEIGHT`/`TUNNEL_HATCH_LENGTH`/`TERRAIN_FIT_FALLOFF` 在 `route_*.cpp` 里重新定义，两边数值要一致）：坡度<=0.5 或山后是海→抬升(`AddTerrainFit`)；否则→隧道（前后各 8 单位 S 形坡道，`AddTunnelHatch` 挖洞，中间水平隧道段），放不下坡道则退化为抬升；车站两端 `TRACK_STATION_FLAT_SAMPLES` 个采样点保持水平；
3. 每相邻两点输出一段三次贝塞尔，完全水平共线的合并；每段 `mesh`/`meshUnit` 由原段复制（**隧道段目前仍用同一个轨道 mesh**，没有隧道标志，将来要换资产只需在这里改 `mesh`）。
- **拆除**：每条 link 的采样折线+`trackWidth` 提交 `AddDemolition`，Core 拆相交的楼体/园区(带 `stationMod` 的不拆)，见 `map.md`。抬升/下降短段(在站内)不提交。

## 依赖关系

- 依赖：`Dependence/traffic/route_mod.h`、`station_train.h`(mesh 常量)、`route_common.h`(`MakeBezierSegment`)。
- 被谁依赖：`Basic/basic.cpp` 注册进 `config.json` 的 `"route_mods"`；`Core/traffic/route.cpp`。

## 待办/后续阶段

- 轨道与轨道、轨道与机场天空线交叉不处理。
- 无头 harness 验证过(随机地图 4 线路/连续/靠右/导航无悬空/速度表合法)；**未在 PIE 验证**：轨道/桥/拆迁视觉、实际行驶/停靠事件、读档重建、`passSpeed` 穿站路径(默认线路每站都停，没用到 Through 连线)。
- 隧道内暂无专用轨道资产。
