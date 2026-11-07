# Registry

## 职责

持有全部21个concept(对照表见`common/loader.md`)的`ModLoader`+21个Factory，把"mod dll的发现/注册"和
"每个持有者(`Map`/`Populace`/`Society`/`Story`/`Industry`/`Traffic`/`Player`)每次开局实际生成一遍对象、
结束时释放、下次开局重新生成"这两件事在生命周期上彻底分开：dll只加载一次，持有者只持有Factory的引用。
`common/`是Core里各域都可以依赖的顶层位置(和`implement.h`的`PostHandle`实现同款定位)。

## 关键设计

- 用函数内static实现懒汉单例(`Get()`，首次调用时构造并对当时`Config::GetMods()`的dll跑一遍
  `RegisterMods`)，不挂在任何UE Subsystem的生命周期上——Core必须保持engine-agnostic(见CONVENTIONS.md)，
  纯C++单例足够满足"全局只注册一次"：`Core.lib`静态链接进`UnrealEditor-Forever.dll`，只要这个dll不被
  卸载/热重载，函数内static对象活得比任何一次PIE开的持有者都长。
- 21个Factory对"已注册mod id列表"之外没有跨局累积的状态——`CreateXxx`/`DestroyXxx`只是按注册时存的
  creator/deleter函数指针创建/销毁一个实例，所以多局共用同一个Factory实例是安全的，不需要每局重新注册。
- 各持有者(`Map`/`Populace`/`Society`/`Story`/`Industry`/`Traffic`/`Player`及`Asset`/`Phone`等)不自己持有
  Factory和`ModLoader`，在构造函数初始化列表里把引用成员绑定到这个单例对应的Factory。引用成员的声明顺序
  必须和初始化列表顺序一致(C++按声明顺序初始化)。
- **`modLoader`必须活得比21个Factory一样长**：Factory存的creator/deleter函数指针指向mod dll的代码段，
  句柄一旦被`FreeLibrary`这些指针就悬空；它们都是这个单例的成员，生命周期天然一致。
- **注册(`RegisterConcept`)和参数(`SetModArgs`)是两个不同调用频率的操作**：前者只要mod dll没变就只需要
  跑一次；后者把`config.json`的`"<concept>_mods"`数组解析出的id->参数字符串表交给Factory，不同局可能指向
  不同的config文件，每次都可能不一样。所以`SetModArgs`单独暴露成`ReloadModArgs()`(不触发任何dll重新扫描)；
  构造函数末尾也调一次兜底，保证`Get()`刚拿到实例时Factory就有参数可用。读档恢复/换config之后
  (`SaveGame`)也会单独调`ReloadModArgs()`。
- **`RefreshMods()`：增量加载新增的dll**。每次真正开局(`AForeverFrameworkActor::BeginPlay`)调用：只对
  `loadedDllPaths`里没有的路径补做21个concept的`RegisterConcept`(`ModLoader`按路径缓存句柄)，然后
  `ReloadModArgs()`。这样退出游戏回菜单、在配置界面新增dll路径之后再开局，新dll的mod也能用。
  - 已加载的dll**不卸载**：从配置里移除的dll保持加载，但它的mod id不在`configuredArgs`里，
    `Factory::IsEnabled`判为未启用，照样是禁用状态。
  - 加载失败的路径(`IsLoaded`为false)不记进`loadedDllPaths`，下一局会重试。

## `CheckModRegistered(id)`：主线剧情`.script`的`mod_dependences`校验用

对全部21个Factory各查一次`CheckRegistered(id)`，只要有任意一个认得这个id就返回`true`——mod id不带concept
前缀，不能靠字符串猜它属于哪个concept(`"building_clean"`完全可能是个Job mod)，所以不按concept分流，直接
全局OR匹配，供`AForeverFrameworkActor::ValidateMainStoryDependencies()`校验用，见`Core/story/script.md`。

**"注册"和"启用"是两件不同的事**：`configuredArgs`(`SetModArgs`存的"id->参数字符串"表，来自`config.json`
每个`"<concept>_mods"`数组)不只是提供参数字符串——19个Factory(除`RoadnetFactory`/`NameFactory`，见下)的
`CreateXxx`/`CheckRegistered`/`GetRegisteredIds`同时认`registries`(DLL有没有声明这个id)和`configuredArgs`
(`config.json`对应数组里有没有列出这个id)两张表：**没在`config.json`列出的id视为未启用**，`CreateXxx`拒绝
创建，`GetRegisteredIds()`也不会枚举到它。每个Factory有一个私有`IsEnabled(id)`helper统一判断，
`OrganizationFactory`/`SchedulerFactory`/`ZoneFactory`/`BuildingFactory`额外的`GetPower`/`Assign`/
`RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`这类"不需要实例的静态查询"方法同样接入这个判断。

**`config.json`里所有在用的`Basic.dll`默认实现id必须列在对应的`"<concept>_mods"`数组里**，否则会被视为
未启用而"消失"——`Populace::InitNames()`用到的`"name_chinese"`最严重，不列的话会直接`THROW_EXCEPTION`导致
游戏进不去，见`populace.md`"InitNames"一节。

**`RoadnetFactory`/`NameFactory`是两个例外，走的是"单选"模式，不是"启用即认领"**：一次只应该有一个路网
布局/取名算法生效(不是Terrain那种按`GetPriority()`多mod叠加)，`CreateXxx`/`CheckRegistered`/
`GetRegisteredIds`保持不看`configuredArgs`的纯`registries`查询，"选哪一个生效"完全由调用方(`Map::InitRoadnet()`/
`Populace::InitNames()`)通过`SetConfig(id, true)`+`GetRoadnet()`/`GetName()`决定；`Config`层面另有
`NormalizeUniqueConcepts()`保证这两个`"<concept>_mods"`数组里至多一项，见`config.md`。

## 依赖关系

- 依赖：`common/loader.h`(`ModLoader`)、`common/config.h`(`Config::GetMods`/`GetConceptMods`)、21个
  `*_factory.h`(均为`Source/Dependence`头文件，`map`/`player`/`populace`/`society`/`story`/`industry`/
  `traffic`七个子目录各自的factory头)。
- 被谁依赖：Core各域持有者的构造函数(`Map`/`Populace`/`Society`/`Story`/`Industry`/`Traffic`/`Player`/
  `Asset`/`Phone`)取Factory引用；`SaveGame`(`save.cpp`，读档时`ReloadModArgs`)；
  `Source/Forever/Framework/ForeverFrameworkActor.cpp`(`BeginPlay()`里调`RefreshMods()`)。

## 待办/后续阶段

- 无。
