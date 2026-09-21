# Registry

## 职责

持有全部20个concept(对照表见`common/loader.md`——map域6个：terrain/roadnet/zone/building/
component/room；player域3个：asset/app/puzzle；populace域2个：name/scheduler；society域2个：
job/organization(Calendar已经不再需要，整体删掉了)；story域1个：script；industry域3个：
product/storage/manufacture；traffic域3个：route/station/vehicle)的`ModLoader`+20个Factory，
把"mod dll的发现/注册"和
"每个持有者(`Map`/`Populace`/`Story`……)每次开局实际生成一遍对象、结束时释放、下次开局重新
生成"这两件事在生命周期上彻底分开：注册只在整个UE进程生命周期里发生一次。**注册**
(`RegisterConcept`)和**参数**(`SetModArgs`)进一步拆成了两个不同调用频率的操作，见下面
"SetModArgs的调用频率"一节。

## 关键设计

- **前身是`Source/Core/map/mod_registry.h`**：一开始只搬了map域6个concept，因为当时只有
  `Map`暴露了"每次开局重新注册mod dll"这个问题。后来发现`Populace`(`nameFactory`)、
  `Story`(`scriptFactory`)各自也持有一份自己的`ModLoader`+Factory、犯一样的错误，被要求
  扩大到全部concept、搬到`common/`(不再是"map域自己的东西")、改名`Registry`。
- 用函数内static实现懒汉单例(`Get()`)，不挂在任何UE Subsystem的生命周期上——Core必须保持
  engine-agnostic(见CONVENTIONS.md)，纯C++单例足够满足"全局只注册一次"：`Core.lib`静态
  链接进`UnrealEditor-Forever.dll`，只要这个dll不被卸载/热重载，函数内static对象活得比
  任何一次PIE开的`Map`/`Populace`/`Story`都长。
- 20个Factory对"已注册mod id列表"之外没有跨局累积的状态——`CreateXxx`/`DestroyXxx`只是按
  注册时存的creator/deleter函数指针创建/销毁一个实例，和调用了多少次、上一局有没有开过
  完全无关，所以多局共用同一个Factory实例是安全的，不需要每局重新注册来"重置"什么。
- `Map`/`Populace`/`Story`不再自己持有对应的Factory和`ModLoader`，改成引用成员绑定到这个
  单例(构造函数初始化列表里绑定)：`Map`的`terrainFactory`/`roadnetFactory`/`zoneFactory`/
  `buildingFactory`/`roomFactory`/`componentFactory`，`Populace`的`nameFactory`，`Story`的
  `scriptFactory`。引用成员的声明顺序必须和初始化列表顺序一致(C++按声明顺序初始化)。
- **SetModArgs的调用频率**：最初`SetModArgs`和`RegisterConcept`一起只在构造函数里跑一次，
  被指出这是错的——`RegisterConcept`(dll发现/加载/把creator/deleter函数指针记进Factory)
  只要mod dll本身没变就真的只需要跑一次，但`SetModArgs`(把这次`config.json`
  `"<concept>_mods"`数组解析出的id->参数字符串表交给Factory)不是：不同局可能指向不同的
  config文件，参数因此也可能不一样，"整个UE进程只算一次"对`SetModArgs`不成立。现在拆成
  两半：构造函数只调`RegisterConcept`(先调一次`ReloadModArgs()`兜底，保证`Get()`刚拿到
  实例时Factory就有参数可用)，`SetModArgs`挪进公开方法`ReloadModArgs()`，由调用方
  (`AForeverFrameworkActor::BeginPlay()`，在7个域各自的`Ensure*Generated()`之前统一调
  一次，不属于任何单个域)在每次真正开局、确认这一局要用的config已经被`Config::ReadConfig`
  读进内存之后单独调用，不会触发任何dll重新扫描。
- 目前只有上面这8个Factory真的被某个Core类引用——剩下12个
  (asset/app/puzzle/scheduler/job/organization/product/storage/manufacture/
  route/station/vehicle)对应的domain(player的物件系统、society、industry、traffic)这次
  还是空骨架，没有任何Core类创建对应实例。提前在`Registry`里注册好这12个是为了让20个
  concept的注册入口从一开始就集中在一个地方——等对应domain落地时，直接从`Registry::Get()`
  拿引用即可，不需要再回来改这个类或重新养成"自己持有ModLoader"的习惯。

## `CheckModRegistered(id)`：主线剧情`.script`的`mod_dependences`校验用（新增）

对全部20个Factory各查一次`CheckRegistered(id)`，只要有任意一个认得这个id就返回
`true`——mod id不带concept前缀，不能靠字符串猜它属于哪个concept
（`"building_clean"`完全可能是个Job mod），所以不按concept分流，直接全局OR匹配，供
`AForeverFrameworkActor::ValidateMainStoryDependencies()`校验用，见
`Core/story/script.md`"主线剧情.script新增三个顶层字段"一节。

**这次同时把"注册"和"启用"变成了两件真正不同的事**：`configuredArgs`（`SetModArgs`
存的"id->参数字符串"表，来自`config.json`每个`"<concept>_mods"`数组）以前只提供参数
字符串，不管一个id列不列在数组里，只要DLL声明了这个id，`CreateXxx(id)`就会成功创建
它——这条阶段3早期约定被发现没法表达"mod依赖关系"（想校验"某个mod是不是真的在用"，
但"已注册"和"已启用"混成一件事，永远是true），这次改成18个Factory（除`RoadnetFactory`
/`NameFactory`——这两个走的是"单选"模式，见下）的`CreateXxx`/`CheckRegistered`/
`GetRegisteredIds`都同时认`registries`（DLL有没有声明这个id）和`configuredArgs`
（`config.json`对应数组里有没有列出这个id）两张表——**没在`config.json`列出的id现在
视为未启用**，`CreateXxx`拒绝创建，`GetRegisteredIds()`也不会再枚举到它。每个Factory
新增一个私有`IsEnabled(id)`helper统一判断逻辑，`OrganizationFactory`/
`SchedulerFactory`/`ZoneFactory`/`BuildingFactory`额外的`GetPower`/`Assign`/
`RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`这类"不需要实例的静态查询"方法一并
接入这个判断。

**`RoadnetFactory`/`NameFactory`是两个例外，走的是"单选"模式，不是"启用即认领"**：
一次只应该有一个路网布局/取名算法生效（不是Terrain那种按`GetPriority()`多mod叠加），
`CreateXxx`/`CheckRegistered`/`GetRegisteredIds`保持不看`configuredArgs`的纯
`registries`查询，"选哪一个生效"完全由调用方（`Map::InitRoadnet()`/`Populace::
InitNames()`）通过`SetConfig(id, true)`+`GetRoadnet()`/`GetName()`决定——`config.json`
数组列出哪个id就选哪个，两者都没有额外的`enabledConfig`之外的启用判断。`NameFactory`
最初也按上面18个Factory那样接入过`IsEnabled`，后来发现Name这个concept的语义（"唯一
一个当前生效"）和`RoadnetFactory`完全一样，改成照抄`RoadnetFactory`的模式，不是照抄
另外18个Factory的"启用即认领、可以有多个同时生效"模式，见`populace.md`"InitNames"
一节。

这条改动牵扯到`config.json`里已经在用、但之前从未被列出的一大批`Basic.dll`默认实现
（`chinese`/`ocean`/`mountain`/`zone_residence`/`building_residence`/`shop`/
`factory`/`component_residence`/`shop`/`factory`/`room_residence`/`shop`/
`warehouse`/`parking`/`factory`/`scheduler_basic`/`job_shop_saler`/
`organization_shop`/`script_basic`），改完必须同步把这些id补进`config.json`对应
数组，否则这些默认内容会全部"消失"（`Populace::InitNames()`用到的`"chinese"`最严重，
不补的话直接`THROW_EXCEPTION`导致整个游戏进不去，见`populace.md`"InitNames"一节）。

## 依赖关系

- 依赖：`common/loader.h`(`ModLoader`)、`common/config.h`(`Config::GetMods`/
  `GetConceptMods`)、20个`*_factory.h`(均为`Source/Dependence`头文件，`map`/`player`/
  `populace`/`society`/`story`/`industry`/`traffic`七个子目录各自的factory头)。
- 被谁依赖：`Source/Core/map/map.cpp`(`Map`构造函数绑定6个Factory引用成员)、
  `Source/Core/populace/populace.cpp`(`Populace`构造函数绑定`nameFactory`)、
  `Source/Core/story/story.cpp`(`Story`构造函数绑定`scriptFactory`)、
  `Source/Forever/Framework/ForeverFrameworkActor.cpp`(`BeginPlay()`里、7个
  `Ensure*Generated()`之前调`Registry::Get().ReloadModArgs()`，每次真正开局刷新一遍
  参数表)。

## 待办/后续阶段

- society/industry/traffic三个域这次还是空骨架(`AForeverFrameworkActor::
  EnsureSocietyGenerated()`/`EnsureIndustryGenerated()`/`EnsureTrafficGenerated()`里
  `new Society()`/`new Industry()`/`new Traffic()`只是占位)，
  player域也只迁移了全局时钟(见`player.md`)。等这些域真正落地、需要创建mod实例时，直接
  从`Registry::Get()`拿对应Factory引用即可，不需要再改这个类。
