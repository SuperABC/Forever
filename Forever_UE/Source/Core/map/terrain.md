# terrain.h / terrain.cpp

## 职责

`Terrain`是某个具体`TerrainMod`实例的薄包装——构造时向`TerrainFactory`要一个实例（按id，如
`"ocean"`/`"mountain"`），把`GetType`/`GetName`/`GetPriority`/`SetupTexture`/`GetTexture`/
`GetWater`/`DistributeTerrain`原样转发出去，析构时交还给Factory销毁。对照老工程
`E:\Projects\Forever_UE\Source\Core\map\terrain.h/.cpp`迁移。

## 关键设计

- **自身不持有任何地图格子数据**——地形类型/高度/水面/hatch这些格子数据活在`Map`/`Element`
  （见`map.md`），`Terrain`只是"某个地形类型的行为句柄"，`Map::InitContents()`按优先级顺序
  临时创建一批`Terrain*`跑完`DistributeTerrain`就地析构，不长期持有。
- **没有迁移老工程的`EmptyTerrain`**——老工程`terrain.h`里声明了一个`EmptyTerrain`
  （priority=0、`DistributeTerrain`空实现），本来是`Map::InitTerrains`里硬编码注册的兜底
  地形。`Forever_Mod/Empty`这个demo mod已经有一个同名同用途的`EmptyTerrain`（id`"empty"`），
  会通过`ModLoader`正常发现注册，没必要在Core层再重复实现一份、还要操心两边id`"empty"`
  撞在同一个`TerrainFactory::registries`里谁覆盖谁的问题。这是有意的范围裁剪。
- **析构必须走`TerrainFactory::DestroyTerrain`，不能直接`delete mod`**——`mod`可能是
  Basic.dll或某个Forever_Mod DLL里`new`出来的实例，析构要走它自己注册时提供的deleter，这是
  `TerrainFactory`本身已经处理好的跨DLL new/delete安全（见`terrain_factory.md`），`Terrain`
  只是老实调用`factory->DestroyTerrain(mod)`。

## 依赖关系

- 依赖：`terrain_mod.h`、`terrain_factory.h`、`common/error.h`（`NullPointerException`）。
- 被谁依赖：`Source/Core/map/map.h`（`Map::InitContents()`临时构造`Terrain*`跑
  `DistributeTerrain`）。
