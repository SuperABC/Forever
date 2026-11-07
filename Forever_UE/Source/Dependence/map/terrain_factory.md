# terrain_factory.h / terrain_factory.cpp

## 职责

`TerrainFactory`是`TerrainMod`(Terrain(地形))的注册表/工厂：负责创建、销毁、枚举已启用的地形类型。

## 关键设计

- 通用骨架与`building_factory.md`相同：全部公开方法`virtual`、`creator`/`deleter`用裸函数指针、`DestroyTerrain`走注册时的`deleter`、"启用"=已注册且在`terrain_mods`里列出(`IsEnabled`)、`SetModArgs`/`GetLiveInstances`，不重复展开。没有`Temp`+`MergeTemp()`两段式注册：`Map::InitTerrains()`用`ModLoader::RegisterConcept<TerrainFactory>`一次性发现/注册所有terrain mod dll，不存在并发抢注册。
- **`Map`是这个Factory实例唯一的长期持有者**：`Map`把`TerrainFactory`作为普通成员(不是指针，生命周期跟着`Map`走)。

## 依赖关系

- 依赖：`map/terrain_mod.h`。
- 被谁依赖：`Source/Core/map/map.h`(`Map::terrainFactory`成员)。

## 待办/后续阶段

- 无。
