# Controller 体系（载具 / 公交 / 人物）

`Source/Forever/Controller/`：`CommandQueue.h`（指令队列模板）、`VehicleCommandController`（载具基类）、`BusController`、`CitizenController`（见 `CitizenController.md`）。

## 指令体系
- `TCommandQueue<T>`：`Push / Clear / Current / Advance / IsEmpty`，维护"指令列表 + 当前执行哪条"，所有 Controller 共用；`Advance`/`Clear` 后之前拿到的 `Current()` 指针失效。
- 载具指令 `FVehicleCommand`：
  - `NavigateToNode(node)`：从当前车位导航到某个节点（`vehicleNavGraph` 节点或 Map 辅助节点=公交站 stationNode），到了指令结束。
  - `LoopNodes(nodes, loopStart)`：按节点序列永远循环，直到 `ClearCommands/StopNow`。`bStartAtStop`=车当前就停在 `nodes[loopStart-1]`（先停靠 `startDwellSeconds`）；`bFireEnterAtStart`=对该站补发进站；`bStopAlreadyEntered`=进站事件已由 Core 时刻表发过，只负责补出站（运动学→物理接管用）。
- Core/UE 只管往 Controller 里塞指令，开车全在 Controller 里。

## AVehicleCommandController（载具基类）
- 开车方式和玩家 WASD 完全一致：`AVehicleElement::SetDriveInput(throttle, brake, steer)` / `SetHandbrake`。
- 路径：`Map::PlanVehicleRouteToNode`（起点是车位置，带车头朝向避免选反向车道）/`PlanVehicleLeg`（节点到节点）→ UE 坐标（×1000）→ 保留原顶点、长段按 `pathSpacing=250uu` 细分 → 每点曲率（三点外接圆，取相邻最大）→ 限速 `v=sqrt(maxLateralAccel/κ)`（夹在 `minCornerSpeed..maxSpeed`）→ 终点 v=0 并反向扫描 `v_i ≤ sqrt(v_{i+1}²+2·maxBrakeDecel·ds)`，得到"弯前/站前提前减速"的速度剖面。
- 物理模式：pure pursuit 转向（`steer=atan(2·wheelbase·sinα/Ld)/maxSteerAngle`，轴距取前后轮骨骼，最大转角取前轮 CDO），前视 `lookaheadBase+lookaheadGain·v`；油门/刹车对速度剖面误差做 P 控制（`throttleKp/brakeKp`）；低于 80uu/s 不踩刹车改拉手刹（Chaos 的"倒车当刹车"会挂倒挡）；到站判定 = 剩余距离 ≤ `arriveDistance` 且速度 < `arriveSpeed`，或剩余 < 40uu，或终点附近卡住 5 秒。
- 运动学模式：同一条路径、同一份剖面，Controller 自己积分标量速度沿路径推进，`SetActorLocationAndRotation` 到路径采样点，关物理（`AVehicleElement::SetKinematicMode`）。模式由 owner 注入的 `SetPhysicsRangeQuery(pos, bCurrentlyPhysics)` 决定（带滞回，见 `ForeverTrafficFrameworkComponent.md`），状态机（指令/停站）两种模式共用，切换不丢状态。切回物理：位置落到路径采样点上方 20uu、`ResumePhysics(切线方向*速度)`。
- 控制权：`drivenVehicle` 是弱引用。玩家搭乘（`SetRideMode`）时 Possess 权在玩家手里，AI 仍按引用发输入；玩家驾驶（非搭乘）时不发输入；玩家放手后车没有 Controller，下一帧 Tick 自动重新 Possess。空闲（无指令）时原地拉手刹。车被销毁则 Controller 自毁。
- 可调参数（`UPROPERTY(EditAnywhere)`）：`maxSpeed`、`maxLateralAccel`（防翻车关键）、`maxBrakeDecel`、`maxAccel`（运动学）、`minCornerSpeed`、`throttleKp/brakeKp`、`lookaheadBase/lookaheadGain`、`arriveDistance/arriveSpeed`、`pathSpacing`。

## ABusController
- 公交默认参数更保守（`maxLateralAccel=200`、`maxSpeed=1400`）。循环里到站：发 `enter_stop`、停靠 `dwellSeconds`（默认 5 真实秒，Core 的 `RouteMod::dwellSeconds` 是游戏秒不直接用）、发 `leave_stop`、去下一站。`NavigateToNode` 路过站不发事件。`ClearCommands/StopNow` 在停站中会补发 `leave_stop`（让脚本撤掉上车/下车选项）。
- 事件经 `UForeverTrafficFrameworkComponent::FireStopEvent(vehicle, stationNodeId, entering)` 转成站名事件发给这辆车自己的脚本。

## 未验证 / 风险
- 未在 PIE 验证。重心下移（`AVehicleElement::busCenterOfMassDropZ`，默认 60uu）和 `maxLateralAccel` 需要实测调。
- 起步朝向与首段路径差很大（例如测试运行时车横在路上）靠方向盘打满宽转弯，可能蹭墙；不处理倒车掉头。
- 公交撞前车会被缓慢推着走，本次不做避障。
