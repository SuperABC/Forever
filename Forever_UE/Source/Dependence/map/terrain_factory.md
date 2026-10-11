# terrain_factory.h / terrain_factory.cpp

## 职责

`TerrainFactory`是`TerrainMod`的注册表/工厂。

## 关键设计

- **没有`Temp`+`MergeTemp()`两段式注册**——`Map::InitTerrains()`用`ModLoader::
  RegisterConcept<TerrainFactory>`一次性发现/注册所有terrain mod dll（Basic.dll的
  Ocean/Mountain、`Forever_Mod/Empty`的EmptyTerrain等），不存在多个mod DLL并发抢注册的
  场景，两段式隔离没有必要。
- **`Map`是这个Factory实例唯一的长期持有者**——`Source/Core/map/map.h`的`Map`类把
  `TerrainFactory`作为普通成员（不是指针，生命周期跟着`Map`走），不由其它子系统临时代管。

## 依赖关系

- 依赖：`terrain_mod.h`。
- 被谁依赖：`Source/Core/map/map.h`（`Map::terrainFactory`成员）。
