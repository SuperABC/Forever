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
4. `InitAirportVehicles`：机场的飞机还没有对应的组织，先每个`plane`机位无条件停一架（不挂资产，等以后有机场组织再改）。
   车型用`VehicleFactory::GetIdsByCategory("plane")`随机选，`BindSlot`停进车位。**普通停车位的车不在`Init`里生成**，由
   `AssignVehicles`按人分配（见下）。
5. `InitTransitVehicles`：按线路名排序后对每条`Route`（车辆编号`Transit<N>`因此与哈希顺序无关）的每条line按`GetLineVehicleCount(lineIdx)`(mod为每条线路单独指定，缺省1辆)创建车辆、
   `SetRouteBinding`、`AddVehicleToLine`。

### AssignVehicles：按人分配载具（`Traffic::AssignVehicles`）

不在`Init`里：`Init`在人口/社会之前，分配要用人。框架在"构建社会与产业"最后（`Map::BuildEstateAssets`之后）调用一次
（`ForeverFrameworkActor::EnsureAssetsGenerated`），**读档不调用**（载具和所有者随存档恢复，见`traffic_save.md`）。
- **市民**：每个成年(≥18)市民20%概率尝试；在`citizen->GetBuilding()`（家所在建筑）里找**任何带停车位的房间**（和是不是
  `room_parking`无关）的空闲、`Accepts("car")`的车位（多个时随机一个）；没有就不分配。成功：`VehicleFactory::GetIdsByCategory("car")`
  随机车型，`CreateVehicle`+`BindSlot`，建`VehicleAsset`挂到市民资产列表。
- **组织**：遍历组织所有`Component`的所有房间，收集带停车位的房间；随机1-5次尝试，每次找空闲`Accepts("truck")`车位，
  没有空位就停止；成功则生成truck和`VehicleAsset`挂到组织资产列表。
- 车位的类别标记（`ParkingSlot::categories`）起检查作用：找不到包含要生成的载具类别的空闲车位就当没有空位（目前住宅车位都是
  car、工厂车位都是truck，不会出现类别拒绝）。
- `VehicleAsset`归`Traffic`（`vehicleAssets`），销毁车时同步删除并从所有者列表摘除，见`player/vehicle_asset.md`。

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

## 通勤行程（开车上下班）
`BeginCommute(citizen, finalDest)`：市民当前所在建筑里停着自己的车(`VehicleAsset`、Parked、非线路)、目的地建筑有接受该车类别且有navigationNode的空闲车位、两栋建筑不同，则预订目的车位并登记`CommuteTrip{citizen, vehicle, finalDest, parkOrigin, parkDest, destSlot, phase}`(phase=WalkToCar/Driving)，返回行程；否则返回nullptr(走路)。`FindTrip/FindTripByVehicle/EndTrip/CancelTrip`管理行程，`CancelTrip`在WalkToCar阶段释放预订。行程状态机在UE层(`ForeverPopulaceFrameworkComponent`)，行程记录随traffic段存档(见traffic_save.md)，读档由`ResolveVehicleAssets`还原。

## 通勤统计报告
`Traffic::CommuteReport(populace)`返回若干行文本，UE层写日志：成年人数、有车人数、每种职位(无业记`(none)`)的人数和其中有车人数、开车寻路累计(started)/到达/失败/进行中，以及各种走路原因计数。开局分配车之后和读档恢复后各打一次；每次开车通勤出发(`Commute drive pathfinding`)和到达/失败也各打一行累计。
