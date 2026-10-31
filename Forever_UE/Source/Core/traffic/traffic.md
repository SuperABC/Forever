# traffic.h / traffic.cpp

## 职责

`Traffic`是Traffic域的聚合入口。`Route`/`Station`两个Mod扩展点目前是空骨架，业务聚合逻辑
留到后续再做；`Vehicle`先落地"能上下车、能开动"这个最小闭环——`Traffic`按name持有全部当前
存在的`Vehicle*`（`unordered_map<string, Vehicle*>`），供UE层上下车时创建/销毁，见
`vehicle.md`。

和`Source/Forever/Framework/ForeverTrafficFrameworkComponent`（UE层Traffic域组件，上下车的
Possess切换逻辑在那边，见其自身注释）不是一回事——`Traffic`不知道UE Actor/Controller的存在，
两者没有互相持有关系。

## 关键设计

- **持有方式**：和`Map`/`Populace`/`Story`一样，直接挂在`AForeverFrameworkActor`身上
  （`Traffic* traffic`成员，生命周期管理方式相同），不是被`UForeverTrafficFrameworkComponent`
  持有。
- **`vehicles`：按name索引，创建失败不入表**：

  ```cpp
  Vehicle* CreateVehicle(const std::string& modId, const std::string& name);
  void DestroyVehicle(const std::string& name);
  Vehicle* FindVehicleByName(const std::string& name) const;
  const std::unordered_map<std::string, Vehicle*>& GetVehicles() const;
  ```

  `CreateVehicle`用`Registry::Get().GetVehicleFactory()`（构造函数里绑定的引用成员，和
  `Society`构造函数绑定`organizationFactory`同一个写法）`new`一个`Vehicle`，
  `Vehicle::IsValid()`为false（`modId`未注册/未在`config.json`的`"vehicle_mods"`里启用）
  就整个丢弃、返回`nullptr`，不会往`vehicles`塞半成品——调用方
  （`UForeverTrafficFrameworkComponent::ToggleVehicle`）据此判断上车是否成功。`name`已存在
  会先`delete`旧的再覆盖（车辆目前都是玩家上车时按自增计数器现取的临时测试名，不要求更强的
  唯一性校验）。析构函数`delete`全部`vehicles`。
- **`Tick`/`ApplyChange`：占位，保持"每个Core域都有这两个方法"的形状一致**：目前仍是空实现。
  `AForeverFrameworkActor::Tick`每帧会挨个调用`map`/`populace`/`society`/`industry`/
  `traffic`/`story`六个域各自的`Tick`，`AForeverFrameworkActor::ApplyChange`（统一的Change
  消费入口）也会把没被它自己处理掉的Change转发给七个域各自的`ApplyChange`——Traffic认识`add_option`/
  `remove_option`(车辆的交互选项)，其余返回false(域自己不打"未实现"警告：`ApplyChange`返回是否认识，七个域都不认识时由`AForeverFrameworkActor::ApplyChange`统一打一条)。等
  Route/Station真正落地、或者Vehicle需要每帧自己做点什么（比如AI车流）时再补内容，详见
  `Source/Forever/Framework/ForeverFrameworkActor.md`"统一的Change消费入口：
  `ApplyChange`"一节。

## 依赖关系

- 依赖：`Core/traffic/vehicle.h`（`Vehicle`，持有所有权）、
  `Dependence/traffic/vehicle_factory.h`（`VehicleFactory`）、`common/registry.h`
  （`Registry::Get().GetVehicleFactory()`）、`Core/common/class.h`（`Time`/`Change`）、
  `Dependence/common/handle.h`（`PostHandle`）。
- 被谁依赖：`Core/common/implement.h`（`PostImplement`构造函数）、
  `Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（持有+`Tick`驱动+
  `ApplyChange`转发）、`Source/Forever/Framework/ForeverTrafficFrameworkComponent.cpp`
  （`ToggleVehicle`调用`CreateVehicle`/`DestroyVehicle`）。

## 待办/后续阶段

- `Route`/`Station`两个Mod扩展点的业务聚合逻辑（当前空骨架）。

## 停站事件与测试代码
- `Traffic::ConsumeStopEvents()`/`SilenceInitialStopEvents()`：汇总各线路的停站/离站事件，见`route.md`。
- `InitParkedVehicles`按停车位的`ParkingSpot::categories`选载具：先在种类里随机选一个，再用`VehicleFactory::GetIdsByCategory(category)`(每个种类只查一次、本函数内缓存)从该种类已启用的车型里随机选一个，**不管是否drivable**；种类为空/该种类没有车型就跳过这个车位。`ParkingRoom`的车位种类是`{"car","bus"}`(相当于原先"一半概率是可驾驶公交"的测试逻辑，现在是数据，不想要公交就从`ParkingRoom::SetProperty`删`"bus"`)；`AirportRoom`有左右两个车位，种类`{"plane"}`，停的是`PlaneVehicle`(可驾驶，`blueprintPath`指向飞机蓝图，见`Forever/Element/AirplaneElement.md`)。
- 车型的种类(`"car"/"bus"/"train"/"plane"`)是每个VehicleMod子类的`static const char* GetCategory()`，经`VehicleFactory::RegisterVehicle`的`CategoryFunc`注册，`Vehicle`构造时向工厂按mod id查一次存成成员(`GetCategory()`)。
