# traffic.h / traffic.cpp

阶段4-3：`Route`/`Station`两个Mod扩展点仍然是空骨架，业务聚合逻辑留到后续再做（见
`PHASE4_PLAN.md`）；`Vehicle`这一份先落地"能上下车、能开动"这个最小闭环——`Traffic`按
name持有全部当前存在的`Vehicle*`（`unordered_map<string, Vehicle*>`），供UE层上下车时
创建/销毁，见`vehicle.md`。

和`Source/Forever/Framework/ForeverTrafficFrameworkComponent`（UE层Traffic域组件，
上下车的Possess切换逻辑在那边，见其自身注释）不是一回事——`Traffic`不知道UE
Actor/Controller的存在，两者没有互相持有关系。

## 持有方式

和`Map`/`Populace`/`Story`一样，直接挂在`AForeverFrameworkActor`身上（`Traffic* traffic`
成员，生命周期管理方式相同），不是被`UForeverTrafficFrameworkComponent`持有。

## `vehicles`：按name索引，创建失败不入表

```cpp
Vehicle* CreateVehicle(const std::string& modId, const std::string& name);
void DestroyVehicle(const std::string& name);
Vehicle* FindVehicleByName(const std::string& name) const;
const std::unordered_map<std::string, Vehicle*>& GetVehicles() const;
```

`CreateVehicle`用`Registry::Get().GetVehicleFactory()`（构造函数里绑定的引用成员，和
`Society`构造函数绑定`organizationFactory`同一个写法）`new`一个`Vehicle`，`Vehicle::
IsValid()`为false（`modId`未注册/未在`config.json`的`"vehicle_mods"`里启用）就整个
丢弃、返回`nullptr`，不会往`vehicles`塞半成品——调用方（`UForeverTrafficFrameworkComponent::
ToggleVehicle`）据此判断上车是否成功。`name`已存在会先`delete`旧的再覆盖（这一阶段
车辆都是玩家上车时按自增计数器现取的临时测试名，不要求更强的唯一性校验）。

析构函数`delete`全部`vehicles`。

## `Tick`/`ApplyChange`：占位，保持"每个Core域都有这两个方法"的形状一致

这两个方法目前仍是空实现。`AForeverFrameworkActor::Tick`每帧会挨个调用
`map`/`populace`/`society`/`industry`/`traffic`/`story`六个域各自的`Tick`，
`AForeverFrameworkActor::ApplyChange`（统一的Change消费入口）也会把没被它自己
处理掉的Change转发给这六个域各自的`ApplyChange`——目前没有任何Change子类是Traffic域
自己认识、需要处理的，`ApplyChange`因此是空实现，也**不打"未实现"警告**（只有
`Story::ApplyChange`保留那条兜底警告，避免六个域各打一遍重复日志）。等Route/Station
真正落地、或者Vehicle需要每帧自己做点什么(比如AI车流)时再补内容，详见
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
