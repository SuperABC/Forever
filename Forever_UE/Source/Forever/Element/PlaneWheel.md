# PlaneWheel.h / PlaneWheel.cpp

## 职责

飞机的两种轮子配置：`UPlaneWheelNose`(鼻轮)、`UPlaneWheelMain`(主轮，左右各一)。

## 关键设计

- **飞机的推力是直接作用在机身上的力**(见`PlaneElement.md`)，轮子不带动力(`bAffectedByEngine=false`)，只负责转向、滚动和刹车。
- **鼻轮**：参与转向(地面A/D，`MaxSteerAngle=40`)，不参与刹车/手刹/驱动；半径25。**主轮**：不转向，参与刹车和手刹(Space)，用来在地面刹停/停机位驻车；半径30。
- 悬挂行程8/8，`SuspensionDampingRatio=1.2`(过阻尼，落地不回弹)，`FrictionForceMultiplier=3`。
- **半径是估值**，要按飞机模型的轮子大小调：在编辑器里基于这两个类各建一个蓝图子类改`WheelRadius`，再填进飞机蓝图的`WheelSetups[i].WheelClass`。(普通载具不用这样做，`AVehicleElement`有蓝图变量`wheelRadius`按轮位覆盖，飞机因为轮子类不同保留了单独的类。)

## 依赖关系

- 依赖：`UChaosVehicleWheel`。
- 被谁依赖：`APlaneElement`构造函数里把`WheelSetups`的三个轮子指派成这两个类。

## 待办/后续阶段

- 半径是估值，需按模型调。
