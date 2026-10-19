# terrain.h / terrain.cpp

## 职责

`Terrain`是某个具体`TerrainMod`实例的薄包装——构造时向`TerrainFactory`要一个实例（按id，如
`"ocean"`/`"mountain"`），把`GetType`/`GetName`/`GetPriority`/`SetupTexture`/`GetTextures`/
`DistributeTerrain`原样转发出去，析构时交还给Factory销毁（水面现在per-cell设置，不再经过
`Terrain`/`TerrainMod`任何getter，见`terrain_mod.md`"水面"一节，这个类也相应没有`GetWater`
了）。`GetTextures()`返回
`TerrainTexturePaths{diffuse, normal, roughness}`三个路径打包的小结构体（不是单独一个
`GetTexture()`返回diffuse字符串——近处/远处材质重做之后normal/roughness也要走这条通路，
见`ForeverTerrainFrameworkComponent.md`"近处地形混合"一节）。

## 关键设计

- **自身不持有任何地图格子数据**——地形类型/高度/细化高度/水面/hatch这些格子数据活在`Map`/`Element`
  （见`map.md`），`Terrain`只是"某个地形类型的行为句柄"，`Map::InitTerrains()`按优先级顺序
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
- 被谁依赖：`Source/Core/map/map.h`（`Map::InitTerrains()`临时构造`Terrain*`跑
  `DistributeTerrain`，见`map.md`）。

## 近处地形混合（`NearTerrainBlend`，原`terrain_blend.h/.cpp`，已合并进本文件）

### 职责

为近处地形的材质生成"混合权重"：给定一块方形区域（若干个地形 element），输出两块字节数组，
每个 texel 记录这个位置最大的 4 个地形槽位 id（`outIds`）和它们的权重（`outWeights`，4 个字节的
和恒为 255）。UE 侧把这两块数组拷进两张纹理，近处材质固定采样 4 个漫反射并加权混合，从而在不同
地形的交界处平滑过渡。纯 C++17，只依赖标准库，不碰 `Map` 和任何 UE 类型。

### 关键设计

- **输出只取决于地图数据和 texel 的世界位置，与区域怎么划分无关。** 用不同大小、不同起点的区域
  拼出来的结果逐字节相同。UE 侧因此可以随意平移控制区域，而不会让画面跳变。
- **平滑核**：以 texel 中心为圆心、半径 `radius`（element 单位，夹到 [1,3]）内所有 element，
  按到 element 中心的距离加权，`f(t) = (1 − t²)²`（t = d / radius，t ≥ 1 时为 0）。同一地形 id 的
  权重累加。`radius = 1.5` 大致覆盖 3×3 个 element，`2.0` 大致覆盖 4×4（实际是 5×5 的角被裁掉）。
- **核权重表**：核权重只和 texel 在 element 内的位置有关，构造时预计算一张
  `texelsPerElement² × (2·reach+1)²` 的表，`reach = ceil(radius + 0.5) − 1`。
- **前四大截断**：不同 id 超过 4 个时，设第 5 大的权重为 `w5`，前四大各自改成 `max(w − w5, 0)`
  再归一化；这样一个 id 从第 4 名掉到第 5 名时权重连续地降到 0，不会在排名交换处跳变。四项全为
  0（五个权重完全相等）时退回直接取前四大再归一化。
  - 代价：当 5 个以上 id 的权重几乎相等时，归一化会放大梯度，场仍然连续但在那个点附近非常陡。
    这只会出现在 5 种以上地形挤在 1.5 个 element 内的极端情况，自然地形几乎遇不到。
- **槽位顺序**按权重从大到小（并列按 id 升序）。消费方对两张纹理都用最近点采样，不依赖相邻
  texel 的槽位对齐。
- **量化**用最大余数法，保证每个 texel 的 4 个字节恰好加起来是 255，且每项误差小于 1/255。
- **construction 保险**（`constructionGuard`）：texel 所在 element 是 construction 时直接输出纯槽位。
  `radius ≤ 1.5` 时按现有规则（construction 的 3×3 邻域全是槽位 0 的 plain / construction）核本身
  就是纯的，这一条只是防止规则变了出现隐性错误。
- **construction 拉力**（`constructionRamp`，只在 `radius > 1.5` 时启用）：`radius > 1.5` 时核会越过
  construction 外的第二圈，不处理的话 construction 内外在边界上会有约 12% 的权重跳变（实测）。
  对 construction 外的 texel，按到最近 construction element 的距离算一个拉力
  `s = clamp(1 − d / constructionRamp, 0, 1)`，权重改成 `(1 − s) × 原权重 + s × 纯槽位`，边界上 `s = 1`。
- **快速路径**：texel 所在 element 的 `(2·reach+1)²` 邻域里所有槽位 id 都相同，或者命中 construction
  保险时，直接写纯槽位，跳过核累加；结果与慢速路径逐字节相同（`BuildRegion` 的 `useFastPath`
  开关就是为了验证这一点）。
- **纹理坐标约定**：输出下标 `(ty * res + tx) * 4 + slot`，`res = size * texelsPerElement`，
  texel (tx, ty) 的中心在地图坐标 `origin + ((tx + 0.5) / tpe, (ty + 0.5) / tpe)`，ty 向地图 +Y 增长，
  对应纹理第 ty 行，不需要翻转。
- 越出地图范围的 element 坐标夹到边缘 element。

### 依赖关系

- 依赖：算法本身只用标准库(和文件里`Terrain`类的Dependence依赖无关)。
- 被谁依赖：`Source/Forever/Framework/ForeverTerrainFrameworkComponent`（`RebuildNearControl`）。

### 待办/后续阶段

- UE 侧集成（两张控制纹理、近处材质 `TemplateNear`）见计划文档，这个文件本身不依赖它们。
- `ComputeTexelReference` 只用于验证，不在游戏里调用。
