# roadnet_factory.h / roadnet_factory.cpp

## 职责

`RoadnetFactory`是`RoadnetMod`的注册表/工厂，阶段3骨架的`registries`/`liveInstances`/
`configuredArgs`原样保留（和`terrain_factory.h`一样没有恢复老工程的`Temp`+`MergeTemp()`两段式
注册），新增`SetConfig`/`GetRoadnet`支撑"单选"语义。

## 关键设计

- **`SetConfig(id, enabled)`+`GetRoadnet()`是单选机制**，不是Terrain那种多mod叠加分发——
  一次只应该有一个路网布局方案生效。`GetRoadnet()`遍历`enabledConfig`返回第一个`enabled==true`
  的id，找不到返回空字符串。谁来调`SetConfig`：`Map::InitRoadnet()`对
  `Config::GetConceptMods("roadnet_mods")`解析出的每个id调一次`SetConfig(id, true)`——按约定
  这个数组应该只配一个roadnet mod id，多配了也不会崩，`GetRoadnet()`只会取到其中之一（不保证
  是哪一个，这是使用方约定问题，不是Factory要校验的事）。
- 其余（`RegisterRoadnet`/`CreateRoadnet`/`DestroyRoadnet`/`CheckRegistered`/`GetRegisteredIds`/
  `SetModArgs`、全部公开方法`virtual`、跨DLL new/delete走deleter）和`terrain_factory.h`同一套
  理由，不重复展开，见`terrain_factory.md`。

## 依赖关系

- 依赖：`roadnet_mod.h`。
- 被谁依赖：`Source/Core/map/map.h`（`Map::roadnetFactory`成员）。

## 待办/后续阶段

- 阶段4：Zone/Building等其余Map域concept迁移时重复"Factory从`ForeverModSubsystem`临时验证代码
  移出、转由`Map`长期持有"这个模式。
