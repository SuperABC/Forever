# vehicle_mod.h / vehicle_mod.cpp

## 职责

`VehicleMod`：一种具体车型的行为/外观定义(比如`VehicleBasic`小汽车、`BusVehicle`公交、`TruckVehicle`货车)。

## 关键设计

- **`SetProperty()`两段式**：具体子类在这里填下面所有字段，不在构造函数里赋值；构造函数只负责id/count这类登记，Core创建完实例后立刻调一次，和`StorageMod::SetProperty()`同一套约定。
- **`blueprintPath`**：指向一个继承`AVehicleElement`的蓝图类的资源路径(`/Game/.../BP_XxxElement.BP_XxxElement_C`)。Dependence/Basic层是纯C++，不能持有`UClass`指针，只能存路径字符串。**用"蓝图类路径"而不是"骨骼网格路径"**，是因为Chaos载具组件要求骨骼网格在Actor构造函数阶段就绑好，而`SpawnActor`不能给构造函数传参，"这个Actor长什么样"必须靠"用哪个`UClass`去生成"在生成前定下来。`UForeverTrafficFrameworkComponent`用这个路径`LoadClass<AVehicleElement>()`再`SpawnActor`。新增车型的完整流程见`Forever_UE/Tools/VehicleRig/README.md`。
- **轮子半径和重心不在这里**：它们是`AVehicleElement`蓝图里的变量(`wheelRadius`/`centerOfMassOffset`)，按车型在蓝图里填。
- **Script配置**：`scriptModName`(成员默认空串=不建自定义ScriptMod；Basic的各车型在`SetProperty`里显式设为`script_empty`)+`milestoneNames`，和`JobMod`同一模式；用来驱动"靠近车辆弹出'上车'选项"，车辆自己的milestone脚本在`game_start`时`add_option`，选中后`enter_vehicle`。
- **下车点**：`exitOffsetX/Y/Z`，车身局部坐标(UE单位，X前/Y右/Z上)，默认车身左侧。下车前用它算世界坐标并检测站不站得下人，站不下就拒绝下车。`exitOffsetZ`默认100而不是0：车身Actor原点在车轮接地点附近，碰撞检测胶囊体是以给定点为中心，若用贴地高度当中心，胶囊下半截会整个扎进地板，永远报"被挡住"。
- **车型分类不是实例字段**：每个具体子类提供`static const char* GetCategory()`，通过`VehicleFactory::RegisterVehicle`的`CategoryFunc`注册，Core用`VehicleFactory::GetCategory(id)`/`GetIdsByCategory(category)`查，不需要为读种类创建临时实例。多个车型可共用一个种类名，停车位(`ParkingSpot::categories`)按种类挑车型。
- **操控/搭乘**：`drivable`/`boardable`/`capacity`。公共交通车辆是运动学地沿线路移动的展示物，不接受玩家直接操控。
- **立方体占位外观**：`sizeX/Y/Z`——`transitMeshPath`为空时，`ATransitVehicleElement`按这个缩放`/Engine/BasicShapes/Cube`。
- **`transitMeshPath`+`meshTransform`**：公交/火车/飞机这类线路载具的真实网格路径(不给小汽车用，两个字段服务两种外观机制，不要混用)；非空时直接`LoadObject`这个路径当外观，不再按`sizeX/Y/Z`缩放立方体。`meshTransform`只修正这条路径加载出的mesh(缩放+`yawOffsetDegrees`朝向修正)；Actor自身朝向每帧仍由行驶方向决定，两者各管一段。
- 带存档钩子(含命名计数器)，默认不存任何东西，见`common/mod_state.md`。

## 依赖关系

- 依赖：`common/mod_state.h`。
- 被谁依赖：`vehicle_factory.h`创建它；`Core/traffic/vehicle.h`消费它；`Source/Forever/Element/VehicleElement`/`TransitVehicleElement`渲染它；具体实现在`Source/Basic/traffic/vehicle_basic.*`。

## 待办/后续阶段

- 无。
