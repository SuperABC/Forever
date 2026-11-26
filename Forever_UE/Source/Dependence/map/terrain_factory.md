# terrain_factory.h / terrain_factory.cpp

## 职责

`TerrainFactory`是`TerrainMod`(Terrain(地形))的注册表/工厂：负责创建、销毁、枚举已启用的地形类型。

## 关键设计

- 通用骨架与`building_factory.md`相同：全部公开方法`virtual`、`creator`/`deleter`用裸函数指针、`DestroyTerrain`走注册时的`deleter`、"启用"=已注册且在`terrain_mods`里列出(`IsEnabled`)、`SetModArgs`/`GetLiveInstances`，不重复展开。
- **实例由`Registry`单例持有**(`Source/Core/common/registry.h`)，`Map`在构造时取引用(`Map::terrainFactory`是`TerrainFactory&`)。

## 依赖关系

- 依赖：`map/terrain_mod.h`。
- 被谁依赖：`Source/Core/common/registry.h`(`Registry`持有唯一实例)、`Source/Core/map/map.h`(`Map::terrainFactory`引用成员)、`Core/map/terrain.h`。
