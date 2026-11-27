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
- **红绿灯**（车和过横道的行人都按灯，转角不管）：
  - `BuildFromMap` 给多于两条路的路口建信号表：`PairApproaches(angles, hasInbound)` 只统计有驶入车道的路（只出不进的单行道不分组，组号 -1），每次在剩下的路里挑夹角最接近 180° 的两条成一组，最后可能剩一条单独一组。车行连线登记时带上驶入的路、人行横道登记时带上它横穿的路（两端锚点所属的那条，都记在 `Entry::approach`）；转角不横穿道路，总是 -1。
  - 时长是常量 `kGreenSeconds`=15、`kYellowSeconds`=3（现实秒，和请求用同一个 `now`）。一组一个槽（绿+黄），各组按组号轮流；`GetSignal(junctionId, approach, now)` 从 `now - cycleStart` 算出相位，不需要 Tick。没有信号表/没激活返回 `None`，不在当前组的是 `Red`。
  - `SetSignalActive(junctionId, active, now)`：激活时 `cycleStart=now`，从第 0 组绿灯开始。只有激活的路口有灯（UE 里只激活玩家附近生成了灯的路口），没激活就和没有红绿灯一样。
  - `Request`：已持有的照常续租（黄灯/红灯都不赶已经放行的车）；之后如果是车、路口激活、它那条路不是绿灯，就直接拒绝，**不写进 `waiters`**（写进去的话它的 t0 早于绿灯车，会让绿灯车一直等它）。黄灯不放新车；换组时上一组还在路口里的车由冲突表挡住新组的冲突方，路口自然清空。绿灯时同组内仍按冲突表互相让（对向左转让直行等）。
  - 行人（横道）反过来：横穿的那条路**是红灯**才放（那条路的车正停着），绿灯和黄灯都拒绝、同样不进 `waiters`；那条路不分组（只出不进）或路口没激活就照旧只按冲突表。已经在横道上的人续租照常，那条路变绿以后也走完，并且照旧用持有挡住那条路刚放行的冲突车。红灯期间别的路转弯进这条路的车仍要按冲突表给横道上的人让。
- **`FindVehicleSpans(points, tolerance)`**：Core 推算的路线只是一串点（不带连线），按车行连线的起点（驶入锚点）和终点（驶出锚点）的位置在点列里找出经过的路口连线，返回（连线, 起点下标, 终点下标），起点后 256 个点内没出现这条连线的终点就不算。UE 的 `FollowPath` 指令（流式接手、读档恢复的车）用它补路口信息。
- **`FindCrosswalk(a,b)`**：市民路径只是节点坐标串，用"这一段两端坐标"匹配横道连线（格子哈希，方向不限）。
- **`CheckInvariant(now)`**：互相冲突的连线不会同时有未过期的持有者，测试用。

## 注意

- 单位：几何一律是地图单位（1=10m）；`length/exitFree` 也是。
- 某个等待者因出口满等着，也会挡住更晚的冲突方（队头阻塞），这是简化。
- 没有道路优先级。行人按横穿的那条路的车行灯走（没有单独的行人灯模型，也没有行人专用相位），信号分组和时长写死在代码里，不能按路口配置。

## 依赖关系

- 依赖：`map/map.h`（`GetJunctions`/导航图，仅 `BuildFromMap`）、`map/roadnet.h`（`RoadJunction`）、`map/general_geometry.h`（`Connection`/`Node`）。
- 被谁依赖：UE 层 `UForeverTrafficFrameworkComponent`（持有）、`AVehicleCommandController`、`ACitizenController`；测试 `Tools/JunctionTest`。
