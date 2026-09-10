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
- **`Element`的`hatches`字段最初（Terrain阶段）验证时永远是空的**——`Source/Forever/
  Framework/ForeverTerrainFrameworkComponent.cpp`的挖洞逻辑（要求#2）当时是按`hatches`
  永远为空来实现并验证的，字段本身是那次迁移范围的一部分，只是消费方还没接上。Roadnet
  的隧道口是第一个真正往里面塞数据的消费方（`InitRoadnet()`把`roadnet->GetHatches()`转发进
  `AddHatch`），Building迁移时大概率也会有自己的hatch来源。
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

- **`InitRoadnet()`必须在`InitTerrains()`+`InitContents()`之后调用**——`RoadnetMod::
  DistributeRoadnet`要采样已经生成好的地形类型/水面（`getTerrain`/`getWater`两个回调），道路
  本身高度固定0，不采样/不跟随地形高度（这是这次范围裁剪，不是遗漏，以后要做的话再加
  `getHeight`回调和高度贴合逻辑）。内部流程：①`ModLoader::RegisterConcept<RoadnetFactory>`
  发现/注册roadnet mod dll（复用`Map`已有的`modLoader`成员，不新建）；②按
  `RoadnetFactory::GetRoadnet()`（单选，见`roadnet_factory.md`）选出唯一启用的mod，
  `new Roadnet(&roadnetFactory, id)`；③`roadnet->DistributeRoadnet(...)`+
  `roadnet->AllocateAddress()`，随后把`roadnet->GetHatches()`（目前唯一的来源是Roadnet隧道口，
  见`Source/Basic/map/roadnet_basic.md`"隧道"一节）逐个转发进`this->AddHatch(quad,rotation)`
  ——复用Terrain阶段已经建好的挖洞机制，不需要为Roadnet另起一套；④按每个`Intersection`
  收集与之相连的`Road`，
  `RoadJunction::Build`逐个建路口（车行/行人锚点+路缘角点）；⑤遍历每条`Road`建"最内侧车道
  贯通线"+遍历每个`RoadJunction`建路口内部连接，一起构成`vehicleNavGraph`/
  `pedestrianNavGraph`两张导航图。具体设计理由（车道级偏移锚点、车行全联通/行人人行横道+
  转角连通模型）见`roadnet.md`。
- **`AddRoadAccessNode`是给未来Building/Zone用的公开API，这次由Forever层的demo验证代码调用
  一次**——车道分裂/开口这套逻辑目前没有真正的调用方（Building/Zone还没迁移），但接口和实现
  都是完整、可用的，不是占位。`ThroughLine`（`Map`私有实现细节，见`roadnet.md`最后一条）记录
  每条`Road`每个方向/类别当前的贯通线，供`AddRoadAccessNode`拆分。
- **`GetVehicleNavGraph()`/`GetPedestrianNavGraph()`/`GetNavAnchorNodes()`是只读访问**，
  给`Source/Forever/Framework/ForeverRoadnetFrameworkComponent.cpp`的导航图可视化用（按id画
  锚点方块+边长方体），未来Traffic域（阶段4-3）寻路时也会用同一套接口，不需要改`Map`。
  图里出现的id除了`GetNavAnchorNodes()`能查到坐标，还可能是地图边缘的extern残端（在
  `GetExterns()`里），调用方要两个列表都查。

## 依赖关系

- 依赖：`terrain.h`、`terrain_factory.h`、`roadnet.h`、`roadnet_factory.h`、`map/geometry.h`
  （`Quad`/`Node`/`Road`/`Lot`等，`hatches`字段类型）、`common/config.h`、`common/loader.h`
  （`InitTerrains`/`InitRoadnet`用）、`common/utility.h`（`debugf`）。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（持有`Map*`，
  `EnsureMapGenerated`时依次调用`InitTerrains`+`InitContents`+`InitRoadnet`）、
  `Source/Forever/Framework/ForeverTerrainFrameworkComponent.h/.cpp`（`GenerateTerrain(Map*)`
  读取生成好的格子数据建mesh）、`Source/Forever/Framework/
  ForeverRoadnetFrameworkComponent.h/.cpp`（`GenerateRoadnet(Map*)`读取`GetRoads()`/
  `GetJunctions()`/`GetLots()`建mesh）。

## 待办/后续阶段

- 阶段4：Zone/Block/Component/Room/Building/Roadnet迁移时在这个类上继续扩展，具体怎么扩展
  （加字段还是拆分成多个协作的类）留到那几个阶段开始时再定。
- 阶段4：`InitTerrains()`目前假定调用方（`AForeverFrameworkActor::BeginPlay`）已经在此之前
  跑过一次`Config::ReadConfig`（实际上是`ForeverModSubsystem`这个`UGameInstanceSubsystem`
  在game instance启动时做的，早于任何关卡的`BeginPlay`）——这个顺序依赖目前只是约定，没有
  代码层面强制，如果以后出现`Config::ReadConfig`没跑就调用`InitTerrains`的场景，需要补上
  显式检查或调整调用时机。
