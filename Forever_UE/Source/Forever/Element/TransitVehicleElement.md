# TransitVehicleElement

## 职责

公共交通线路车辆(公交/火车/飞机)在场景里对应的Actor——纯展示物，运动学地沿`Route`算出来
的transform移动，不模拟物理、不接受操控/搭乘。

## 关键设计

- **运动学跟随，不是物理驱动**：`Tick`每帧直接读`vehicle->GetTransform()`(`Core`层
  `Route::Update`早已按时刻表算好的世界坐标+偏航角)写`SetActorLocationAndRotation`，
  和`AVehicleElement`(真实可驾驶载具，`bSimulatePhysics=true`的骨骼网格)完全是两套机制——
  这里没有刚体、没有碰撞响应，`Tick`只是把Core算好的数字誊抄到Actor transform上。
  `TRANSIT_WORLD_SCALE`(=1000)和`ForeverTrafficFrameworkComponent.cpp`的
  `TRAFFIC_WORLD_SCALE`是同一个值、同一套"地图单位→UE厘米"换算，这个项目的既有约定是每个
  需要它的`.cpp`文件各自维护一份，不额外抽公共常量。
- **纯展示物，不参与碰撞/导航**：构造函数里`bodyMesh->SetCollisionEnabled(NoCollision)`+
  `SetCanEverAffectNavigation(false)`——这次不做操控/搭乘，不需要挡住别的东西，也不需要被
  别的东西的重叠检测测到，见`VehicleMod::drivable`/`boardable`字段(这次公交/火车/飞机三个
  `VehicleMod`都设成`false`，预留给以后真的要做"公共交通能上下车"时用)。
- **立方体占位，按`VehicleMod::sizeX/Y/Z`缩放**：三种公共交通车型的`blueprintPath`都是
  空字符串，`Init()`固定用`/Engine/BasicShapes/Cube`，`SetWorldScale3D`按
  `sizeX/sizeY/sizeZ`除以Cube默认边长(100cm)算缩放系数——以后要换成真实蓝图资产时，
  `GenerateVehicles`已经按`blueprintPath`非空时`LoadClass`优先的逻辑写好了，不需要改这个
  类本身。
- `vehicle`字段是裸指针引用，不持有所有权——`EndPlay`只置空不delete，`Vehicle`对象的生命
  周期归`Traffic`管(和`ACitizenElement`/`AVehicleElement`同一套"Forever层Element只引用、
  不管理Core对象生命周期"的安全原则)。

## 依赖关系

- 依赖：`Core/traffic/vehicle.h`(`GetTransform`/`GetSize`)。
- 被谁依赖：`Forever/Framework/ForeverTrafficFrameworkComponent.cpp`的`GenerateVehicles`
  (对`vehicle->GetRoute()`非空的车辆生成这个Actor，而不是`AVehicleElement`)。

## 待办/后续阶段

- 不支持操控/搭乘——`VehicleMod::drivable`/`boardable`已经预留了这个开关，真要做的话这个
  类需要补充和`AVehicleElement`类似的`Possess`/上下车逻辑，这次不实现。
