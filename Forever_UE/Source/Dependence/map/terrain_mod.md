# terrain_mod.h / terrain_mod.cpp

## 职责

`TerrainMod`定义地形Mod必须实现的完整业务接口。

## 关键设计

- **`GetPriority()`决定生成顺序**——`Map::InitTerrains()`按这个值降序排列所有已注册地形，
  依次调用`DistributeTerrain`。数值越高越先跑，后跑的地形理论上可以覆盖先跑的（但具体覆盖
  策略由每个Mod自己在`DistributeTerrain`内部决定，接口本身不强制）。已知取值：Ocean=1.0f，
  Mountain=0.9f。
- **`SetupTexture()`和`DistributeTerrain()`分两步**——`SetupTexture`只填`diffusePath`/
  `normalPath`/`roughnessPath`/`waterHeight`这几个数据字段，`DistributeTerrain`才是真正
  改地图的算法。这样`Map::InitTerrains`可以先统一调用一轮`SetupTexture`拿到所有地形的
  贴图路径去构建`terrainTextures`索引表，再按优先级顺序跑`DistributeTerrain`，两件事
  互不依赖执行顺序。`normalPath`/`roughnessPath`都可以留空——UE侧
  `ForeverTerrainFrameworkComponent`遇到空路径时会给对应贴图数组槡位填平的法线+中性
  粗糙度，不是必填项（只有`diffusePath`留空会导致这个地形类型完全没有视觉表现）。
- **`DistributeTerrain`用4个`std::function`回调读写地图，不直接依赖`Map`/`Element`类型**——
  这是Dependence层一贯的做法（Mod DLL不知道也不需要知道`Map`的具体实现），调用方（`Map`）
  把自己的`GetTerrain`/`SetTerrain`/`GetHeight`/`SetHeight`包成闭包传进去。
- **没有`ShapeFilter`工具方法**——3x3邻域多数投票平滑滤波，目前没有任何地形Mod需要它。如果
  未来某个地形Mod确实需要这类平滑滤波，再按需加回，不用为了"完整"而维护一个没人用的方法。
- **`diffusePath`/`normalPath`/`roughnessPath`/`waterHeight`是公开数据成员，不是私有+
  getter**——`Terrain`包装类（`Source/Core/map/terrain.h`）的`GetTextures()`直接读
  `mod->diffusePath`/`mod->normalPath`/`mod->roughnessPath`打包成`TerrainTexturePaths`
  返回，`GetWater()`直接读`mod->waterHeight`，都不通过虚函数转发，减少一层调用。

## 依赖关系

- 依赖：无（纯C++，不依赖Core/UE）。
- 被谁依赖：`Source/Core/map/terrain.h`（`Terrain`包装类持有`TerrainMod*`）、
  `Source/Basic/map/terrain_basic.h`（`OceanTerrain`/`MountainTerrain`）、
  `Forever_Mod/Empty`的`EmptyTerrain`demo mod。

## 待办/后续阶段

- 如果后续出现"某个地形Mod需要平滑滤波工具"的真实需求，把`ShapeFilter`加回来。
