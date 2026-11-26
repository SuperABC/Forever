# roadnet_factory.h / roadnet_factory.cpp

## 职责

`RoadnetFactory`是`RoadnetMod`的注册表/工厂，和其它Factory同一套通用骨架(`registries`/`liveInstances`/`configuredArgs`)。

## 关键设计

- **单选concept(Roadnets/Names)和其它Factory用同一套"启用"语义**(2026-11-25统一)：启用=已注册且id在`configuredArgs`里，`Create`/`CheckRegistered`/`GetRegisteredIds`都只认启用的。"单选"不是Factory的事——一次只应该有一个路网布局方案生效(不是Terrain那种多mod叠加分发)，"用哪一个"由Core从`Config::GetUniqueConceptMod("Roadnets")`取，见`Source/Core/common/config.md`。以前这里另有`SetConfig`/`GetRoadnet`和一张启用标记表，和`configuredArgs`是两份"是否启用"的记录，还要调用方每局手动清空再填，已经删掉。
- **有`IsEnabled`门槛，和其它Factory一样**：没启用的id`CreateRoadnet`返回nullptr、`CheckRegistered`为false、不出现在`GetRegisteredIds`里。
- 其余(全部公开方法`virtual`、`creator`/`deleter`用裸函数指针、`DestroyRoadnet`走注册时的`deleter`、`GetLiveInstances`、`SetModArgs`)与`building_factory.md`同一套理由，不重复展开。

## 依赖关系

- 依赖：`map/roadnet_mod.h`。
- 被谁依赖：`Source/Core/map/map.h`(`Map::roadnetFactory`成员)。
