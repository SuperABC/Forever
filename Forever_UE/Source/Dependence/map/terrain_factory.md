# terrain_factory.h / terrain_factory.cpp

## 职责

`TerrainFactory`是`TerrainMod`的注册表/工厂，Terrain是阶段4-1第一个从`Dependence/README.md`
共用文档独立出来的concept。内容和阶段3的骨架**完全没有变化**——单阶段`registries`/
`liveInstances`/`configuredArgs`设计已经够用，业务接口的补齐只发生在`TerrainMod`
（见`terrain_mod.md`），`TerrainFactory`本身不需要跟着变。

## 关键设计

- **没有恢复老工程的`Temp`+`MergeTemp()`两段式注册**——`Source/Dependence/README.md`已经
  说明这是阶段3的有意简化，`Map::InitTerrains()`用`ModLoader::RegisterConcept<TerrainFactory>`
  一次性发现/注册所有terrain mod dll（Basic.dll的Ocean/Mountain、`Forever_Mod/Empty`的
  EmptyTerrain等），不存在多个mod DLL并发抢注册的场景，两段式隔离没有必要。
- **`Map`是这个Factory实例唯一的长期持有者**——`Source/Core/map/map.h`的`Map`类把
  `TerrainFactory`作为普通成员（不是指针，生命周期跟着`Map`走），`Source/Forever/Mod/
  ForeverModSubsystem.cpp`不再临时代管`TerrainFactory`（它的验证代码已经删掉Terrain这一块），
  这是`Core/common/loader.md`"阶段4:Core会开始出现真正的领域系统类如Map/Story，这些类会持有
  对应的Factory实例"这条待办第一次真正落地。

## 依赖关系

- 依赖：`terrain_mod.h`。
- 被谁依赖：`Source/Core/map/map.h`（`Map::terrainFactory`成员）。

## 待办/后续阶段

- 阶段4：Zone/Building/Roadnet等其余Map域concept迁移时会重复这个模式——各自的Factory从
  `ForeverModSubsystem`的临时验证代码里移出，转由`Map`长期持有。
