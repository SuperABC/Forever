# ForeverTrafficFrameworkComponent

## 职责

对应旧Framework Actor `Traffic`（C++ Base:`TrafficBase`）。`Route`/`Station`两个域仍然
是空实现，这次先落地"上下车"这一件事——玩家按T键（`Test`动作）时，在当前pawn位置生成
一辆`Vehicle`(Core)+对应的`AVehicleElement`并占有它，或者反过来删掉当前占有的车辆、
恢复到上车前的pawn。

## `RequestToggleVehicle`：T键共用入口

```cpp
static void RequestToggleVehicle(UWorld* world, APlayerController* controller);
```

`AForeverCharacter`和`AVehicleElement`都不方便直接互相知道对方——两者没有继承关系
（见`Element/VehicleElement.md`"为什么不继承AForeverCharacter"一节），也都不是
`AForeverFrameworkActor`的子组件（拿不到`GetOwner()`那条近路）。所以T键的处理函数
（`AForeverCharacter::ToggleVehicle`/`AVehicleElement::ToggleVehicle`）各自只做一件事：
调用这个静态方法。它按`world`找场景里唯一的`AForeverFrameworkActor`
（`UGameplayStatics::GetActorOfClass`），转发给它的`TrafficFramework`组件实例调用真正的
`ToggleVehicle(controller)`。找不到`framework`/`trafficFramework`/`controller`时什么都
不做。

## `ToggleVehicle`：真正的上下车逻辑

```cpp
void ToggleVehicle(APlayerController* controller);
```

这个实例方法是`AForeverFrameworkActor`的子组件，直接`Cast<AForeverFrameworkActor>(
GetOwner())`拿`Traffic*`（和`UForeverStoryFrameworkComponent::ApplyControlChange`同一个
访问方式）。

- **当前`controller->GetPawn()`是`AVehicleElement`→下车**：先记下车辆当前的位置/朝向，
  `Traffic::DestroyVehicle(name)`删掉Core对象，`vehicleElement->Destroy()`删Actor，
  **把`previousPawn`的位置/朝向摆到刚才记下的车辆位置**（用户明确要求：下车不是传送回
  上车前的原位置，人应该出现在车所在的地方），重新显示+开碰撞，`controller->
  Possess(previousPawn)`。
- **否则→上车**：`Traffic::CreateVehicle("vehicle_basic", name)`（`modId`这一阶段先
  写死`"vehicle_basic"`——`VehicleBasic`测试车型，将来做车辆种类选择时改成参数；`name`
  按`vehicleCounter`自增生成，如`"TestVehicle0"`）创建失败（`vehicle_basic`没有在
  `config.json`的`"vehicle_mods"`里启用）打一条warning直接返回，不影响当前占有状态。
  成功则在当前pawn位置`SpawnActor<AVehicleElement>`，`Init(vehicle, currentPawn)`存住
  `previousPawn`+加载mesh，隐藏+关掉原pawn的碰撞，`controller->Possess(vehicleElement)`。

## 依赖关系

- 依赖：`Source/Forever/Framework/ForeverFrameworkActor.h`（`GetOwner()`拿`Traffic*`）、
  `Source/Core/traffic/traffic.h`/`vehicle.h`（`CreateVehicle`/`DestroyVehicle`）、
  `Source/Forever/Element/VehicleElement.h`（`AVehicleElement`）。
- 被谁依赖：`AForeverCharacter::ToggleVehicle`、`AVehicleElement::ToggleVehicle`
  （两者都只调用静态方法`RequestToggleVehicle`）。
