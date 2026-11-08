# VehicleCommandController.h / VehicleCommandController.cpp

(`VehicleController.md`是Controller体系总览，本文档是载具基类的完整说明。)

## 职责

`AVehicleCommandController`：**载具Controller基类**(公交现在用，以后各种交通工具共用)：维护指令列表+当前执行哪条(`TCommandQueue<FVehicleCommand>`)，用和玩家WASD完全一样的油门/刹车/转向输入(`AVehicleElement::SetDriveInput`)开车。

## 关键设计

- **指令**(`FVehicleCommand`)：`NavigateToNode`(从当前位置导航到某个节点——`vehicleNavGraph`节点id或公交站的`stationNode`，到了就结束)、`LoopNodes`(按`nodes`循环行驶，永不结束，直到`ClearCommands`/`StopNow`；到站由子类决定停靠/发事件)。Core/UE只负责塞指令，开车细节全在Controller里。`bStartAtStop`/`startDwellSeconds`/`bFireEnterAtStart`/`bStopAlreadyEntered`几个字段服务"中途接管"(从Core时刻表交给物理、或`NavigateToNode`刚到站后接循环)，保证进站/出站事件成对、不重发。
- **路径跟随**：指令 -> Map规划的折线(Map单位x1000=UE单位) -> 以`cornerRadius`做拐角圆角 -> `pathSpacing`(250uu)重采样 -> 每点按曲率限速`v=sqrt(aLat/k)` -> 终点`v=0`并反向扫描刹车距离，得到"弯前/站前提前减速"的速度剖面(`profileAccel`限制过弯后的加速) -> 转向用Stanley式(航向误差+横向偏差修正)：航向参考取前方`lookaheadBase+lookaheadGain·v`处的路径切线，横向偏差用**前轴位置**(`AVehicleElement::GetFrontAxleLocation`)相对导航线计算，`crossTrackGain`为增益，除以前轮最大转角(`GetMaxSteerAngleDegrees`，取不到用40度)得到steer -> 油门/刹车对速度误差做P控制(`throttleKp`/`brakeKp`)，低于80uu/s不踩刹车改拉手刹(Chaos的"倒车当刹车"会挂倒挡)。到站判定：剩余距离<=`arriveDistance`且速度<`arriveSpeed`，或剩余<40uu，或终点附近(<800uu)几乎不动持续5秒。
- **两种执行模式**：**物理**(真实Chaos驾驶)和**运动学**(不发输入，Controller自己积分速度沿同一条路径推进，关物理)。远离玩家(地形LOD范围外，物理不可靠)自动切运动学，靠近切回物理；指令/停站状态机两种模式共用，所以切换不丢状态。范围判定由owner(`UForeverTrafficFrameworkComponent`)通过`SetPhysicsRangeQuery`注入，带滞回(第二个参数是当前是不是物理)。
- **控制权**：`drivenVehicle`是弱引用。玩家搭乘(`SetRideMode`)时Possess权在玩家手里，这里照样每帧发输入；玩家驾驶(非搭乘)时不发输入；玩家放手后车没有Controller了，下一帧自动重新Possess回来。
- **状态机**：`Idle`/`Driving`/`Dwelling`。`BeginDwell`/`DepartFromDwell`处理停靠；`QueryLoopProgress`返回循环进度(所在腿、已走比例、是否停靠中、停靠已过比例)，供交还Core时刻表时对齐进度。卡住检测用`stuckSeconds`。
- **`GetRemainingWaypoints`**：行驶中返回剩余路径(世界坐标，第一个点是车当前位置)，空闲/停靠中/没有有效路径返回false，"你在去哪"折线(`StartTestNavigation`)用它。
- 空闲(无指令)时原地保持：滑行中踩刹车，慢速后拉手刹；车被销毁则Controller自毁。
- **子类钩子**：`OnStopEntered`/`OnStopLeft`/`GetDwellSeconds`，公交在`ABusController`里实现。
- 可调参数(UE单位cm)都是`UPROPERTY(EditAnywhere)`：`maxSpeed`/`maxLateralAccel`(弯道限速，防侧翻关键)/`maxBrakeDecel`/`maxAccel`/`arriveDistance`/`arriveSpeed`等。

## 导航结束回调

`SetNavigateFinishedCallback(f(controller,node,bArrived))`：`NavigateToNode`到达或规划失败时，在指令已出队、回到Idle之后调用（回调里可以直接`PushCommand`）；`LoopNodes`不回调。`UForeverTrafficFrameworkComponent`用它发`vehicle_arrive`。

## 有序通行（跟车 + 路口请求）

每帧在 `TickDrivingPhysics/Kinematic` 里调 `ComputeTrafficLimit`，目标速度 = `min(速度剖面, 跟车限速, 障碍限速, 停止线限速)`；`bEnableTraffic=false` 整个关掉。

- **前车**：`FindLeader` 遍历 `UForeverTrafficFrameworkComponent::GetDrivers()`（被驱动车辆登记表），把对方车心投影到**我的路径**上，横向在 `pathHalfWidth` 内且在前方、高差<400 的最近一辆就是前车（和车道/边无关，所以同车道排队、汇入、路口里交叉都覆盖）。用 Krauss 安全速度 `v=-bT+sqrt((bT)²+vl²+2b·gap)`（b=0.8×`maxBrakeDecel`，T=`followTimeGap`，gap 扣掉 `standstillGap`）。对方速度取沿我行进方向的分量（≥0）。
- **障碍**：`ScanObstacles` 每 0.1s 沿路径向前每 250uu 做一个半径 170 的 `ECC_Pawn`+`ECC_Vehicle` 球形 overlap（忽略自己和已登记车辆；注意本项目 Pawn 预设忽略 Visibility，必须用对象类型），找到行人/玩家/无人驾驶车就当作前车并入 Krauss。人的胶囊对载具通道是 Overlap（车不会被挡），`AVehicleElement::PushNearbyCharacters` 只是最后兜底，所以靠这个扫描提前减速。
- **路口**：`PlanAndFollow` 把 `Map::VehicleRoutePlan::edges` 里属于 `JunctionTraffic` 的连线转成 `FPath::junctions`（`FJunctionMarker`，弧长区间靠把规划折线端点投影到建好的路径上得到）。进入视距（刹停距离+速度×请求间隔+`junctionRequestMargin`）且是队头（我和入口之间没有前车）后，每 `junctionRequestInterval`（交通时钟秒）`Request` 一次；放行后继续请求续租，车尾出了连线终点 `Leave`；没放行就在入口前 `stopLineMargin` 处按 `sqrt(2·b·距离)` 限速停住。车头已经压进连线就当作持有，不再退回。出口检查把连线终点之后第一辆前车的车尾距离（地图单位）作为 `exitFree` 传给路口。
- **对速度合成的影响**：物理模式限速到 <30 且速度低于 `kHandbrakeSpeed` 时松刹车拉手刹原地等；卡住计时在被前车/路口挡住时不累加；运动学的 60uu 最小蠕行在限速<60 时关闭。
- **清理**：`ClearCommands`/`FinishCommand`/重新规划/`EndPlay` 都会 `ReleaseJunctions` 对所有请求过的连线 `Leave`；租约（1s）兜底。
- **限制**：目前只有 `Controller` 驱动的车（公交、测试导航车）进登记表，没有"最多 16 辆"的强制上限（私家车实例化时再加）；停在路中的车（含停靠的公交）会挡住后车，不会变道超车。

## 依赖关系

- 依赖：`TCommandQueue`、`AVehicleElement`、`UForeverTrafficFrameworkComponent`、`Core/map/map.h`(导航图)。
- 被谁依赖：`ABusController`继承它；`UForeverTrafficFrameworkComponent`创建和驱动。

## 待办/后续阶段

- 目前只有公交用；横向加速度/重心(`AVehicleElement::centerOfMassOffset`，蓝图按车型填)等参数需PIE实测调。
- 起步朝向与首段路径差很大时靠方向盘打满宽转弯，可能蹭墙；不处理倒车掉头；不做避障(公交撞前车会被缓慢推着走)。
