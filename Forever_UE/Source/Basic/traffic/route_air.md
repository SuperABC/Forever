# route_air.h / route_air.cpp

## 职责

`AirRoute`(`route_air`)：route域的默认实现之一，多机场。

## 关键设计

- **模型**：每座机场(`station_air`)有两条跑道 t=0/1，每条有 `inlet t`/`outlet t`/`stop t`，朝向=跑道方向。`LayoutRoute` 输出 `externs`/`links`/`lines`；**没有天空节点/`_ext` 站点/`controlRules`**，空中几何直接写在 link 的段里。
- **出城再回来**：每座机场每条跑道两个 extern：`air_<机场>_out<r>`(出)、`air_<机场>_in<r>`(进)。出站口沿跑道方向前方、进站口后方各 200 单位(=巡航高度 100/爬升斜率 0.5)，z=100。若在地图内，extern 沿跑道方向延长到地图边界(`RayBoxIntersection`)，已在图外则在原位(沿飞行方向错开 0.01)。links：outlet→extern out(爬升 + 天空段，臂 16)、extern out→extern in(直线)、extern in→inlet(下降)。
- **机场环线**(每座机场一条)：`{A,0}, out0, in1, {A,1}, out1, in0`(出 extern 后直接飞到另一条跑道的进 extern 回城)。
- **机场互联**：每对机场(A,B)，A 用朝向最接近 A→B 的跑道起飞、B 用朝向最接近来向的跑道降落；link outlet→inlet 三段 `[爬升, 中段贝塞尔(臂 32 级，随两机场距离缩放), 下降]`。线路 `[{A,降落跑道},({A,起飞跑道}，不同时),{B,…}]`，一对机场一条线路一架飞机，不调度避让。
- **同站滑行**：降落跑道≠起飞跑道时加 `withinStation=true` 的 link(outlet(降落)→inlet(起飞)，同端头 U 形)，这样的 run 整段按 `sameStationSpeed`。
- **`tiltAlongPath`**：飞机俯仰跟路径切线竖直分量走。
- **速度**(`SetProperty`)：`normalSpeed=0.175`、`sameStationSpeed=0.0175`、`passSpeed=0.0875`、`dwellSeconds=720`、`easeSeconds=6`。单位、换算和三类型对照表见 `route_train.md`。

## 依赖关系

- 依赖：`Dependence/traffic/route_mod.h`、`route_common.h`。
- 被谁依赖：`Basic/basic.cpp`；`Core/traffic/route.cpp`。

## 待办/后续阶段

- 互联线路只用前两条跑道；专用起飞/降落跑道未实现。
- 天空段间距/高度/滑行速度是估值。harness 验证过线路规划(1 座环线，2 座 3 条线；滑行 run 上限 0.0175、飞行 0.175)，**未 PIE**。
