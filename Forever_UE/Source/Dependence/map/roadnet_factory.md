# roadnet_factory.h / roadnet_factory.cpp

## 职责

`RoadnetFactory`是`RoadnetMod`的注册表/工厂，在通用骨架(`registries`/`liveInstances`/`configuredArgs`)之上多了`SetConfig`/`GetRoadnet`，支撑"单选"语义。

## 关键设计

- **`SetConfig(id, enabled)`+`GetRoadnet()`是单选机制**，不是Terrain那种多mod叠加分发——一次只应该有一个路网布局方案生效。`GetRoadnet()`遍历`enabledConfig`返回第一个`enabled==true`的id，找不到返回空字符串。`Map::InitRoadnet()`对`Config::GetConceptMods("roadnet_mods")`解析出的每个id调一次`SetConfig(id, true)`；按约定这个数组只配一个roadnet mod id，多配了不会崩，但`GetRoadnet()`取到哪一个不保证(使用方约定，Factory不校验)。
- **和其它Factory不同，没有`IsEnabled`门槛**：`CreateRoadnet`/`CheckRegistered`/`GetRegisteredIds`只看是否已注册，不看`configuredArgs`；`CreateRoadnet`在`configuredArgs`里找不到参数时传空字符串给`creator`。"启用"由`SetConfig`单独表达。
- 其余(全部公开方法`virtual`、`creator`/`deleter`用裸函数指针、`DestroyRoadnet`走注册时的`deleter`、`GetLiveInstances`、`SetModArgs`)与`building_factory.md`同一套理由，不重复展开。

## 依赖关系

- 依赖：`map/roadnet_mod.h`。
- 被谁依赖：`Source/Core/map/map.h`(`Map::roadnetFactory`成员)。

## 待办/后续阶段

- 无。
