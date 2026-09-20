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
  - **Building内部布局落地后第一次有了真正的调用方（行人`outside`端点接路网），PIE验证
    立刻炸出一个之前从未触发过的bug**：`AddRoadAccessNode`算新访问点世界坐标一直是自己
    内联一份`offsetDist = LaneCenterOffset(lanes, laneIndex)`，直接从道路中心线量——这个
    公式对车行道是对的，但对行人道是错的：人行道物理上在同侧车行道+停车道**外侧**，必须像
    `ComputeLaneAnchorPosition`那样先加上`SumWidths(车行道)+SumWidths(停车道)`才是真正的
    人行道基准（和`RoadJunction::Build`里`pedestrianSide`锚点用的是同一套算法，见下面
    `ComputeLaneAnchorPosition`那条）。之前一直没暴露是因为这条函数从来没有真正的行人调用方；
    这次改成直接调用`ComputeLaneAnchorPosition`算`nx/ny`，不再自己重复一份（本该一开始就
    共用，`map.h`的`ComputeLaneAnchorPosition`注释其实早就写着"和`AddRoadAccessNode`算
    `nx/ny`用的是同一套公式"，但实现一直没跟上）。
  - **紧接着又炸出第二个bug：`AddRoadAccessNode`原来的参数是`const string& roadName`，
    在`roadnet->GetRoads()`（只装"大路"）里按名字线性查找**——building贡献的outside端点
    如果朝向的边界Road正好是一条小路（`Lot::SplitWithPath`产的path road），两个问题一起
    炸：①小路根本不在`roadnet->GetRoads()`里（小路挂在各自`Lot::GetPathRoads()`下），
    查找必然落空；②就算把小路也塞进查找范围，小路全部共用同一个名字字面量`"path"`（不像
    大路每条名字唯一），按名字查根本没法在多条同名小路之间区分。PIE验证发现的现象是"建筑
    连到大路正常，连到小路完全没反应"，正对应这两个问题。修复：`AddRoadAccessNode`签名
    从`const string& roadName`改成`Road* road`，去掉内部按名字查找那段——调用方
    (`Map::FlushPendingBuildingRoadAccess`)手上本来就有确定的`Road*`（来自
    `Building::GetBoundaryRoad`，`ThroughLine`自己也是按指针不是按名字索引），直接传指针
    既修好小路场景又省一次线性查找。这次改动前`AddRoadAccessNode`唯一的调用方就是
    `Map`自己（Forever层那次demo验证调用早就删掉了，见
    `ForeverRoadnetFrameworkComponent.md`），所以是纯粹的签名清理，不影响其它调用方。
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
- **`FindPedestrianPath(fromNodeId, toNodeId)`（进入society域新增，`pedestrianNavGraph`
  第一次真正被寻路消费）**：这份图之前一直只是可视化用的数据结构，没有任何代码真正走过
  它来寻路——市民按Job调度上下班需要真的从家走到商店，这次补上一个标准Dijkstra：边权用
  每条`Connection::CalcDistance()`（弧长，比两端点欧式距离更准确），最短路径用
  `unordered_map<int,int> prev`回溯，再翻转成id序列，最后按`GetNavAnchorNodes()`/
  `GetExterns()`两个列表反查真正的`Node*`返回。图不连通/起点终点id不存在时返回空——
  调用方（`UForeverPopulaceFrameworkComponent::RequestWalk`）要处理这个兜底（直接瞬移，
  不模拟中途过程），见该文件的`.md`"市民走路"一节。不用`AIController`/`NavMesh`，纯
  Core层数据结构上的图搜索。
- **`GetAllComponents()`（进入society域新增）**：拍平`buildings`里每个`Building::
  GetComponents()`，供`AForeverFrameworkActor::EnsureSocietyGenerated()`喂给
  `Society::Init`——和`ComputeAccommodationTarget()`喂给`Populace::Init()`同一个已有
  套路，`Society`不知道`Map`/`Building`的存在，只接收这份拍平结果。
- **`resolveAnchor`第三条兜底分支：既不是`RoadJunction`也不是`extern`的端点，按车道宽度
  现算+缓存各自的锚点（第六轮迁移新增，中间有一次返工）**：起因是`JingRoadnet`的隧道口把
  一条路自己拆成了三段独立`Road`（引道/下坡/隧道内平路，见`Source/Basic/map/roadnet_basic.md`
  "隧道"一节），中间两个分段点(`flatNode`/`splitNode`)只是mod自己引入的几何过渡，不是
  `RoadnetMod::intersections`里的真正路口，也不是地图边缘的`extern`——PIE验证发现隧道范围内
  车行/人行导航线整段断掉，因为`resolveAnchor`原来只有"命中`RoadJunction`"/"命中`extern`"
  两条分支，两条都不中就直接返回`nullptr`，`fromAnchor`/`toAnchor`有一个为空整条贯通线就
  建不出来。
  - 第一次修复尝试把`flatNode`/`splitNode`也注册成`Intersection`走`RoadJunction::Build`，
    但`RoadJunction`会按`setback`裁剪+摆一个强制水平的路口平面——`splitNode`正好卡在S形下坡
    曲线中间，PIE验证发现斜坡中间平白多出一个路口平面，渲染完全不对（路口本来就不该出现在
    斜坡上），这个思路被否掉了。
  - 第二次修复把所有车道/人行道在这个端点全部退化成同一个共享`Node`（照搬extern端点"没有
    真正分叉、所有车道挤到一个点"的规则）——这个思路也被否掉了：`flatNode`/`splitNode`
    两端的车道数/宽度配置完全一致（前后两段`Road`用同一套`configureLanesEx`参数），只是
    几何上直接续接，并没有"车道消失、没必要按宽度区分"这个前提，仍然应该按各自车道的真实
    宽度摆开，不能因为不是真路口就把车道全部收缮成一点。
  - 最终修复：`resolveAnchor`第三条分支现算一个"直接经过"锚点——取该端切线的右手垂线方向
    (和`RoadJunction::Build`的`makeAnchor`同一套约定)，按`LaneCenterOffset`/车道宽度算出
    真实的横向偏移(`setback`固定为0，这类点没有喇叭口不需要沿路收缩)，`passthroughAnchorCache`
    按`(端点id, 车行/行人, side, laneIndex)`缓存结果——前一段`Road`在这个端点的终点锚点和
    后一段`Road`在这个端点的起点锚点用的是同一套key，第二次请求直接命中缓存返回同一个
    `Node*`，两条贯通线因此接到一起；两段路在分段点处方向连续（引道/S形曲线在`flatNode`/
    `splitNode`处切线完全一致，见`roadnet_basic.md`"隧道"一节`addControls`的说明），所以
    用哪一段路现算结果都一样，缓存只是为了保证两次现算返回同一个`Node*`。新建的`Node`推进
    `navAnchorNodes`（和路口/车道分裂新增锚点同一套生命周期+可视化机制，不需要另开一张表）。

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
  - `InitZones()`：发现/注册zone mod。**仿照老工程"一个Zone独占一个ZoneMod实例"的做法**：
    `ZoneMod::Distribute()`/`explicitPlacements`已经改成不需要任何实例的static方法
    `ZoneMod::Assign(lots, emit, context)`——对每个类型先调一次`zoneFactory.Assign(id,
    排好序的GetLots(), &EmitPlacementRequest, &requests)`一次性扫完全地图的lot（不存在任何
    `ZoneMod`实例），拿到这个类型想要的全部`LotPlacementRequest`，逐条调用
    `lot->RequestPlacement(...)`，只有真的成功了，才`zoneFactory.CreateZone(id)`创建**唯一
    一次**、真正要被长期持有的实例，直接交给新建的`Zone`持有、存进`zones`——不会再出现
    "构造了一个mod实例结果这块地根本不要、白白析构"的情况，因为"要不要这块lot"这个问题完全
    不需要构造实例来回答（`EmitPlacementRequest`是map.cpp里的一个静态自由函数：
    `static_cast<vector<LotPlacementRequest>*>(context)->push_back(request);`——它编译在
    Core这一侧，mod调它触发的`push_back`用的是Core自己的分配器，不会出现"mod分配、Core释放"
    的跨DLL问题，和`AssignFunc`/`PlacementEmitFunc`裸函数指针机制配套，见`zone_mod.md`）。
    **Zone只有这一步，不对Zone做权重/`FillRemainder`填充**——用户明确要求"zone生成阶段不要
    填满lot"。`Zone`落地成功后：①`SetPosition`+从`RequestPlacement`的`outBoundaryRoads`
    输出参数逐个`zone->SetBoundaryRoad`；②调用一次`zone->Layout(request.direction)`（内部
    转发`mod->Layout(direction, *this, GetBoundaryRoads())`，填好`walls`/`gates`/
    `vehicleEntries`/`vehicleExits`/`pedestrianAccess`/`internalRoads`/`internalBuildings`——
    这几个字段的填充时机从`Distribute()`里"边声明显式占位边算围墙"，拆成`Assign`只决定"要不要、
    往哪摆"、`Layout`只管"摆下去之后长什么样"，且只在真正会被保留的实例上跑一次）；③对
    `mod->vehicleEntries`/`vehicleExits`/`pedestrianAccess`逐点调用`ConnectZoneAccessPoint`
    接图；④对`mod->internalRoads`逐条调用`ConnectZoneInternalRoad`实例化真正的`Road`并接图，
    结果`zone->SetInternalRoads`；⑤`Map::AddZone(zone)`——重名返回`false`就`delete zone`
    （`~Zone()`里`factory->DestroyZone(mod)`会跟着跑），成功就登记进`zones`（`寻址`一节）。
    围墙/大门（`mod->walls`/`mod->gates`）不需要额外搬运——`Zone::GetWalls()`/`GetGates()`
    直接转发`zone`自己持有的这个`mod`。**内部建筑（`mod->internalBuildings`）这一步不在
    `InitZones()`里处理**——`PlaceZoneInternalBuilding`要`new Building(&buildingFactory,
    mod)`，但`buildingFactory`的mod注册在`InitBuildings()`里才做（`InitBuildings()`必须在
    `InitZones()`之后跑，要用到这里裁剪完的剩余空闲面积），这个阶段`buildingFactory`还是
    空的，`CreateBuilding`会返回`nullptr`导致`Building`构造函数抛异常崩溃（PIE验证发现）。
    真正的实例化推迟到`InitBuildings()`（见下），直接读`zone->GetMod()->internalBuildings`
    （不需要`Zone`额外存一份拷贝——`Zone`本来就独占持有这个`mod`，数据不会失效）。②③④
    具体规则见下"Zone内部布局"一节。
  - `InitBuildings()`：和`InitZones()`完全同构，不再有`scanners`表。**显式占位**：对每个
    building类型调`buildingFactory.Assign(id, GetLots(), &EmitPlacementRequest, &requests)`
    一次性扫完全部lot，逐条`RequestPlacement`成功才`buildingFactory.CreateBuilding(id)`
    创建**唯一一次**的独占实例，`SetPosition`+`SetBoundaryRoad`之后调用
    `building->Layout(request.direction)`（显式占位有真实方向）。**权重登记**：不再需要
    任何mod实例，直接对每个类型、每个lot调`buildingFactory.GetPower(id, lot->GetArea())`
    （不需要实例的static方法，替代原来mod动态push`candidateWeights`那条链路——`RoadnetMod`
    （`JingRoadnet::DistributeRoadnet`）已经会给每个lot调用`Lot::SetArea()`标好实际的分区
    类型，**不是**`AREA_NONE`；`ResidenceBuilding`（改名自`BuildingBasic`，见
    `Source/Core/populace/populace.md`）这个通用占位类型目前不按分区细分权重，对所有
    `area`一视同仁返回`1.f`，PIE验证时曾经错误假设成"所有lot都是`AREA_NONE`默认值、只给
    `AREA_NONE`非零权重"，导致每个真实lot都查到0权重、一个独立Building都生成不出来，
    已修复），非0权重登记进`lot->AddCandidate(id, weight)`。**园区内部建筑**：遍历`zones`，
    对每个`Zone`的
    `zone->GetMod()->internalBuildings`逐条`buildingFactory.CreateBuilding(spec.type)`
    创建一个独占实例，`PlaceZoneInternalBuilding`落好位置后调用
    `building->Layout(spec.direction)`（`ZoneInternalBuildingSpec`新增的`direction`字段，
    园区内部建筑没有`Assign`挑选的方向，由mod直接声明），成功`AddBuilding`才
    `zone->AddInternalBuilding`。**FillRemainder**：`randomAcreage`/`acreageMinMax`两个
    lambda直接转发`buildingFactory.RandomAcreage(type)`/`GetAcreageMin/Max(type)`（同样
    不需要任何实例），每个成功结果`buildingFactory.CreateBuilding(result.type)`创建独占
    实例，`Layout(-1)`（没有方向概念）。所有这几条路径落地成功后都要`Map::AddBuilding(
    building)`——重名返回`false`就`delete building`（`~Building()`里`DestroyBuilding(mod)`
    会跟着跑）。最后对所有lot调`ClearCandidates()`。**第十五轮迁移新增**：显式占位和
    `FillRemainder`两条路径落地成功后都要设置`boundaryRoads`——前者从`RequestPlacement`的
    `outBoundaryRoads`输出参数拷贝，后者从`FillResult::boundaryRoads`拷贝（这个字段是
    `FillRemainder`重写时新增的，见`geometry.md`）。
  - **`Building`独占持有一个mod实例，和`Zone`完全一样**：曾经有一轮"按类型共享"的设计
    （`explicitPlacements`/`FillRemainder`/Zone内部建筑三条路径产出的同类型`Building`共用
    `InitBuildings()`自己的`scanners`表里同一个实例），是为了配合`candidateWeights`/
    `RandomAcreage()`这类"按类型"查询而引入的；这次这些查询本身改成了不需要任何实例的
    static方法（见上），共享模型不再必要，改回独占——`Building(BuildingFactory* factory,
    BuildingMod* mod)`构造，`~Building()`里`factory->DestroyBuilding(mod)`，`scanners`表
    和函数末尾统一销毁都不再需要，每个mod实例的生命周期完全绑定它独占的`Building`。
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

## Building内部布局（组合/房间/楼层几何/行人导航，第N轮迁移）

`roomFactory`/`componentFactory`(以及`terrainFactory`/`roadnetFactory`/`zoneFactory`/
`buildingFactory`)的mod注册不再是`Map`自己的职责，全部移到`Registry::Get()`(见
`Source/Core/common/registry.md`——不只是map域，`Populace`/`Story`各自的Factory也搬进了
这个单例，同一个问题不能只修map这一处)——这个单例只在整个UE进程生命周期里注册一次，`Map`每次
`EnsureMapGenerated()`构造/析构都不会重新扫描dll。这也让上一轮拆出来的`Map::
InitComponents()`/`InitRooms()`失去了存在理由：那两个函数当初只做mod注册、不生成任何
实例(Room/Component始终是`Building::AssignRoom()`/`ArrangeRow()`里现场创建的)，注册这一步
挪走之后`Map`层再没有任何属于它们的工作，直接删掉了，不再保留成空函数。`InitBuildings()`
开头保留的是`buildingLayoutLibrary.ReadTemplates(Config::GetLayouts())`——这是从磁盘解析
`.layout`模板，不是mod dll注册，仍然每次生成时按需读一遍(`Config::GetLayouts()`见
`common/config.md`，`BuildingLayoutLibrary`见`Source/Core/map/building.md`)。

原来3处`building->Layout(direction)`调用点都改成
`building->Layout(direction, buildingLayoutLibrary, roomFactory, componentFactory,
navResult)`——`Building::Layout()`现在除了解析footprint/楼层高度之外，还会按mod声明的
`AssignFloor`/`AssignRoom`/`ArrangeRow`实例化每层的`Floor`+`Room`+`Component`，并构建
building内部的行人导航图，产出一个`BuildingNavResult`（新建节点+新建连接+`"outside"`
类型的待接路网端点）。调用完之后立刻调`MergeBuildingNavigation(building, navResult)`：

- `result.nodes`直接登记进`Map`自己的`navAnchorNodes`。
- `result.connections`按两个方向都插入`pedestrianNavGraph`（行人边双向，和`InitRoadnet`
  建图同一个约定）。
- `result.outsideNodes`：用`building->GetBoundaryRoad(building->GetDirection())`找到
  building朝向的边界`Road`，把端点投影到该`Road`中心线上求弧长比例`t`（`Road`没有中间
  控制点时直接解析算垂足，O(1)；有控制点才退化成数值采样+局部细化），判断building在
  `Road`哪一侧——但**这里不立即调用`Map::AddRoadAccessNode`**，只是把`(road, t,
  useForwardSide, outsideNode)`记进`pendingBuildingRoadAccess`，真正断开延后到
  `InitBuildings()`三段落地循环全部跑完之后统一调用`FlushPendingBuildingRoadAccess()`
  （PIE验证发现的bug修复，第十三轮迁移）：`InitBuildings()`处理building的顺序（显式占位
  按注册id、权重CDF按`GetLots()`遍历、园区内部按`zones`这个`unordered_map`）和building
  在同一条Road上的实际物理位置（沿road的弧长比例`t`）完全无关，如果哪个building先跑到
  `MergeBuildingNavigation`就立即断开，物理上靠后的building可能先断、把物理上靠前的
  building还没轮到的那一段"剩余尾巴"抢先切掉——`Map::BreakThroughLine`"每次都断当前
  剩余尾巴、`fromAnchor`跟着往通行方向前进"这个设计假设要求同一条车道上的连续断开必须
  按物理顺序进行（side0沿Start→End即`t`升序，side1沿End→Start即`t`降序），否则新插入的
  node会被接到错误的相邻锚点之间，行人贯通线在可视化上出现连线交叉/跳跃，看起来像整条
  Road的导航图都被搞乱了。`FlushPendingBuildingRoadAccess()`按`(road, side, laneIndex)`
  分组（`side`/`laneIndex`用新拆出的纯查询函数`Map::ResolveAccessLane`预判——和
  `AddRoadAccessNode`内部真正断开时共用同一份判定逻辑，不会出现"排序用一套规则、断开用
  另一套规则"的不一致），组内按上述物理顺序排序后才依次调用`AddRoadAccessNode`断开+建
  `Connection`——这样不管building落地/遍历的顺序多乱，同一条Road上的断点次序始终和它们
  的物理位置一致。building内部导航图和道路网导航图从此共享同一个断点，不是"两个图靠坐标
  凑近似"（老工程`Building::BuildNavigation`是"连最近两个角"的近似，这次改掉了）。
  building没有可用边界Road（`GetDirection()`仍然是`-1`，mod没能兜底选出任何方向）时，
  不产生任何pending项，`outsideNode`仍然通过上面`result.nodes`那一步正常登记进
  `navAnchorNodes`（不连道路网而已，不是完全没登记）。
  - **`outsideNode`不能在这里重新`push_back`进`navAnchorNodes`——它本来就已经是
    `result.nodes`的成员之一**（`Building::BuildPedestrianNavigation()`里`resolveEndpoint()`
    解出某个"node"/"line"锚点或Room导航节点时就已经无条件收进了`navOut.nodes`，"outside"
    只是从这些已收集的节点里再挑一份"需要接道路网"的子集，不是另开一份新节点），早期实现
    在这个循环开头多写了一行`navAnchorNodes.push_back(outsideNode);`，导致同一个`Node*`
    在`navAnchorNodes`里出现两次——`~Map()`按`navAnchorNodes`逐个`delete`时对它double
    free，退出游戏必现崩溃（PIE验证发现，第十四轮迁移）。修复就是删掉这一行多余的
    `push_back`，`outsideNode`只在上面`result.nodes`那个循环里登记一次。

**车辆导航（第N轮迁移，`result.vehicleNodes`/`vehicleConnections`/`vehicleOutsideNodes`）**：
和上面行人分支平行处理——`result.vehicleNodes`登记进`navAnchorNodes`；
`result.vehicleConnections`**只按`Connection`自己的`Start→End`方向单向插入
`vehicleNavGraph`**（不能照抄行人"两个方向都插入"，车道本来就有方向性，和`InitRoadnet`给
普通`Road`建车行图同一个"单向、按实际通行方向"的约定）；`result.vehicleOutsideNodes`里每个
`outsideNode`，先在`result.vehicleConnections`里统计它的**入度/出度**（按`Node::GetId()`
比较，某条connection的`GetEnd().GetId()`等于这个`outsideNode`算一次入度，`GetStart()`
等于算一次出度）——**入度为0（只有出边）是出口**：车辆从building内部经这个node开到路上，
最终连接方向`outsideNode→路面access node`（单向）；**出度为0（只有入边）是入口**：车辆
从路面开进building，方向`路面access node→outsideNode`（单向）；入度出度都不为0（这个
node在building内部图里本来就双向都在用）或都为0（孤立点）则**拒绝**接路网，只留在
`navAnchorNodes`里（不产生pending项）——这是用户明确给出的判定规则。边界Road/方向判定
（`GetBoundaryRoad(GetDirection())`+投影+`useForwardSide`符号判断，决定接到路的哪一侧
车道）和行人分支完全复用同一段代码。`Map::PendingRoadAccess`因此新增`isVehicle`/`isExit`
两个字段，`FlushPendingBuildingRoadAccess()`分组key也加入`isVehicle`（车行/人行是完全
独立的车道空间，不应该混进同一组按物理顺序排序），`AddRoadAccessNode`调用时传对应的
`isVehicle`，新`Connection`按`isVehicle`插入`vehicleNavGraph`（单向，按上面判定出的
入口/出口方向）还是`pedestrianNavGraph`（双向，行人分支不变）。`Building`侧的实现是
`Building::BuildVehicleNavigation`，和`BuildPedestrianNavigation`共用同一份算法
（`Building::BuildNavigationGraph`，按`isVehicle`选读`GetPedestrianNavigation`还是
`GetVehicleNavigation`），详见`building.md`"车辆导航"一节。

`InitBuildings()`三段落地循环各自在`MergeBuildingNavigation(building, navResult)`之后紧接着
调一次`ForwardBuildingHatches(building)`，处理**地下室→世界地形的挖洞**：如果这栋building有
basement，取`building->GetFloor(-1)->GetHatches()`（离地表最近的那层basement，不是"每层看
下面一层"这种通用规则），把每个`Hatch`的局部ratio坐标按这栋building的位置/旋转（
`building->LocalToWorld`换算中心，尺寸不用跟着换算——旋转由`AddHatch`的`rotation`参数单独
处理）换算成世界坐标，逐个调用`this->AddHatch(quad, rotation)`——和roadnet隧道口完全复用
同一套挖洞机制（见`Source/Basic/map/roadnet_basic.md`"隧道"一节），不是用来挖building自己
楼层的`Ceiling`/`Ground`（那些照模板原样绘制，不做运行时裁剪）。**这一步之前只写进了设计
文档、代码里一直没有真正调用**——building落地流程里从来没有出现过`GetHatches`/`AddHatch`
字样，PIE验证发现地下室楼梯/电梯/坡道井道对应的位置完全没有在世界地形上开洞，才发现这个
遗漏，第十五轮迁移补上。

## ConnectPathRoad（第十二轮迁移，小路正式接导航图）

`Map::ConnectPathRoad(const PathRoadLink& link)`把`Lot::SplitWithPath`产出的一条小路正式接入
`vehicleNavGraph`/`pedestrianNavGraph`，由`InitZones()`/`InitBuildings()`对每条新产生的
`PathRoadLink`各自记一次——调用点在`request.lot->RequestPlacement(...)`/`lot->FillRemainder(...)`
调用前后各记一次`lot->GetPathRoadLinks().size()`，处理`[之前的size, 现在的size)`这一段新增的
link，不改`RequestPlacement`/`FillRemainder`的函数签名（小路数据只存在`Lot`自己身上这条原则
不变）。**不管`RequestPlacement`最终返回`success`还是`false`都要处理新增的link**——`SplitWithPath`
产生的小路即使整体placement请求失败，也已经是真实持久化的几何（被某个freeLot的边界引用着）。

**这次不再立即调用`ConnectPathRoad`，改成先记进`pendingPathRoadLinks`，`InitZones()`+
`InitBuildings()`全部跑完之后统一调用`FlushPendingPathRoadLinks()`按物理顺序处理**（第N+1轮
迁移，用户报告bug后排查修复）——早期实现确实是发现一条link就立即调用`ConnectPathRoad`，
理由写的是"处理顺序天然=创建顺序（同一顶层`Lot`内部cascading cut时，后一刀如果连到前一刀
新建的小路，前一刀的link一定排在`pathRoadLinks`里更靠前的位置，先于后一刀被处理）"——这句话
只对**同一个顶层`Lot`自己内部**的cascading cut成立，但完全没考虑**不同顶层`Lot`**各自的
小路共享同一条host道路的情况：`InitZones()`/`InitBuildings()`三段落地循环遍历lot的顺序（按
`GetLots()`原始顺序、按free acreage降序、按`zones`这个`unordered_map`遍历顺序）和这些lot的
小路在共享host道路上的实际物理位置（弧长比例`t`）完全无关——如果物理上更靠后的lot先被遍历到、
它的小路先调`ConnectPathRoad`断开，会把物理上更靠前的lot还没轮到的那段"剩余尾巴"抢先切掉，
和`FlushPendingBuildingRoadAccess()`要解决的是完全同一类bug（`Map::BreakThroughLine`"每次都
断当前剩余尾巴、`fromAnchor`跟着往通行方向前进"这个假设被打破），但这次是`ConnectPathRoad`
自己没有同款的延后排序保护。**症状比building access那次更隐蔽**：导航图仍然全联通（Dijkstra
总能找到一条路），不会报错也不会崩溃，只是路径会在断点附近出现"先跳到更远的断点、再折返回近的
断点"这种局部绕路——用户实测复现：一个T字路口市民该往右拐，却先往左跑到下一个路口再掉头往右走，
一度怀疑是路口本身少连了一条边，实际是这条更隐蔽的跨Lot断点顺序错位。

修复：仿照`FlushPendingBuildingRoadAccess()`的思路，但`PathRoadLink`比`PendingRoadAccess`多一层
复杂度——一条link同时占两个"触点"（`endRoad1`/`endT1`、`endRoad2`/`endT2`，可能落在两条不同的
host道路上），不能像building access那样把每个触点当独立工作项直接按`(road,side,laneIndex)`
分组排序完事，因为一条link的两端必须在同一次`ConnectPathRoad`调用里处理（要在这次调用里一起
建好小路自己的4条贯通线）。`FlushPendingPathRoadLinks()`先用`ResolveNearSide()`（`ResolvePathEndAnchors`
开头那段点积判近侧逻辑的纯查询版本，不产生任何副作用）预判每个触点会落在哪条host道路的哪一侧，
按`(road,side)`分组、组内按该侧实际通行方向排序（side0沿Start→End即`t`升序，side1沿End→Start
即`t`降序，和`FlushPendingBuildingRoadAccess`同一个规则），再用组内相邻两个触点的先后关系
构造"link A必须先于link B处理"的依赖边，跑一次Kahn拓扑排序算出link之间的全局处理顺序，最后
按这个顺序依次调用`ConnectPathRoad`。理论上只有很反常的路网几何才会在多条host道路之间形成
排序环，出现环时剩余部分退化成按原始发现顺序处理（不阻塞整个流程，不会崩溃，只是环内那几条
link之间仍可能有本节描述的局部绕路，是已知的兜底简化）。`FlushPendingPathRoadLinks()`调用点
在`InitBuildings()`末尾、`FlushPendingBuildingRoadAccess()`之前（语义上更接近早期"小路在同一次
循环体内先于building导航合并发生"的相对顺序，两者谁先谁后目前没有更强的正确性要求）。

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

## Zone内部布局（第十五轮迁移：围墙/大门/出入口/内部道路/内部建筑）

`ZoneMod`新增`walls`/`gates`/`vehicleEntries`/`vehicleExits`/`pedestrianAccess`/
`internalRoads`/`internalBuildings`七个成员（和`explicitPlacements`平级，字段语义见
`zone_mod.md`），`InitZones()`对每个成功落地的`Zone`都读一遍。围墙/大门是纯数据搬运
（`zone->SetWalls`/`SetGates`，渲染在Forever层）；出入口/内部道路/内部建筑需要Core实际接图
/实例化，靠4个新增私有方法：

- **`ZoneLocalToWorld(zone, x, y)`**：把zone局部坐标（原点在zone矩形中心——注意和`Lot`局部
  坐标系原点在WEST-NORTH角不是同一个约定，这是专属于Zone内部布局这几个结构体的新约定）转成
  世界坐标，标准2D旋转，和`Lot::GetPosition`同一套cos/sin写法（少了`Lot`那边"局部原点在角上
  需要先减半尺寸"那一步，因为这里局部坐标已经是中心原点）。
- **`ConnectZoneAccessPoint(zone, x, y, width, isVehicle, isEntry, anchorCache)`**：给一个
  车行/行人出入口点接图。先转世界坐标，比较到zone四条边（局部坐标下）的距离找最近的一条
  （和`FACE_DIRECTION`一一对应），取`zone->GetBoundaryRoad(direction)`；没有对应边界Road，或
  该侧没有对应类别车道，返回`nullptr`（出入口一定要连到真实道路，连不上说明mod配置有问题，
  不静默退化成孤立锚点——这点和`ResolvePathEndAnchors`遇到无路可退化成孤立锚点不一样，因为
  出入口的语义就是"要连到外面"，没有"孤立端点"这个合法状态）。找到host road后：直线投影算
  弧长比例`t`（和`ProjectT`同样的做法），用zone中心相对该点`perp0`的点积判断"zone在哪一侧"
  （和`ResolvePathEndAnchors`"小路伸向哪一端"点积判断同一个方法，换成"zone中心在哪一侧"），
  取该侧最外侧车道，`ComputeLaneAnchorPosition`算车道中心世界坐标，`BreakThroughLine`断出
  一个node并给road补一个`RoadOpening`标记；再在zone自己的世界坐标点`MakeIsolatedAnchor`一个
  "zone侧"锚点（登记进`anchorCache`，供内部道路端点复用），两个锚点间建一条`Connection`——
  车行按`isEntry`单向（`true`=road->zone，`false`=zone->road），行人双向（忽略`isEntry`）。
- **`ConnectZoneInternalRoad(zone, spec, anchorCache)`**：把`ZoneInternalRoadSpec`实例化成
  一条真正的`Road`（`mesh=""`/`unit=0.f`，和`Lot::SplitWithPath`产的小路同样"不参与
  `BuildRoadInstances`铺设、只连导航图"的约定）。**`spec`只能是"一条车辆单行道"或"一条人行道"
  二选一**（`isVehicle`+单个`width`，不是`vehicleLanes0/1`+`pedestrianLanes0/1`那种多车道
  打包结构——用户明确要求和`vehicleEntries`/`vehicleExits`/`pedestrianAccess`分开指定的模型
  保持一致），固定用side0：`isVehicle=true`时沿Start->End单向插入`vehicleNavGraph`，`false`
  时双向插入`pedestrianNavGraph`。要双向车行/车行+人行都通，`ZoneMod`需要declare多条
  `ZoneInternalRoadSpec`（端点坐标相同的会在`anchorCache`里自动合并，不需要`Map`这边额外
  处理）。锚点位置用`ComputeLaneAnchorPosition`算（t=0/1对应Start/End，side固定0）——单一
  车道时这个公式的偏移量正好抵消成0，等价于直接用中轴线坐标，但保留这个调用是为了和大路/
  小路的锚点算法保持同一套写法。端点如果和`anchorCache`里已有的世界坐标（含车行/行人类别——
  同一个坐标车行和行人各自独立，不会混用彼此的node）在`ZONE_ANCHOR_MERGE_RADIUS_SQ`（1个
  地图单位的平方）容差内重合，复用同一个node；否则`MakeIsolatedAnchor`新建一个并登记进
  cache。这个容差是为了让出入口锚点(`ConnectZoneAccessPoint`产出，落在mod声明的原始坐标)
  和内部道路的锚点能被判定成"同一个位置"自动桥接，是刻意的近似，不是精确匹配。
- **`PlaceZoneInternalBuilding(zone, spec, builtInternalRoads, mod)`**：把
  `ZoneInternalBuildingSpec`实例化成`Building`：`ZoneLocalToWorld`算世界坐标中心，
  `SetPosition`，`building->SetParentZone(zone)`+`building->SetParentLot(zone->GetParentLot(),
  spec.relativeRotation)`（`Building`同时持有`parentZone`和`parentLot`，前者只是反向查询
  登记，`GetRotation()`走的是`parentLot`那条转发链路，`parentLot`直接复用zone自己的
  `parentLot`所以转发基准天然一致，见`zone.md`"building.h"一节），再用`builtInternalRoads`
  （按`spec.roadIndices`的`FACE_DIRECTION->下标`）解析出`Road*`逐个`SetBoundaryRoad`。
  `mod`参数是调用方为这个spec单独`buildingFactory.CreateBuilding(spec.type)`出来的、独占的
  `BuildingMod*`实例（见上"`Building`独占持有一个mod实例"）——`new Building(&buildingFactory,
  mod)`直接用它。这个方法内部不调用`Layout()`——调用方（`Map::InitBuildings()`）在拿到返回
  的`Building*`之后统一调用一次`building->Layout(spec.direction)`。**这个方法在`InitZones()`
  执行期间不能调用**（`buildingFactory`此时还没注册mod），只由`InitBuildings()`遍历
  `zone->GetMod()->internalBuildings`时调用。

`anchorCache`（`vector<tuple<float,float,bool,Node*>>`，`InitZones()`里的局部变量，一个zone
一份）由出入口和内部道路共用——这是让"内部道路端点恰好落在出入口位置"时能自动接上外部道路的
唯一机制：本身没有专门为这两者设计"桥接"逻辑，纯粹靠坐标（含类别）重合就复用同一个node。

## 寻址（Zone/Building/Room按名字/分层地址查找）

`Map::zones`/`Map::buildings`这次改成老工程`Map::zones`/`Map::buildings`同款存储方式——
`unordered_map<string, Zone*/Building*>`，不是`vector`（`GetZones()`/`GetBuildings()`签名
跟着改成返回map，`Source/Forever/Framework/ForeverZoneFrameworkComponent.cpp`/
`ForeverBuildingFrameworkComponent.cpp`两处消费方遍历方式改成结构化绑定）。

- **唯一性由mod自己保证，不是Core生成的**：`Zone::GetName()`/`Building::GetName()`直接转发
  `mod->GetName()`，mod在自己的构造函数里用一个`static int count`给每个实例编号（老工程
  `ResidentialZone::count`/`EmptyZone::count`同款做法），拼进返回值——每个mod实例独占服务
  一个Zone/Building（见上），构造函数只会跑一次，计数器天然对应"这是第几个真正落地的实例"。
- **`Map::AddZone(Zone*)`/`Map::AddBuilding(Building*)`（私有）**：按`GetName()`插入对应的
  map，发现重名直接返回`false`并`debugf`打印警告、不插入——**不生成消歧名字**，唯一性是mod
  自己的责任，这里只是防御性的兜底（正常情况下不应该触发，触发了说明某个mod的计数器实现有
  问题）。调用方（`InitZones()`/`InitBuildings()`三处落地路径）发现`AddZone`/`AddBuilding`
  返回`false`就把这个刚构造的对象`delete`掉——`~Zone()`/`~Building()`会顺带
  `DestroyZone`/`DestroyBuilding`掉它独占的mod实例，不会内存泄漏。
- **`GetZone(name)`/`GetBuilding(name)`**：对`zones`/`buildings`这两个map的简单`find`，
  找不到返回`nullptr`。
- **`LocateZone(address)`/`LocateBuilding(address)`：老工程"Block→Zone→Building"路径地址的
  直接移植**——这次工程里"Block"概念等价于顶层`Lot`：`Roadnet::AllocateAddress()`已经在
  `InitRoadnet()`阶段给每个顶层`Lot`登记了它所有临街道路的`(road,index)`地址对（一个角地块
  可能有多个，`Lot::AddAddress`/`GetAddresses()`），`Map::LocateLot(road,index)`（已有的
  公开方法，转发`Roadnet::LocateLot`）能正确反查——不存在"选哪个地址作为规范代表"的歧义：
  地址字符串由想引用某个zone/building的调用方自己提供（他知道自己想用哪条临街路），角地块的
  好几个`(road,index)`地址全都指向同一个`Lot`，用哪一个都能查到同样的结果。地址格式：
  `"<road> <index> <zoneName>"`定位直接落在这个`Lot`上的`Zone`；`"<road> <index>
  <buildingName>"`定位直接落在这个`Lot`上、没有`parentZone`的`Building`；`"<road> <index>
  <zoneName> <buildingName>"`定位某个`Zone`内部的`Building`（`Zone::GetInternalBuildings()`
  按`name`匹配）。用`istringstream`按空格切分，找不到返回`nullptr`。
- **`LocateRoom(address)`（第N轮迁移补全，`Room`层级的地址解析）**：地址格式在
  `LocateBuilding`的基础上再加一段房间号（`"... <buildingName> <number>"`或
  `"... <zoneName> <buildingName> <number>"`）——取最后一个空格分隔的token当房间号，
  剩下部分拼回字符串直接交给`LocateBuilding`定位`Building`，再在`building->GetRooms()`
  里线性找`GetNumber()`匹配的`Room`，不重复实现一遍"Lot→Zone→Building"那段解析逻辑。
  不需要`LocateComponent`——老工程`Component`本来就没有`GetAddress`，只能通过
  `Building`/`Room`间接找到。配套的正向查询（对象→地址字符串）是`Lot::GetAddress()`/
  `Zone::GetAddress()`/`Building::GetAddress()`/`Room::GetAddress()`这一条链子（之前只有
  反向的字符串→对象查找，没有正向的），`Lot::GetAddress()`固定取`GetAddresses()[0]`
  （角地块有多个地址时任选其一，语义和`LocateLot`一致），其余每一层都是"上一层的地址
  + \" \" + 自己的Name/门牌号"，格式和上面`LocateZone`/`LocateBuilding`/`LocateRoom`的
  解析格式一一对应，照抄老工程`Block::GetAddress`/`Zone::GetAddress`/
  `Building::GetAddress`/`Room::GetAddress`。

## 人口初始化对接（进入populace域，第N+1轮迁移）

`Map`不持有任何`Citizen*`/`Populace*`——`Populace`是和`Map`平级的顶层Core类（不知道`Map`
的存在），`Map`只提供两个方法单向"读"外部传入的`Populace`，和老工程`Map::InitContents()`/
`Map::Checkin(populace, player)`的关系完全对应，详见`Source/Core/populace/populace.md`：

- `int Map::ComputeAccommodationTarget() const`：遍历所有building的所有room，
  `IsResidential()`的加`ResidentialCapacity()`，除以2——老工程`Map::InitContents()`对
  "accomodation"的统计口径（先求和再减半）。供调用方（`AForeverFrameworkActor::
  EnsurePopulaceGenerated()`）算好之后传给`Populace::Init()`。
- `void Map::Checkin(const Populace& populace)`：**两个完全独立的步骤**，"房产归属"决定
  每个zone/building/room的`owner`/`stated`是什么，"住处分配"决定谁实际住在(`tenants`/
  `occupants`)哪个room——一个room的owner可以从来没在这里住过（相当于"房东"），见
  `Source/Core/map/room.md`。
  1. **房产归属**：照抄老工程`Map::Checkin`"Zone→Building→Room逐级下探"的算法——对每个
     zone先`GetRandom(100)`决定"整个zone公有"/"整个zone归一个随机成年citizen私有"/
     "下探到各个building各自独立决定"三选一；下探到building这一级同样三选一（公有/私有/
     下探到各个room各自独立`SetOwner`）。**一旦某一级判定"整体统一归属"，就把owner/
     stated一路级联写到它下面所有building/room**——用户明确要求"如果一个园区/建筑属于
     某人或公有，那么它内部所有房间/建筑都属于这个人或公有"。**如果内部不同room/building
     各自独立归属不同人，上一级的`owner`/`stated`保持默认值（`nullptr`/`false`）**，
     不需要额外"重置"逻辑，天然由"只在统一归属分支才调用setter"这个结构保证。独立于任何
     zone的building单独走一遍同一套building级归属roll，但**跳过已经属于某个zone的
     building**——老工程这里对zone内部building有重复roll的疏漏（会把zone级联下来的
     归属静默覆盖掉），这次没有照抄，详见`Source/Core/populace/populace.md`"房产归属"
     一节（含"公有"概率是这次新加、不是老工程原始数值的说明）。
  2. **住处分配**：遍历所有building的所有room，`IsResidential()`的每个room贡献1个名额
     （不按`ResidentialCapacity()`重复贡献——这个capacity只用来算上面的城市级目标，不在
     这一步重复消费）。**按老工程"一个家庭消费一个room名额"的算法**（不是"一人一间"）：
     只遍历成年（`Citizen::GetAge(populace.GetCurrentYear()) >= 18`）且还没有房间的
     citizen，每人`GetRandom(pool.size())`随机抽一个名额（swap-remove出名额池），配偶
     （`GetSpouse()`非空、还没房间）以90%概率（`GetRandom(10)>0`，老工程同款）跟着搬进
     同一间，未成年子女（`GetChildren()`里年龄<18且还没房间的）无条件一起搬进同一间——
     未成年citizen不会独立抽房间，只能通过父母这边被带进去。成功分配的citizen同时设置
     `Lot`/`Zone`/`Building`/`Room`+`CurrentRoom`（`Building::GetParentLot()`/
     `GetParentZone()`/`Room::GetParentBuilding()`逐级取），并把citizen登记进
     `room->AddTenant()`+`room->AddOccupant()`——**这一步完全不碰`owner`/`stated`**，
     和第1步"房产归属"是两回事。这一版是几次修正过的：第一版`Citizen`没有配偶/子女字段时
     曾经简化成"一人一间随机分配"，被用户指出"所有人都独自一间"不对；之后补上了
     `CurrentRoom`/owner/tenants/occupants；最后被用户指出"房产归属"和"住处分配"是两个
     独立概念、且归属要迁移老工程的zone/building/room级联算法，才有了现在这版，见
     `Source/Core/populace/populace.md`。

调用顺序：`AForeverFrameworkActor::EnsureMapGenerated()`（`map->InitBuildings()`，residential
room数据必须先落地）之后，`EnsurePopulaceGenerated()`里依次
`populace->Init(map->ComputeAccommodationTarget())`→`map->Checkin(*populace)`——两个
`Ensure*Generated()`各自只保证自己的域，靠`BeginPlay()`里显式的调用顺序保证前者先跑完，
详见`Source/Forever/Framework/ForeverFrameworkActor.md`。

## 依赖关系

- 依赖：`terrain.h`、`terrain_factory.h`、`roadnet.h`、`roadnet_factory.h`、`zone.h`、
  `zone_factory.h`、`building.h`、`building_factory.h`、`map/room.h`（`Checkin`要用
  `Room::IsResidential()`/`ResidentialCapacity()`/`GetParentBuilding()`）、
  `populace/populace.h`/`populace/citizen.h`（`ComputeAccommodationTarget`/`Checkin`的
  `Populace`/`Citizen`参数类型——单向依赖，`Populace`/`Citizen`不反过来include任何map域
  头文件）、`map/geometry.h`（`Quad`/`Node`/`Road`/`Lot`等，`hatches`字段类型）、
  `common/config.h`（`InitTerrains`/`InitRoadnet`/`InitZones`/`InitBuildings`读config用）、
  `common/registry.h`（`Map`构造函数绑定6个Factory引用成员，见`registry.md`）、
  `common/utility.h`（`debugf`/`GetRandom`）。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（持有`Map*`，
  `EnsureMapGenerated`时依次调用`InitTerrains`+`InitRoadnet`+`InitZones`+`InitBuildings`；
  `ComputeAccommodationTarget`+`Checkin`挪到了`EnsurePopulaceGenerated`里，见
  `ForeverFrameworkActor.md`）、`Source/Forever/Framework/
  ForeverTerrainFrameworkComponent.h/.cpp`
  （`GenerateTerrain(Map*)`读取生成好的格子数据建mesh）、`Source/Forever/Framework/
  ForeverRoadnetFrameworkComponent.h/.cpp`（`GenerateRoadnet(Map*)`读取`GetRoads()`/
  `GetJunctions()`/`GetLots()`/`GetPathRoads()`/`GetPathRoadMaterial()`建mesh）、
  `Source/Forever/Framework/ForeverZoneFrameworkComponent.h/.cpp`（`GenerateZones(Map*)`读
  `GetZones()`）、`Source/Forever/Framework/ForeverBuildingFrameworkComponent.h/.cpp`
  （`GenerateBuildings(Map*)`读`GetBuildings()`）、`Source/Forever/Framework/
  ForeverPopulaceFrameworkComponent.h/.cpp`（`GenerateCitizens(Map*, Populace*)`缓存
  `populace->GetCitizens()`列表）。

## 待办/后续阶段

- 阶段4：Block/Component/Room迁移时在这个类上继续扩展，具体怎么扩展（加字段还是拆分成多个
  协作的类）留到那几个阶段开始时再定。Zone/Building已经迁移完，见上"InitZones/InitBuildings"
  一节。
- 阶段4：`InitTerrains()`目前假定调用方（`AForeverFrameworkActor::BeginPlay`）已经在此之前
  跑过一次`Config::ReadConfig`（实际上是`ForeverModSubsystem`这个`UGameInstanceSubsystem`
  在game instance启动时做的，早于任何关卡的`BeginPlay`）——这个顺序依赖目前只是约定，没有
  代码层面强制，如果以后出现`Config::ReadConfig`没跑就调用`InitTerrains`的场景，需要补上
  显式检查或调整调用时机。
