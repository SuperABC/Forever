# traffic.h / traffic.cpp

## 职责

`Traffic`是交通域的聚合入口：按name持有全部当前存在的`Vehicle*`/`Station*`/`Route*`
（三张`unordered_map<string, ...>`），负责开局生成站点、线路、停放车辆和线路车辆，每帧驱动
线路车辆，并消费交通域的`Change`。

和`Source/Forever/Framework/ForeverTrafficFrameworkComponent`（UE层交通域组件，上下车的
Possess切换逻辑在那边）不是一回事——`Traffic`不知道UE Actor/Controller的存在，两者没有互相
持有关系。`Traffic`直接挂在`AForeverFrameworkActor`身上（`Traffic* traffic`成员），和
`Map`/`Populace`/`Story`的生命周期管理方式相同。

## 关键设计

### Init(Map*)的五步（顺序是硬约束）

必须在所有建筑落地之后调用——建筑的`Layout()`/导航图合并已经跑完，这里只读建筑缓存好的数据。
同时必须早于UE侧"生成地形模型"（instantiate线路的铺轨要烘焙进地形，见`route.md`）。

1. `InitStations`：遍历map所有building，对`BuildingMod::stationMod`非空的创建对应`Station`
   （调一次`StationMod::Layout()`）。
2. `InitRoadsideStations`：对每个已注册的`StationMod`类型调`StationFactory::AssignRoads`
   （静态方法，不需要实例）探测要贴哪些路/哪一侧，每条请求创建一个不挂建筑的`Station`，
   坐标取`Map::GetLaneSegmentAt`的真实车道坐标（目前只有公交站用）。站名取请求的`name`(非空)，否则`"Station"+stationCounter`。
3. `InitRoutes`：对每个已注册且启用的`RouteMod`类型创建`Route`，收集`stationType`匹配的
   `Station`(端口快照)交给`Route::Build()`；线路上的extern点由`Route`自己持有，不再创建额外的`Station`。
4. `InitParkedVehicles`：遍历每个房间的每个`ParkingSlot`，**只有`kParkedVehicleFraction`(0.5，`traffic.cpp`顶部常量)的
   车位生成车辆**(`plane`类别不抽，机位全放)——车辆还没和市民绑定，这段随机逻辑等绑定后要整个换掉。选中的车位按
   `ParkingSlot::categories`先随机选一个载具种类，再用`VehicleFactory::GetIdsByCategory(category)`（每个种类只查一次、
   本函数内缓存）从该种类已启用的车型里随机选一个，**不管是否drivable**；种类为空/该种类没有车型就跳过。种类
   是各`VehicleMod`的静态`GetCategory()`经`VehicleFactory::RegisterVehicle`注册的。车通过`Vehicle::BindSlot(room, slotIndex)`
   停进车位(车位记`parkedVehicle`，车记`room`/`parkingSlotIndex`)。
5. `InitTransitVehicles`：按线路名排序后对每条`Route`（车辆编号`Transit<N>`因此与哈希顺序无关）的每条line按`GetLineVehicleCount(lineIdx)`(mod为每条线路单独指定，缺省1辆)创建车辆、
   `SetRouteBinding`、`AddVehicleToLine`。

### 停车位坐标

车位已经是建筑局部坐标(中心+车头朝向，见`map/room.md`"停车位")，`BindSlot`把相对房间中心的偏移和车头朝向
(= 车位开口朝向的反向)写进车的`parking*`字段，UE层`GetParkedPose`再换成世界位姿。

### vehicles表

- `CreateVehicle(modId, name)`用`Registry::Get().GetVehicleFactory()`创建`Vehicle`，
  `Vehicle::IsValid()`为false（`modId`未注册/未在`config.json`的`"vehicle_mods"`启用）就整个
  丢弃、返回`nullptr`，不往表里塞半成品；`name`已存在会先`delete`旧的再覆盖。
  `DestroyVehicle(name)`/`FindVehicleByName`/`GetVehicles()`同理按name索引。
- 析构`delete`全部routes/stations/vehicles。

### Tick

`Tick(currentTime, crossedDay, post)`按`Time::DifferenceInSeconds`累加`elapsedSeconds`
（单调递增，不存在需要补偿的跨天问题；`lastTickTime`不存档，读档后第一帧只记时间），再对每条
`Route`调`Update(elapsedSeconds)`驱动线路车辆。

### 停站事件

`ConsumeStopEvents()`汇总各线路的停靠/离站事件（见`route.md`），`AForeverFrameworkActor::Tick`
在`Tick`之后取走并转发给对应载具的script（`enter_stop`/`leave_stop`）；
`SilenceInitialStopEvents()`让读档后的第一次`Update`不补发。

### ApplyChange

认识`add_option`/`remove_option`（按name找`Vehicle`，调`AddOption`/`RemoveOption`）；返回
"是否认识该类型"，找不到目标静默忽略。`EnterVehicleChange`/`ChangeControlChange`是UE-only效果，
不在这里处理，由`AForeverFrameworkActor::ApplyChange`转发给UE层。七个域都不认识时由
`AForeverFrameworkActor::ApplyChange`统一打"未实现"警告，见`Forever/Framework/ForeverFrameworkActor.md`。

## 路口通行管理

`JunctionTraffic`（`junction.h/.cpp`，见 `junction.md`）和 `Traffic` 没有持有关系：它只依赖 `Map` 的路口与导航图，由 UE 层 `UForeverTrafficFrameworkComponent::GetJunctionTraffic()` 按需建，所以读档（不重跑 `Traffic::Init`）也能用。

## 依赖关系

- 依赖：`traffic/vehicle.h`/`station.h`/`route.h`、`Dependence/traffic/*_factory.h`、
  `common/registry.h`、`common/class.h`（`Time`/`Change`）、`Dependence/common/handle.h`
  （`PostHandle`）、`map/map.h`。
- 被谁依赖：`common/implement.h`（`PostImplement`）、`traffic_save.cpp`、
  `Forever/Framework/ForeverFrameworkActor.h/.cpp`（持有+`Tick`驱动+`ApplyChange`转发）、
  `Forever/Framework/ForeverTrafficFrameworkComponent.cpp`（`CreateVehicle`/`DestroyVehicle`）。

## 待办/后续阶段

- 无特别延后的内容。
