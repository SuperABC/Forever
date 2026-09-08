# map.h / map.cpp

## 职责

`Map`是阶段4-1（Map域）聚合类的**雏形**——目前只承担Terrain域需要的职责：地图尺寸、
`Element`格子存储、`TerrainFactory`归属、地形分发（`InitTerrains`/`InitContents`）、格子
数据的读写accessor。**这不是最终形态**：Zone/Block/Component/Room/Building/Roadnet按
`PHASE4_PLAN.md`阶段4-1的顺序陆续迁移时，会在**这同一个类**上继续加字段（zone/building归属、
Roadnet指针等）和方法（各自的Factory、`InitZones`/`InitBuildings`等），不是到最后单独另建
一个"真正的"`Map`类替换掉这个。

## 关键设计

- **用扁平`std::vector<Element>`（行主序，`y*width+x`）取代老工程的`Chunk`分块存储**——老
  工程`Source/Core/map/map.h`里的`Chunk`（`CHUNK_SIZE=64`）是内存/流式加载优化，不是正确性
  必须的设计；对于目前的地图规模（默认1024x1024=1048576个`Element`，每个`Element`几十字节），
  扁平数组已经足够，没必要现在就引入分块的复杂度。如果后续实测证明大地图下`Chunk`分块确实
  必要（比如支持更大地图或流式卸载），再补，不算重新设计（`GetTerrain`/`SetTerrain`等
  accessor的对外签名不用变，只是内部存储换掉）。
- **`Element`现在就带上了`hatches`字段**——虽然目前没有任何系统（Roadnet/Building还没迁移）
  会真的往里面塞hatch，`GetHatches`永远返回空列表。这不是"为假设的未来需求"预留：
  `Source/Forever/Framework/ForeverTerrainFrameworkComponent.cpp`的挖洞逻辑（要求#2）就是
  按`hatches`永远为空来实现并验证的，字段本身是这次迁移范围的一部分，只是消费方（Roadnet/
  Building）还没接上。
- **`InitTerrains()`只做Mod发现/注册，不含地形分发**——对照老工程`Map::InitTerrains`
  （`modHandles`+`dlls`两个参数，手动`LoadLibraryA`+函数指针类型`RegisterModTerrainsFunc`），
  新版直接复用`Core/common/loader.h`已经验证过的`ModLoader::RegisterConcept<TerrainFactory>`
  通用模板方法，不重新发明一遍老工程手写的DLL加载逻辑。**也没有像老工程那样在这里硬编码注册
  一份`EmptyTerrain`**——理由见`terrain.md`。
- **`InitContents()`对应老工程`Map::InitBlocks`里"生成地形"那一段+`Map::InitContents()`
  的3x3晋升规则那一段，不是老工程同名的`InitContents`**——老工程的命名下`InitBlocks`才是
  真正跑地形分发+construction晋升+生成路网的地方，`InitContents`实际是生成zone/building。
  这里的`InitContents()`是给"Terrain域范围内的生成步骤"起的新名字，不是1:1复刻老工程的函数
  职责划分——因为这个`Map`目前只有Terrain域的能力，等Zone/Building迁移时再决定要不要把
  `InitContents`拆成多个更细的步骤对齐老工程的调用顺序。
- **"3x3邻域全plain才晋升construction"的规则**（要求#4）精确对照老工程`Map::InitBlocks`
  里的那段逻辑：3x3包含自身，任一邻居越界（地图边缘）则永远不晋升，`plain`和已经是
  `construction`的格子都算合格。这条规则和其余地形生成一样，在`InitContents()`里、所有
  `DistributeTerrain`跑完之后统一执行一次。
- **`AddHatch(Quad q, float rotation)`自动分发到所有与`q`重叠的element**——按`q`的旋转AABB
  算出重叠的格子范围，逐个格子调用`Element::hatches.emplace_back`，和老工程`Map::AddHatch`
  同语义（老工程按`Chunk`转发，这里直接操作扁平数组）。

## 依赖关系

- 依赖：`terrain.h`、`terrain_factory.h`、`map/geometry.h`（`Quad`，`hatches`字段类型）、
  `common/config.h`、`common/loader.h`（`InitTerrains`用）、`common/utility.h`（`debugf`）。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（持有`Map*`，`BeginPlay`
  时调用`InitTerrains`+`InitContents`）、`Source/Forever/Framework/
  ForeverTerrainFrameworkComponent.h/.cpp`（`GenerateTerrain(Map*)`读取生成好的格子数据建
  mesh）。

## 待办/后续阶段

- 阶段4：Zone/Block/Component/Room/Building/Roadnet迁移时在这个类上继续扩展，具体怎么扩展
  （加字段还是拆分成多个协作的类）留到那几个阶段开始时再定。
- 阶段4：`InitTerrains()`目前假定调用方（`AForeverFrameworkActor::BeginPlay`）已经在此之前
  跑过一次`Config::ReadConfig`（实际上是`ForeverModSubsystem`这个`UGameInstanceSubsystem`
  在game instance启动时做的，早于任何关卡的`BeginPlay`）——这个顺序依赖目前只是约定，没有
  代码层面强制，如果以后出现`Config::ReadConfig`没跑就调用`InitTerrains`的场景，需要补上
  显式检查或调整调用时机。
