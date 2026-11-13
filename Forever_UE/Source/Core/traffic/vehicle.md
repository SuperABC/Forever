# vehicle.h / vehicle.cpp

## 职责

`Vehicle`：一辆车的实体，独占持有一个`VehicleMod*`（`Dependence/traffic/vehicle_mod.h`
定义，具体车型在`Basic/traffic`）——完全照抄`Job`持有`JobMod`的模式
（见`society/job.h/.md`），构造函数传入`VehicleFactory*`+id，内部调
`factory->CreateVehicle(id)`拿到`VehicleMod*`，析构时`factory->DestroyVehicle(mod)`。
**`Vehicle`由`Traffic`持有所有权**（`unordered_map<string, Vehicle*>`，按name索引，见
`traffic.md`）。

主要字段：

- `name`：`Traffic`按这个索引，也是UE侧`Traffic::DestroyVehicle(name)`的key。
- `mod`：转发`GetType()`/`GetCategory()`/`IsDrivable()`/`IsBoardable()`/`GetCapacity()`/
  `GetSize()`/`GetBlueprintPath()`/`GetTransitMeshPath()`/网格变换/下车偏移等只读属性
  （`mod`为空时返回空/默认值，见下"创建失败"）。种类`GetCategory()`在构造时向工厂按modId查一次
  （注册的静态`GetCategory()`），查不到兜底`"car"`。
- `x`/`y`/`z`/`yaw`/`pitch`：当前世界位姿，由UE侧`AVehicleElement::Tick`或`Route::Update`
  通过`SetTransform`/`SetPitch`写回；`pitch`只有沿坡道/隧道行驶的线路载具才非零，纯运行时不存档。
- `script`/`options`：自己的`Script`（按`mod->scriptModName`创建、按`mod->milestoneNames`加载
  milestone）和交互选项列表（照抄`Citizen`），驱动"靠近弹出上车选项"。
- `room`+停车位局部坐标+旋转：`Traffic::InitParkedVehicles`生成时`SetParking`一次，只服务
  于"生成时摆在哪"。
- `route`/`routeLine`：线路公交由`Traffic::InitTransitVehicles`调`SetRouteBinding`设置，
  普通小汽车恒为`nullptr`；UE层据此决定生成`AVehicleElement`还是`ATransitVehicleElement`。

**不存`driver`/`owner`字段**：这一阶段明确不考虑车辆和房间/停车位的关系，"谁在开这辆车"
这份信息由UE侧`AVehicleElement::previousPawn`自己保留（上车前被占有的pawn，下车时要
恢复谁），不需要`Vehicle`重复记一份。

## 外观资源路径：`GetBlueprintPath()`转发`mod->blueprintPath`

车辆的可见外观（骨骼网格、四个轮子的骨骼名）不是`VehicleMod`直接存的数据，而是一个继承
`AVehicleElement`的蓝图(Blueprint)类——`VehicleMod`只存这个蓝图类的资源路径字符串
（`blueprintPath`，和`JobMod::scriptModName`同一个"mod自己的字段，Core只读转发"模式，见
`vehicle_mod.h`）。`SedanVehicle`构造函数里写死指向：

```cpp
blueprintPath = "/Game/3rdParty/Cars_for_Arcade_Demolition_Racing_Games/Actor/BP_VehicleElement.BP_VehicleElement_C";
```

`Vehicle::GetBlueprintPath()`只读转发这个字段。`UForeverTrafficFrameworkComponent::
ToggleVehicle`在生成车辆**之前**（这时已经通过`traffic->CreateVehicle(...)`拿到了具体的
`Vehicle`实例）调用这个方法+`LoadClass<AVehicleElement>()`拿到具体该用哪个蓝图类，再拿这个
类去`SpawnActor`——`UChaosWheeledVehicleMovementComponent`真正要求"必须在构造函数阶段就
绑好"的是骨骼网格本身，而"选用哪个`UClass`去`SpawnActor`"这件事发生在构造函数执行**之前**，
此时`Vehicle`实例已经存在，天然没有时序问题。骨骼网格/轮子骨骼名的具体设置在蓝图编辑器里
完成，不需要C++代码，详见`Source/Forever/Element/VehicleElement.md`"多车型共存"一节。

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

## on_route脚本变量
车辆Script创建时写`on_route=0`，`SetRouteBinding`(及读档恢复线路绑定)置1。同一个VehicleMod既能跑线路又能停车位(公交)，车辆script靠`$$self.on_route`区分要不要提供"驾驶"选项，见`Resource/Story/vehicle_bus.script`。

## 脚本可为空
`VehicleMod::scriptModName`默认`""`，和`milestoneNames`都空则`GetScript()==nullptr`(见`Core/story/script.md`MakeScript)；`AVehicleElement`没有脚本且没有选项时不挂弹选项的触发盒，之后`add_option`出现选项时`RefreshOptions`补建。市民同理(Scheduler脚本为空且无选项不建盒)。Job/Organization/Scheduler同样可为空。

## 当前所在房间：GetCurrentRoom / SetCurrentRoom

运行时状态，不进存档。`SetParking`时初始化成停车位房间；测试导航出发时置空、到达时设成目的房间；玩家上车
(`ApplyEnterVehicle`)时置空。`Map::PlanVehicleRoute`据此决定起点是房间导航节点还是路网上最近的车道。读档后为
空（当作在室外）。

## physicsDriven
- `SetPhysicsDriven/IsPhysicsDriven`：线路公交处于物理段（近处由UE物理载具+Controller驱动，或已脱离时刻表）时为真，
  `Route::Update`对它跳过时刻表驱动和停站事件。运行时状态，不进存档。

## Core行驶推算（BeginDrive/AdvanceDrive）

和`Citizen::BeginWalk/AdvanceWalk`对称：没有Actor的车由UE层给一条路线（UE单位cm、速度cm/s），Core按`nowSeconds`沿折线推算位置并写`Transform`（含yaw），走完`SetCurrentRoom(目的房间)`并返回true。`GetRemainingDrive`取从当前位置起的剩余路径，`EndDrive`取消。纯运行时，不进存档。用法见`Forever/Framework/ForeverTrafficFrameworkComponent.md`。
