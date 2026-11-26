# route_bus.h / route_bus.cpp

## 职责

`BusRoute`(`route_bus`)：route域的默认实现之一，`useRoadnet=true`。

## 关键设计

- **只输出 `lines`**：每项 `{站名, 0}`(公交站只有 stop 0)；没有 `externs`/`links`。Core 对每个 run 用路网车辆寻路拼路线(`route.md`"两种线路")。
- **`LayoutRoute`：同名道路成一条线**：按 `RouteStationInfo::roadName` 分组(棋盘格里同名=同一条横/竖线，被路口切成多段 `Road`，站序跨路口连续)。取组内第一个站的行驶方向作参考，每站按自己 `dir·参考` 分去程(>=0)/回程；沿路坐标 `p = x*ref.x + y*ref.y`(棋盘格线是旋转约±0.2弧度的近直线，够用)。线路 = 去程站按 p 升序 + 回程站按 p 降序，首尾相接成环；没有对侧站的组不建线。线路两端尽头没有显式掉头：Core 在相邻站之间用路网寻路，到头时靠路口的 U 形连接(`RoadJunction::BuildConnectors`)回到对侧站。
- **车辆数**：`lineVehicleCounts = max(1, 站数/4)`(估值)。
- **手动线路 `--skip`（2026-11-23）**：`BusRoute(args)` 读 `--skip`（config 写 `"route_bus --skip"`）；`LayoutRoute` 在 `skipAuto` 时走 `LayoutManual`：遍历 `manual->roadLines`（`route==route_bus` 且>=2 站），每条线 `entries = {BusStopName(线,站), 0}`，`lineVehicleCounts = max(1, 站数/4)`；站由 `station_bus::AssignManualRoads` 建，缺站/不通的线由 Core `Assemble` 丢弃。没有 `--skip` 时手动数据被忽略，走自动棋盘格公交。没有手动数据时 `--skip` 只告警、不建线。
- **速度**(`SetProperty`)：`normalSpeed=0.025`(只用这一个速度)、`dwellSeconds=360`、`easeSeconds=3`。单位、换算和三类型对照表见 `route_train.md`。公交每站都停、站距短，多数 run 是三角形速度曲线。

## 依赖关系

- 依赖：`Dependence/traffic/route_mod.h`。
- 被谁依赖：`Basic/basic.cpp`；`Core/traffic/route.cpp`、`traffic.cpp`。
