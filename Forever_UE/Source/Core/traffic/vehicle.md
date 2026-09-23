# vehicle.h / vehicle.cpp

## 职责

`Vehicle`：一辆车的实体，独占持有一个`VehicleMod*`（`Dependence/traffic/vehicle_mod.h`
定义，目前唯一的具体类型是测试车型`VehicleBasic`）——完全照抄`Job`持有`JobMod`的模式
（见`society/job.h/.md`），构造函数传入`VehicleFactory*`+id，内部调
`factory->CreateVehicle(id)`拿到`VehicleMod*`，析构时`factory->DestroyVehicle(mod)`。
**`Vehicle`由`Traffic`持有所有权**（`unordered_map<string, Vehicle*>`，按name索引，见
`traffic.md`）。

这是阶段4-3"能上下车、能开动"这个最小闭环的产物，字段刻意保持最少：

- `name`：`Traffic`按这个索引，也是UE侧`Vehicle::GetName()`/
  `Traffic::DestroyVehicle(name)`的key。
- `mod`：转发`GetType()`（`mod`为空时返回空字符串——`mod`为空说明`CreateVehicle`本身
  失败，见下"创建失败"一节）。
- `x`/`y`/`z`/`yaw`：当前世界坐标+朝向，由UE侧`AVehicleElement::Tick`每帧
  `SetTransform`写回，供将来Traffic/Room等系统查询用——这一阶段没有任何系统会去读它，
  纯粹是为了不丢这份信息。

**不存`driver`/`owner`字段**：这一阶段明确不考虑车辆和房间/停车位的关系，"谁在开这辆车"
这份信息由UE侧`AVehicleElement::previousPawn`自己保留（上车前被占有的pawn，下车时要
恢复谁），不需要`Vehicle`重复记一份。

## 外观资源路径：`GetBlueprintPath()`转发`mod->blueprintPath`

车辆的可见外观（骨骼网格、四个轮子的骨骼名）不是`VehicleMod`直接存的数据，而是**一个
继承`AVehicleElement`的蓝图(Blueprint)类**——`VehicleMod`只存这个蓝图类的资源路径
字符串（`blueprintPath`，和`JobMod::scriptModName`同一个"mod自己的字段，Core只读
转发"模式，见`vehicle_mod.h`）。`VehicleBasic`构造函数里写死指向：

```cpp
blueprintPath = "/Game/Blueprint/Vehicle/BP_VehicleBasic.BP_VehicleBasic_C";
```

`Vehicle::GetBlueprintPath()`只读转发这个字段。`UForeverTrafficFrameworkComponent::
ToggleVehicle`在生成车辆**之前**（这时已经通过`traffic->CreateVehicle(...)`拿到了
具体的`Vehicle`实例）调用这个方法+`LoadClass<AVehicleElement>()`拿到具体该用哪个
蓝图类，再拿这个类去`SpawnActor`——这是为什么这次能通过`Vehicle`正常转发（不像车身
骨骼网格早前一度需要绕过`Vehicle`直接读`VehicleMod`）：`UChaosWheeledVehicleMovementComponent`
真正要求"必须在构造函数阶段就绑好"的是骨骼网格本身，而"选用哪个`UClass`去
`SpawnActor`"这件事发生在构造函数执行**之前**，此时`Vehicle`实例已经存在，天然没有
时序问题。骨骼网格/轮子骨骼名的具体设置在蓝图编辑器里完成，不需要C++代码，详见
`Source/Forever/Element/VehicleElement.md`"多车型共存"一节。

## 创建失败：`IsValid()`

`Vehicle`构造函数不保证`mod`非空——`id`对应的车型没有被注册，或者注册了但没有在
`config.json`的`"vehicle_mods"`数组里启用（`VehicleFactory::IsEnabled`检查，见
`vehicle_factory.h`），`CreateVehicle`都会返回`nullptr`。`Vehicle::IsValid()`
（`return mod != nullptr;`）供`Traffic::CreateVehicle`创建完之后立刻检查，失败就整个
丢弃这个`Vehicle`，不会把半成品塞进`vehicles`表，见`traffic.md`。

## 依赖关系

- 依赖：`Dependence/traffic/vehicle_mod.h`/`vehicle_factory.h`（`VehicleMod`/
  `VehicleFactory`）。
- 被谁依赖：`Traffic`（持有所有权，`CreateVehicle`/`DestroyVehicle`）、
  `Source/Forever/Framework/ForeverTrafficFrameworkComponent.cpp`
  （`ToggleVehicle`用`GetBlueprintPath()`决定`SpawnActor`哪个类）、
  `Source/Forever/Element/VehicleElement.h/.cpp`（`AVehicleElement`不持有所有权的
  引用，每帧`SetTransform`写回）。
