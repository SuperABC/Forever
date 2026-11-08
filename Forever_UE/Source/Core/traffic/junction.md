# junction.h / junction.cpp

## 职责

`JunctionTraffic`：路口通行管理。车/人只和它交互，它只认路口里的 `Connection*`，不持有车/人的指针（车/人用一个 `int64` entity 标识自己，`MakeEntity(kind, id)`）。不读任何时钟：`now`（秒，单调、暂停时不走）由调用方传入，所以可以脱离 UE 单测（`Tools/JunctionTest`）。

## 关键设计

- **连线登记与冲突表**：`AddConnection(conn, kind, junctionId)` + `BuildConflicts()`；`BuildFromMap(map)` 从 `Map::GetJunctions()` 和车行/行人导航图收集：车行 = 进车道锚点出发、终点是本路口出车道锚点的边；行人 = 两端都是本路口人行锚点且长度 <3 地图单位的边（同一 approach 两侧 = 横道 `Crosswalk`，不同 approach = 转角 `Corner`）。冲突按同一路口内的几何算：
  - 车–车：同一进车道出发（分叉）不冲突；终点是同一出车道（汇入）冲突；否则 2D 折线距离 ≤ `vehicleTolerance` 冲突。
  - 车–横道 ≤ `crosswalkTolerance`；车–转角 ≤ `cornerTolerance`（基本只在真正相交时）；人–人从不冲突。
- **`Request(conn, entity, now, length, exitFree)`**（返回 true=可进，含续租）：
  - 已持有：续租（`leaseSeconds`，默认 1s），粘性，不再检查。
  - 连线已开（有持有者）：同连线新请求放行，除非出口检查不过，或请求者是车且有 t0 更早的冲突连线上的**人**在等；可选 `maxOpenSeconds`（默认 0=关）限制连续开放时间。
  - 连线未开：无冲突持有者 且 无 t0 更早的冲突等待者（相同比 entity）且出口检查通过才放行。
  - 没放行则记入 `waiters`（保留首次 t0），租约内不再请求就自动消失。
  - **出口检查**（仅车）：`exitFree − 已放行但还没离开的同出口锚点连线持有者长度之和 ≥ length`。`exitFree` 由调用方量（连线终点之后出口车道的实际空闲长度，地图单位）。
  - 允许车流连续通过造成冲突方饿死（设计如此，不是 bug）。
- **`Leave`**：车尾出了连线终点/放弃时调用，同时清掉排队记录。**租约过期等同 Leave**。
- **`FindCrosswalk(a,b)`**：市民路径只是节点坐标串，用"这一段两端坐标"匹配横道连线（格子哈希，方向不限）。
- **`CheckInvariant(now)`**：互相冲突的连线不会同时有未过期的持有者，测试用。

## 注意

- 单位：几何一律是地图单位（1=10m）；`length/exitFree` 也是。
- 某个等待者因出口满等着，也会挡住更晚的冲突方（队头阻塞），这是简化。
- 没有道路优先级、没有红绿灯；红绿灯以后给 `Request` 增加信号组来源（相位表按时间推状态）。

## 依赖关系

- 依赖：`map/map.h`（`GetJunctions`/导航图，仅 `BuildFromMap`）、`map/roadnet.h`（`RoadJunction`）、`map/general_geometry.h`（`Connection`/`Node`）。
- 被谁依赖：UE 层 `UForeverTrafficFrameworkComponent`（持有）、`AVehicleCommandController`、`ACitizenController`；测试 `Tools/JunctionTest`。

## 待办/后续阶段

- `BuildFromMap` 还没有用真实地图验证过（单测用手摆的十字路口），第一次 PIE 看 `UForeverTrafficFrameworkComponent: junction traffic built (N connections)` 日志里的连线数是否合理。
