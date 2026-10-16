# terrain_mod.h / terrain_mod.cpp

## 职责

`TerrainMod`定义地形Mod必须实现的完整业务接口。

## 关键设计

- **`GetPriority()`决定生成顺序**——`Map::InitTerrains()`按这个值降序排列所有已注册地形，
  依次调用`DistributeTerrain`。数值越高越先跑，后跑的地形理论上可以覆盖先跑的（但具体覆盖
  策略由每个Mod自己在`DistributeTerrain`内部决定，接口本身不强制）。已知取值：Ocean=1.0f，
  Mountain=0.9f。
- **`SetupTexture()`和`DistributeTerrain()`分两步**——`SetupTexture`只填`diffusePath`/
  `normalPath`/`roughnessPath`这几个数据字段，`DistributeTerrain`才是真正改地图的算法。
  这样`Map::InitTerrains`可以先统一调用一轮`SetupTexture`拿到所有地形的贴图路径去构建
  `terrainTextures`索引表，再按优先级顺序跑`DistributeTerrain`，两件事互不依赖执行顺序。
  `normalPath`/`roughnessPath`都可以留空——UE侧`ForeverTerrainFrameworkComponent`遇到
  空路径时会给对应贴图数组槡位填平的法线+中性粗糙度，不是必填项（只有`diffusePath`留空
  会导致这个地形类型完全没有视觉表现）。
- **`DistributeTerrain`用6个`std::function`回调读写地图，不直接依赖`Map`/`Element`类型**——
  这是Dependence层一贯的做法（Mod DLL不知道也不需要知道`Map`的具体实现），调用方（`Map`）
  把自己的`GetTerrain`/`SetTerrain`/`GetHeight`/`SetHeight`/`SetWater`/`SetFineHeights`包成闭包传进去。
- **水面(`setWater`)独立于地形类型(`setTerrain`)，每个格子单独设置**——以前水面走
  `waterHeight`这个成员，`SetupTexture()`填一次、整个`DistributeTerrain`调用期间所有
  格子共用同一个水位，没法做出倾斜水面（河流沿下坡方向每个格子高度不一样，水位也该跟着
  不一样）。现在`DistributeTerrain`拿到的`setWater(x,y,hasWater,height)`是per-cell的，
  哪个格子要水、水位多少完全由mod自己在算法里决定，`OceanTerrain`/`RiverTerrain`都已经
  迁移到这套机制，见`terrain_basic.md`。
- **细化高度(`setFineHeights`)按需逐格设置，默认没有**——粗高度`height`是格点`(x,y)`上的
  一个采样，10m一格，UE侧双线性插值在格子边界只有C0连续，起伏大的地方(河岸)会出现明显
  折痕。mod可以给个别element传一个`TERRAIN_FINE_SIZE`×`TERRAIN_FINE_SIZE`(17×17)的高度
  数组，覆盖`[x,x+1]×[y,y+1]`，含右/上边界那一列/行——选17×17不选16×16，是因为这样每块
  自成一体，求值不用读邻居(邻居很可能没有细化数组)，而且UE最细一级LOD每格正好16段，各级LOD
  顶点都落在这个网格的下标上，直接点采样即可。约定：四个角等于对应格点粗高度；相邻两块
  细化数组的共享边应该一致(不一致也不会裂缝，每个点只由floor落到的那一格求值)；必须先
  `setHeight`再`setFineHeights`，因为`Map::SetHeight`会清掉共用该格点的4块细化数组，防止
  后跑的mod改了粗高度之后细化数据过期。数据只传`const float*`，Core自己分配内存拷贝，不会
  跨DLL释放mod分配的内存。目前只有`RiverTerrain`的河岸用到，见`terrain_basic.md`。
- **没有`ShapeFilter`工具方法**——3x3邻域多数投票平滑滤波，目前没有任何地形Mod需要它。如果
  未来某个地形Mod确实需要这类平滑滤波，再按需加回，不用为了"完整"而维护一个没人用的方法。
- **`diffusePath`/`normalPath`/`roughnessPath`是公开数据成员，不是私有+getter**——
  `Terrain`包装类（`Source/Core/map/terrain.h`）的`GetTextures()`直接读
  `mod->diffusePath`/`mod->normalPath`/`mod->roughnessPath`打包成`TerrainTexturePaths`
  返回，不通过虚函数转发，减少一层调用。

## 依赖关系

- 依赖：无（纯C++，不依赖Core/UE）。
- 被谁依赖：`Source/Core/map/terrain.h`（`Terrain`包装类持有`TerrainMod*`）、
  `Source/Basic/map/terrain_basic.h`（`OceanTerrain`/`MountainTerrain`）、
  `Forever_Mod/Empty`的`EmptyTerrain`demo mod。

## 待办/后续阶段

- 如果后续出现"某个地形Mod需要平滑滤波工具"的真实需求，把`ShapeFilter`加回来。
