# terrain.h / terrain.cpp

## 职责

`Terrain`是某个具体`TerrainMod`实例的薄包装——构造时向`TerrainFactory`要一个实例（按id，如
`"terrain_ocean"`/`"terrain_mountain"`），把`GetType`/`GetName`/`GetPriority`/`SetupTexture`/
`DistributeTerrain`原样转发出去，析构时交还给Factory销毁（水面是per-cell设置的，不经过
`Terrain`/`TerrainMod`的getter，见`terrain_mod.md`"水面"一节）。`SetupTexture()`之后可取
`GetMapColor()`(地图界面颜色)和`GetMaterialSpec()`(地表材质声明)，由`Map::GetTerrainAppearances()`
收集成`TerrainAppearance{mapColor, material}`表，UE侧据此生成地形材质(见`ForeverTerrainFrameworkComponent.md`"地形材质"一节)。

## 关键设计

- **自身不持有任何地图格子数据**——地形类型/高度/细化高度/水面/hatch这些格子数据活在`Map`/`Element`
  （见`map.md`），`Terrain`只是"某个地形类型的行为句柄"，`Map::InitTerrains()`按优先级顺序
  临时创建一批`Terrain*`跑完`DistributeTerrain`就地析构，不长期持有。
- **Core层没有内置兜底地形**：占位的`EmptyTerrain`（id`"terrain_empty"`）由`Forever_Mod/Empty`通过`ModLoader`
  正常发现注册，Core不再重复实现一份，避免同一个id在`TerrainFactory::registries`里互相覆盖。
- **析构必须走`TerrainFactory::DestroyTerrain`，不能直接`delete mod`**——`mod`可能是
  Basic.dll或某个Forever_Mod DLL里`new`出来的实例，析构要走它自己注册时提供的deleter，这是
  `TerrainFactory`本身已经处理好的跨DLL new/delete安全（见`terrain_factory.md`），`Terrain`
  只是老实调用`factory->DestroyTerrain(mod)`。

## 依赖关系

- 依赖：`terrain_mod.h`、`terrain_factory.h`、`common/error.h`（`NullPointerException`）。
- 被谁依赖：`Source/Core/map/map.h`（`Map::InitTerrains()`临时构造`Terrain*`跑
  `DistributeTerrain`，见`map.md`）。

地形覆盖图的烘焙(`terrain_coverage.h/.cpp`)有自己的文档，见`terrain_coverage.md`。
