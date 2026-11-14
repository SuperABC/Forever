# route_bus.h / route_bus.cpp

## 职责

`BusRoute`(`route_bus`)：route域的默认实现之一，`useRoadnet=true`。

## 关键设计

- **只输出 `lines`**：每项 `{站名, 0}`(公交站只有 stop 0)；没有 `externs`/`links`。Core 对每个 run 用路网车辆寻路拼路线(`route.md`"两种线路")。
- **`LayoutRoute` 不依赖"哪个方向是南/北/顺时针"的假设**：以所有站形心为参照，用每站真实 `departDir`，`radial x departDir` 的 2D 叉积符号(同号的一组天然构成同一个绕行方向)分两组；`cross>=0` 按角度升序，`cross<0` 降序，才会贴着每个点自己的 `departDir`。不要按固定 +Y 朝向手推——Map 层左右手公式变了结论会全部失效，必须从真实车道方向反推。
- **速度**(`SetProperty`)：`normalSpeed=0.025`(只用这一个速度)、`dwellSeconds=360`、`easeSeconds=3`。单位、换算和三类型对照表见 `route_train.md`。公交每站都停、站距短，多数 run 是三角形速度曲线。

## 依赖关系

- 依赖：`Dependence/traffic/route_mod.h`。
- 被谁依赖：`Basic/basic.cpp`；`Core/traffic/route.cpp`、`traffic.cpp`。

## 待办/后续阶段

- harness 在 Jing 路网下得 2 条线路各 4 个 run、上限 0.025；`AssignRoads` 只在 Jing 中心路上放站，棋盘路网会是 0 线。**未 PIE**(近处物理/远处运动学交接、`easeSeconds` 手感)。
