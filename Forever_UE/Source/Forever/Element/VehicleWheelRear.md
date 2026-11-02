# VehicleWheelRear.h / VehicleWheelRear.cpp

## 职责

`UVehicleWheelRear`：**后轮配置**——参与手刹，不参与转向。是`AVehicleElement`构造函数里`WheelSetups[2..3]`的默认`WheelClass`。

## 关键设计

- **`bAffectedByEngine`必须显式设true**：`UChaosVehicleWheel::bAffectedByEngine`默认是false，`AxleType`也默认`Undefined`；不显式设true的话，`SetupVehicle()`里"`AxleType==Undefined`时直接用`bAffectedByEngine`原值"这条分支会让`EngineEnabled`一直是false，油门给再大力矩也传不到任何轮子上。实测踩过：车完全不响应油门，其余诊断信息(`hasController`/`numWheels`)全部正常，唯独`forwardSpeed`纹丝不动。配合`DifferentialSetup`默认的`RearWheelDrive`，后轮设true、前轮(`UVehicleWheelFront`)保持false。
- `WheelRadius=35`是估值，按车型改半径见`VehicleWheelFront.md`(在`AVehicleElement`蓝图里填`wheelRadius`)。照抄引擎Vehicle模板`TP_VehicleAdvWheelRear`的最小配置；`FrictionForceMultiplier=3`，悬挂行程8/8。

## 依赖关系

- 依赖：`UChaosVehicleWheel`。
- 被谁依赖：`AVehicleElement`(`WheelSetups[2]`/`[3]`)。

## 待办/后续阶段

- 无。
