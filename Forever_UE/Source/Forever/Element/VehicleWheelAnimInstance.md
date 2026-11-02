# VehicleWheelAnimInstance.h / VehicleWheelAnimInstance.cpp

## 职责

`UVehicleWheelAnimInstance`(+ `FVehicleWheelAnimInstanceProxy`)：**纯C++驱动车辆轮子骨骼的滚动/转向动画**，绕开Animation Blueprint节点图，不需要在编辑器里手工搭动画蓝图。也驱动飞机螺旋桨骨骼的旋转。

## 关键设计

- **重写`FAnimInstanceProxy::Evaluate()`**：`PreUpdate()`(游戏线程)从`UChaosWheeledVehicleMovementComponent`读每个轮子的滚动角/转向角写进`WheelPoses`，`Evaluate()`(可能在工作线程)读，两者之间不加锁，因为`FAnimInstanceProxy`本身保证`PreUpdate`和`Evaluate`不会同时跑。
- **在轮骨的局部坐标系上叠加旋转**：骨骼局部Y是车轴(滚动)、Z是转向轴，所以导入的骨骼必须是"identity旋转、Y朝车轴"的约定(glTF导入的骨骼满足)，见`Forever_UE/Tools/VehicleRig/README.md`。轮子骨骼名取自`WheelSetups[i].BoneName`。
- **视觉滚动转速限速**：`UChaosVehicleWheel::GetRotationAngle()`是物理里的真实滚动角，车速一高每帧变化量很大，视觉上像抽风。所以每帧只累加"物理真实变化量"和"允许最大变化量(`MaxWheelSpinDegreesPerSecond`，默认1080度/秒=3圈/秒)"里更小的那个，只影响视觉，**不影响驾驶手感**。该值是`UPROPERTY(EditDefaultsOnly)`，不同车型可按轮子大小各调。
- **飞机螺旋桨**：owner是`APlaneElement`时通过`SetPropellerOwner`设置，`PreUpdate`读它的旋转角/骨骼名/轴，`Evaluate`叠加到骨骼上；头文件里`APlaneElement`只前向声明，`TWeakObjectPtr`赋值需要完整类型，所以实现在`.cpp`。
- 车身组件通过`SetAnimInstanceClass(UVehicleWheelAnimInstance::StaticClass())`在`AVehicleElement`构造函数里指定；`transient`类。

## 依赖关系

- 依赖：`UChaosWheeledVehicleMovementComponent`、`APlaneElement`(螺旋桨)。
- 被谁依赖：`AVehicleElement`/`APlaneElement`的`CarMesh`。

## 待办/后续阶段

- 无。
