# VehicleWheelFront.h / VehicleWheelFront.cpp

## 职责

`UVehicleWheelFront`：**前轮配置**——参与转向(`MaxSteerAngle=40`)，不参与手刹。是`AVehicleElement`构造函数里`WheelSetups[0..1]`的默认`WheelClass`。

## 关键设计

- 是否参与驱动由差速器配置(`DifferentialSetup`)决定，这里不覆盖`bAffectedByEngine`(显式写false只为不留疑问，配合默认后轮驱动)；照抄引擎Vehicle模板`TP_VehicleAdvWheelFront`的最小配置。
- `WheelRadius=35`是估值，**和模型实际轮子半径不一定对应**。按车型改半径不要建蓝图子类，直接在`AVehicleElement`蓝图里填`wheelRadius`，`BeginPlay`里会按轮位覆盖(`SetWheelRadius`)，因为Chaos建物理时读的是轮子类CDO、没法按实例改。`MaxSteerAngle`/悬挂行程(8/8)仍读这里的默认值，没法按车型改。
- `FrictionForceMultiplier=3`。

## 依赖关系

- 依赖：`UChaosVehicleWheel`。
- 被谁依赖：`AVehicleElement`(`WheelSetups[0]`/`[1]`)。

## 待办/后续阶段

- 无。
