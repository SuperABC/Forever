# map.h / map.cpp

## 职责

`Map`是阶段4-1（Map域）聚合类的**雏形**——目前只承担Terrain域需要的职责：地图尺寸、
`Element`格子存储、`TerrainFactory`归属、地形分发（`InitTerrains`）、格子
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
- **`InitTerrains()`同时做Mod发现/注册+地形分发+3x3晋升规则**——按GetPriority()降序对所有
  已注册地形依次执行`DistributeTerrain`，再统一跑一次晋升规则，最后重建`terrainTextures`
  索引表。mod发现/注册这部分对照老工程`Map::InitTerrains`（`modHandles`+`dlls`两个参数，
  手动`LoadLibraryA`+函数指针类型`RegisterModTerrainsFunc`），新版直接复用
  `Core/common/loader.h`已经验证过的`ModLoader::RegisterConcept<TerrainFactory>`通用模板
  方法，不重新发明一遍老工程手写的DLL加载逻辑；地形分发+3x3晋升规则对应老工程`Map::
  InitBlocks`里"生成地形"那一段+`Map::InitContents()`的3x3晋升规则那一段，不是老工程同名
  的`InitContents`（老工程的命名下`InitBlocks`才是真正跑地形分发+construction晋升+生成
  路网的地方，`InitContents`实际是生成zone/building，容易搞混）。**也没有像老工程那样在这里
  硬编码注册一份`EmptyTerrain`**——理由见`terrain.md`。
  **实现期修正**：这部分原来拆成`InitTerrains()`（只注册）+`InitContents()`（只生成）两个
  函数——纯粹是因为Terrain是第一个迁移的domain，直接照抄了老工程本来就分开的这两个函数；
  后面`InitRoadnet`/`InitZones`/`InitBuildings`都是这次全新设计，没有对应的老工程两段式
  结构可抄，一直是注册+生成写在一个函数里，风格不统一。应用户要求合并回`InitTerrains()`
  一个函数，看齐后面几个domain的写法——目前`EnsureMapGenerated()`里这几个`InitXxx`本来就是
  背靠背调用，没有用到"只生成不重新注册"这种分开调用的场景，合并不影响任何现有行为。
- **"3x3邻域全plain才晋升construction"的规则**精确对照老工程`Map::InitBlocks`
  里的那段逻辑：3x3包含自身，任一邻居越界（地图边缘）则永远不晋升，`plain`和已经是
  `construction`的格子都算合格。这条规则和其余地形生成一样，在`InitTerrains()`里、所有
  `DistributeTerrain`跑完之后统一执行一次。
- **`AddHatch(Quad q, float rotation)`自动分发到所有与`q`重叠的element**——按`q`的旋转AABB
  算出重叠的格子范围，逐个格子调用`Element::hatches.emplace_back`，和老工程`Map::AddHatch`
  同语义（老工程按`Chunk`转发，这里直接操作扁平数组）。

- **`InitRoadnet()`必须在`InitTerrains()`之后调用**——`RoadnetMod::
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
- **单行道开口 + 每条车道都有专属贯通线（第八/九两轮迁移）**：`ThroughLine`最终形态是
  "每个(类别,side)对应一个vector，元素数量=该side车道数，每条车道各有一条entry"——这是
  两轮修复叠加的结果：
  - **第八轮**起因是道路3D资产改成按左右车道数命名后（见`Source/Basic/map/
    roadnet_basic.md`"车道资产命名"一节），井字最中间四条路出现了真正的单行道（一侧车道数
    为0）。当时的修复把"每个(类别,side)固定一条"改成"单行道side额外保存最靠左/最靠右两条"，
    双向路仍然只保存最内侧一条。
  - **第九轮**起因是PIE用导航图可视化（`bShowNavigationDebug`）实测发现：车道数>=2的双向
    侧（比如"中山西路"2车道那一侧）仍然只画出一条线——因为双向路多车道时，新开口走的是
    "内侧贯通线不动、外侧另外新增一条分支线"这套workaround，从未真正给外侧车道建立持久的
    贯通线，所以在没有任何开口发生之前，外侧车道天生就没有线可看。既然车道级导航图的目标
    就是每条车道都能独立寻址/可视化，这次干脆去掉这套workaround：`InitRoadnet()`建图阶段
    直接给**每条物理车道**都建一条贯通线（不再区分单行/双向，也不再只挑最内侧或两端），
    对应的锚点也从"每个方向一个、所有车道共用"改成"每条车道各自独立"（`RoadJunctionApproach::
    vehicleInbound`/`vehicleOutbound`从`Node*`变成`std::vector<Node*>`，见`roadnet.md`
    "车道级导航锚点"一节——这一步是可视化上真正看到多条线的关键，只加贯通线entry而不给
    独立锚点，几何上仍然会因为共用端点而重叠成一条线）。
  - 有了这个基础，`AddRoadAccessNode`的分裂规则统一成："先选出目标车道，直接把它自己的
    贯通线切两段"，不再需要按车道数/单行双向分支：双向路车道数>=2时固定选最外侧车道
    （沿用原始设计"新访问点代表外侧车道"的意图）；单行路按调用方要"最靠右
    (`useForwardSide=true`)"还是"最靠左(`false`)"，用居中后的真实横向偏移（和
    `RoadJunction::Build`同一套换算，见`roadnet.md`"车道居中"一节）挑出对应车道。
    `useForwardSide`的语义：请求的side本身有车道时效果不变（`true`=侧0/右手边，`false`=
    侧1/左手边）；请求的side是单行道的空侧时，重新解释成"要右边还是左边的车道"，从对侧车道
    里选。
- **`GetVehicleNavGraph()`/`GetPedestrianNavGraph()`/`GetNavAnchorNodes()`是只读访问**，
  给`Source/Forever/Framework/ForeverRoadnetFrameworkComponent.cpp`的导航图可视化用（按id画
  锚点方块+边长方体），未来Traffic域（阶段4-3）寻路时也会用同一套接口，不需要改`Map`。
  图里出现的id除了`GetNavAnchorNodes()`能查到坐标，还可能是地图边缘的extern残端（在
  `GetExterns()`里），调用方要两个列表都查。

- **`InitZones()`/`InitBuildings()`必须在`InitRoadnet()`之后调用**（要用到`GetLots()`），
  `InitBuildings()`还必须在`InitZones()`之后（假定`InitZones()`已经把`Lot::GetFreeLots()`
  该占的地方占掉）。这次相比老工程`Map::InitContents`有三处新设计（详见`Source/Dependence/
  map/zone_mod.md`、`geometry.md`）：①Zone/Building不再嵌套在一个单独的`Block`类里，直接向
  `Lot`要地（但Zone/Building本身概念上仍保留嵌套关系，这次只是不实现Zone内部再摆Building的
  递归布局）；②新增"mod直接指定一块贴着某条路的矩形区域"这种显式占位方式
  （`LotPlacementRequest`+`Lot::RequestPlacement`），每次真正的切分都会自动生成一条1单位宽
  的小路`Road`（`Lot::SplitWithPath`），还要考虑道路可达性；③权重不再是mod里写死的静态表，
  是mod被调用时动态往每个`Lot`上登记（`BuildingMod::candidateWeights`，**不是mod直接调用
  `Lot::AddCandidate`**——那样会跨模块写`Lot`自己的`std::vector`，退出时析构崩溃，已实测
  修复，见`Source/Dependence/map/zone_mod.md`"关键设计"一节；`Map::InitBuildings()`读到
  `candidateWeights`之后代为调用`lot->AddCandidate(...)`，保证分配器和析构在同一个模块）。
  - `InitZones()`：发现/注册zone mod，按注册顺序对每个类型建一个"扫描用"`ZoneMod*`
    （`zoneFactory.CreateZone(id)`），每次调用前重新按`Lot::GetFreeAcreage()`降序排序
    `GetLots()`（上一个mod的显式占位可能已经改变了各lot的剩余空闲面积，所以每次都要重排，
    不是构造时排一次到处传），调它的`Distribute(lots)`，读出`explicitPlacements`逐条调用
    对应`lot->RequestPlacement(...)`，成功的立刻`zoneFactory.CreateZone(id)`建一个新的
    "落地用"实例包成`Zone*`存进`zones`。**Zone只有这一步，不对Zone做权重/`FillRemainder`
    填充**——用户明确要求"zone生成阶段不要填满lot"。
  - `InitBuildings()`：结构类似，先扫描所有building mod类型的`explicitPlacements`（同
    `InitZones`），再对每个lot调`lot->FillRemainder(...)`（用`lot->GetCandidates()`当权重表，
    `randomAcreage`/`acreageMinMax`转发对应`BuildingMod`扫描实例的`RandomAcreage()`/
    `GetAcreageMin()`/`GetAcreageMax()`），为每个成功结果新建一个落地实例，最后对所有lot调
    `ClearCandidates()`（避免Zone没用完的权重——虽然Zone这次不产生权重——或者本轮
    Building扫描剩下的候选错误地留到下一次场景重建时还在）。
  - **两个mod扫描实例（`ZoneMod*`/`BuildingMod*`）和"落地实例"是分开的对象**——扫描实例只
    用来跑一次`Distribute()`+（Building的话）供`FillRemainder`查`RandomAcreage`等，处理完
    就销毁；每一条成功的占位/CDF结果都单独`new`一个全新的mod实例包成`Zone*`/`Building*`，
    因为mod实例可能带`id`/`name`这类只应该在真正创建一个实例时才递增的状态（`ZoneBasic`/
    `BuildingBasic`目前没有这类状态，但接口设计上不能假设所有mod都没有）。
  - **`Map`自己不持有任何小路`Road*`**——`Lot::RequestPlacement`/`FillRemainder`裁剪出的每条
    小路都记在被裁剪的那个顶层`Lot`自己的`pathRoadLinks`成员里（`Lot::GetPathRoadLinks()`/
    `GetPathRoads()`），析构时也由`Lot`自己`delete`。归属关系上小路本来就是"某个顶层`Lot`的
    空闲空间被裁剪的副产品"，而这个顶层`Lot`正是`RoadnetMod`初始化、`Roadnet`持有的那个
    `Lot`——`Map`没有理由再单独开一份列表重复记一遍"这些小路是谁的"，之前确实先后试过`Map`
    自己开`pathRoads`/`ownedRoads`数组存这些`Road*`，都被撤销改成现在这样：分类信息（是不是
    小路）记在`Road::IsPathRoad()`自己身上，归属/生命周期记在创造它的顶层`Lot`身上，`Map::
    GetPathRoads()`只是遍历`GetLots()`把每个顶层`Lot`自己的`GetPathRoads()`汇总起来，按值
    返回，不做任何持有。**小路现在会接入`vehicleNavGraph`/`pedestrianNavGraph`**——
    `RequestPlacement`/`FillRemainder`每产出一条新的`PathRoadLink`，调用方就立刻对它调一次
    `Map::ConnectPathRoad(link)`，具体规则见下"ConnectPathRoad"一节。

## ConnectPathRoad（第十二轮迁移，小路正式接导航图）

`Map::ConnectPathRoad(const PathRoadLink& link)`把`Lot::SplitWithPath`产出的一条小路正式接入
`vehicleNavGraph`/`pedestrianNavGraph`，由`InitZones()`/`InitBuildings()`对每条新产生的
`PathRoadLink`调用一次——调用点在`request.lot->RequestPlacement(...)`/`lot->FillRemainder(...)`
调用前后各记一次`lot->GetPathRoadLinks().size()`，处理`[之前的size, 现在的size)`这一段新增的
link，不改`RequestPlacement`/`FillRemainder`的函数签名（小路数据只存在`Lot`自己身上这条原则
不变）。**不管`RequestPlacement`最终返回`success`还是`false`都要处理新增的link**——`SplitWithPath`
产生的小路即使整体placement请求失败，也已经是真实持久化的几何（被某个freeLot的边界引用着）。
处理顺序天然=创建顺序（同一顶层`Lot`内部cascading cut时，后一刀如果连到前一刀新建的小路，前一刀
的link一定排在`pathRoadLinks`里更靠前的位置，先于后一刀被处理），这一点很重要——"小路接小路"
这条规则要求被连的小路必须已经建好自己的贯通线。

三条规则（用户逐条确认，含两轮澄清）：

1. 小路横断面固定是中轴线两侧0.3单位车道、再往外0.2单位人行道（`PathLaneSpec`，`geometry.h`）。
2. **小路接"大路"（`link.endRoad`非空且`!IsPathRoad()`）**：用小路自身连接方向和大路在
   `endT`处`perp0`的点积判断"最靠近小路的那一侧"（`dot>=0`是大路的side0，否则side1，和
   `roadnet.cpp`/`Lot::SplitWithPath`一直沿用的`perp0=(fwd.Y,-fwd.X)`同一个约定）——只处理
   这一侧：车行道断出2个node（`fromAnchor->Nin->Nout->toAnchor`，保留大路直行不中断），`Nin`
   （进入点）接小路"驶入"方向的车道、`Nout`（合并点）接小路"驶出"方向的车道（小路side0沿
   Start->End走、side1沿End->Start走，所以Start端side0驶入/side1驶出，End端相反）；人行道
   同理断出`Pa`/`Pb`两个node分别接小路的两条人行道（人行边本来就双向，不用区分驶入/驶出）。
   近侧没有车道/人行道时退化用远侧；两侧都没有就退化成孤立锚点。
3. **小路接"小路"（`link.endRoad`非空且`IsPathRoad()`）**：被连的小路两侧车道（近侧+远侧，
   近/远同样按点积判断）各自断出2个node，一个路口共4个车行node——近侧2个直接按规则2的方式接
   新小路的2条车道；远侧2个不直接接新小路，而是单向搭桥到近侧对应node（远进入点→近进入点、
   近合并点→远合并点），让被连小路的远侧车流也能绕到近侧再拐进新小路，同时不影响被连小路自己
   两侧车道各自的直行连接。人行道套用完全相同的近/远侧模型（近侧2个+近侧2个人行node，远侧和
   近侧对应node之间双向搭桥），并入新小路的2条人行道。

实现上复用/修好了`Map::AddRoadAccessNode`原来内联在自己函数体里、从未暴露过的一个问题：断开
一条车道的贯通线插入新node后，不会回写`ThroughLine.fromAnchor`/`edge`，导致同一条车道被断
第二次时找到的还是最初那条整段的`fromAnchor`/`toAnchor`，和第一次断出来的node脱节——小路的
场景必然会撞上这个问题（同一条临街大路很可能被沿线好几条小路连续断开），所以把"断一条贯通线、
插一个node、正确回写"这部分抽成了新的私有方法`Node* BreakThroughLine(Road*, bool isVehicle,
int side, int laneIndex, float worldX, float worldY, float worldZ)`，`AddRoadAccessNode`
自己也改成调用它（顺带修好了这个从没暴露过的bug，它当前仍然没有真正的调用方）。规则2/3里
"断出2个node"就是对同一个`(road,isVehicle,side,laneIndex)`连续调用两次`BreakThroughLine`——
第二次调用时`fromAnchor`已经自动指向第一次断出来的node，天然形成`fromAnchor->Nin->Nout->
toAnchor`，不需要额外的拼接逻辑。这条路径**不调`Road::AddOpening`**——`SplitWithPath`创建
小路时已经在大路上标过一次跨越整个`pathWidth`的开口了，这里不需要再重复标记。

`ResolvePathEndAnchors`是`ConnectPathRoad`的核心：给定小路在某一端（Start或End）连到的
`hostRoad`/`hostT`，解出小路在这一端的4个锚点（车行side0/1、人行side0/1）——`hostRoad`为空
就是孤立端点（4个都是不接入任何既有贯通线的新node，位置按小路自己的`perp0`偏移）。`ConnectPathRoad`
拿到两端各自的4个锚点后，建小路自己的4条贯通线（车行side0 Start->End、side1 End->Start，
人行两侧各自双向插入，和`InitRoadnet()`对普通`Road`的建图规则完全一致），同时登记进
`throughLines[path]`，保持和普通`Road`一样的基础设施。

已知简化范围（不在这次要求内，没做）：`BreakThroughLine`按"调用顺序"而不是"沿路真实弧长顺序"
拼接同一条车道上的多次打断——如果两条小路连到同一条大路但沿路先后顺序和处理顺序不一致，拼出来
的中间node顺序可能和物理位置顺序不完全对应（连通性依然正确，只是链条上node排列的先后不严格按
t排序）；小路自己两侧人行道之间不建"穿过小路本身"的横道连接（真实`RoadJunction`会建，小路
没有被要求这个）。

## 依赖关系

- 依赖：`terrain.h`、`terrain_factory.h`、`roadnet.h`、`roadnet_factory.h`、`zone.h`、
  `zone_factory.h`、`building.h`、`building_factory.h`、`map/geometry.h`（`Quad`/`Node`/
  `Road`/`Lot`等，`hatches`字段类型）、`common/config.h`、`common/loader.h`
  （`InitTerrains`/`InitRoadnet`/`InitZones`/`InitBuildings`用）、`common/utility.h`
  （`debugf`）。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（持有`Map*`，
  `EnsureMapGenerated`时依次调用`InitTerrains`+`InitRoadnet`+`InitZones`+
  `InitBuildings`）、`Source/Forever/Framework/ForeverTerrainFrameworkComponent.h/.cpp`
  （`GenerateTerrain(Map*)`读取生成好的格子数据建mesh）、`Source/Forever/Framework/
  ForeverRoadnetFrameworkComponent.h/.cpp`（`GenerateRoadnet(Map*)`读取`GetRoads()`/
  `GetJunctions()`/`GetLots()`/`GetPathRoads()`/`GetPathRoadMaterial()`建mesh）、
  `Source/Forever/Framework/ForeverZoneFrameworkComponent.h/.cpp`（`GenerateZones(Map*)`读
  `GetZones()`）、`Source/Forever/Framework/ForeverBuildingFrameworkComponent.h/.cpp`
  （`GenerateBuildings(Map*)`读`GetBuildings()`）。

## 待办/后续阶段

- 阶段4：Block/Component/Room迁移时在这个类上继续扩展，具体怎么扩展（加字段还是拆分成多个
  协作的类）留到那几个阶段开始时再定。Zone/Building已经迁移完，见上"InitZones/InitBuildings"
  一节。
- 阶段4：`InitTerrains()`目前假定调用方（`AForeverFrameworkActor::BeginPlay`）已经在此之前
  跑过一次`Config::ReadConfig`（实际上是`ForeverModSubsystem`这个`UGameInstanceSubsystem`
  在game instance启动时做的，早于任何关卡的`BeginPlay`）——这个顺序依赖目前只是约定，没有
  代码层面强制，如果以后出现`Config::ReadConfig`没跑就调用`InitTerrains`的场景，需要补上
  显式检查或调整调用时机。
