# BusController.h / BusController.cpp

## 职责

`ABusController`：在`AVehicleCommandController`上加**"到站停靠+进出站事件"**，公交专用。

## 关键设计

- **事件只有一个发出方**：物理段(以及Controller自己的运动学段)的`enter_stop`/`leave_stop`由这里判定，经`UForeverTrafficFrameworkComponent::FireStopEvent`发给公交自己的script；远处走Core时刻表的运动学公交(`ATransitVehicleElement`)不经过这个Controller，事件由Core时刻表发。所以任何时刻事件只有一个发出方。
- `OnStopEntered(nodeId, bLoop)`：`bLoop==false`(导航到站)只是进线路前的路过，**不算进站**，不发事件；`OnStopLeft`发出站。
- `dwellSeconds`(默认5真实秒)：每站停靠时长。Core的`RouteMod::dwellSeconds`是游戏秒，不直接用。
- 构造函数把弯道限速调得比基类更保守(`maxLateralAccel=200`、`maxSpeed=1400`)：公交又高又宽，防侧翻。

## 依赖关系

- 依赖：`VehicleCommandController`、`AVehicleElement`、`UForeverTrafficFrameworkComponent`。
- 被谁依赖：`UForeverTrafficFrameworkComponent`(公交近处物理驾驶时创建)。

## 待办/后续阶段

- 无。
