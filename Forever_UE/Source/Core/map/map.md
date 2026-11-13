# map.h / map.cpp

## 职责

`Map`是Map域的聚合类：地图尺寸、`Element`格子存储、Terrain/Roadnet/Zone/Building各自Factory的引用、各域的`InitXxx`生成流程、导航图（车行/行人）与寻路、寻址、门/电梯/实体脚本、人口对接（房产归属与住处分配）、格子数据的读写accessor。

`Tick`/`ApplyChange`是Core各域统一要实现的两个方法：Map的`Tick`是空实现（没有每帧逻辑，只保持各域形状一致）；`ApplyChange`返回"是否认识该Change类型"，七个域都不认识时由`AForeverFrameworkActor::ApplyChange`统一打一条警告，域自己不打。

## 关键设计

### 存储与地形

- **用扁平`std::vector<Element>`（行主序，`y*width+x`）存格子，不分块**：默认1024x1024=1048576个`Element`，扁平数组足够。将来要分块（更大地图/流式卸载）只换内部存储，`GetTerrain`/`SetTerrain`等accessor签名不用变。
- **`InitTerrains()`**：按`GetPriority()`降序对所有已注册地形依次执行`DistributeTerrain`，再统一跑一次"3x3晋升"规则，最后重建`terrainTextures`索引表。mod的发现/注册不在这里，归`Registry`（`common/registry.md`）。Core不内置兜底地形，见`terrain.md`。传给mod DLL的回调里`std::string`一律按引用传（`GetTerrainRef`），各模块自己拷贝——按值返回会一边分配一边释放，超过SSO长度（15字符，如`terrain_mountain`）就在释放时崩溃。
- **3x3晋升**：3x3邻域（含自身）全是`plain`/`construction`才把格子改成`construction`；任一邻居越界（地图边缘）则永不晋升。
- **水面按格子独立设置**（`SetWater`）：同一次`DistributeTerrain`里不同格子可以有不同水位（倾斜水面），见`terrain_mod.md`"水面"。
- **细化高度**：`Element::fineHeights`默认为空，只有起伏大的格子（目前是河岸）才分配17x17数组（约定见`terrain_mod.md`），存成`unique_ptr<array>`让没有数组的格子只多8字节（1024x1024约8MB）。mod通过`setFineHeights`回调（`Map::SetFineHeights`）提交。`SampleHeight(x,y)`取`floor`落到的格子：有细化数组就在1/16网格上双线性（UE各级LOD顶点正好落在网格点上，等于直接查表），否则用4个格点粗高度双线性；UE侧`UForeverTerrainFrameworkComponent::SampleHeight`直接转发到这里。`SetHeight`会清掉共用格点`(x,y)`的4块细化数组，保证"数组角点=粗高度"，所以mod必须先`SetHeight`再`SetFineHeights`。`GetHeight`只返回粗高度。
- **hatch（地形挖洞）**：类型是`TerrainHatch`（`quad`+`rotation`+`wall`+`floorStart`+`floorEnd`，见`general_geometry.h`）。`AddHatch(const TerrainHatch&)`是主入口，按`quad`的旋转AABB算出重叠的格子范围，逐个`Element::hatches.push_back`；`AddHatch(Quad,float)`（建筑地下室用）包成`wall=false`。`GetTunnelWallHatches()`返回路网里`wall=true`的hatch（隧道口），UE层据此生成侧壁。存档里每个hatch存`wall`(U8)+`floorStart/floorEnd`(F32)，Element和Roadnet两处共用`WriteHatches/ReadHatches`。
- **地形贴合`ApplyTerrainFits(fits)`**：路面高度由mod指定，地形去贴路面（`InitRoadnet`里hatch转发之后调用）。对每条请求的每段折线，枚举它外扩（半宽+falloff）范围内的细化格点（间距1/16 element，和UE地形L0顶点重合），取到折线的最近距离d：d<=半宽权重1、目标高度=最近点路面Z；半宽<d<半宽+falloff权重按smoothstep从1降到0；其余不动。同一格点被多段/多请求命中取权重最大的，权重相同（路面宽度内恒为1）取离折线最近那一段的路面Z——只比权重会让"最先遍历到的一段"占住整片路面，地形变成台阶。随后：①被碰到但没有`fineHeights`的element先用`SampleHeight`建出17x17（必须在写入任何高度之前全部建完，且先算进局部数组再挂上去，否则`SampleHeight`会读到半填的零数组）；②每个格点新高度=lerp(原高度, 路面Z, 权重)，写进所有共用该格点的element数组（整数边界上的格点属于相邻最多4个element）；③element左下角整数格点同时写粗高度。**不经`SetHeight`**（它会清掉邻格细化数组）。路面比地形高或低都一样。结果随`Element`存档（`height`/`fineHeights`），请求本身不存、读档不重算。

### 植被与路灯

- **`InitVegetation()`是整张地图最后一个生成阶段，必须在`InitBuildings()`之后**（行道树要避开`Road::GetOpenings`，此时Zone/Building/小路加的开口都已齐全）：`roadnet->LayoutTrees()`展开行道树，再对每栋building调`BuildingMod::LayoutTrees`、对每个园区调`ZoneMod::LayoutTrees`；三者的树请求（单棵/一排/一片）按各自参考系（`TreeFrame`：路网=整张地图不旋转、ratio相对地图宽高；building/zone=各自局部坐标）由`Vegetation::ExpandRequests`展开。过滤规则`CanPlaceTree`：越界、有水面、有hatch的格子不种。UE层通过`GetVegetation()`一次性读取，见`vegetation.md`。
- **`InitLamps()`**紧跟其后：目前只有路网宿主，`roadnet->LayoutLamps()`用整图参考系由`Lamps::ExpandRequests`展开，过滤同`CanPlaceTree`。UE层通过`GetLamps()`读取，见`lamp.md`。

### InitRoadnet与导航图

- **`InitRoadnet()`只能执行一次**：`roadnet`已存在（本次生成过，或读档由`MapSaveCodec`还原）时记日志直接返回，不重复构建，避免泄漏上一个`Roadnet`及重复灌导航图；`roadnet`由`~Map()`释放。
- **`InitRoadnet()`必须在`InitTerrains()`之后**：`RoadnetMod::DistributeRoadnet`要通过`getTerrain`/`getWater`/`getHeight`三个回调采样已生成的地形（`getHeight`就是`Map::SampleHeight`，含细化高度）。路面高度由mod自己决定，需要地形迁就路面时由mod提交`TerrainFitRequest`。流程：①按`Config::GetConceptMods("roadnet_mods")`启用mod；`RoadnetFactory::GetRoadnet()`单选唯一启用的mod（见`roadnet_factory.md`），config没配时退化选第一个被注册的mod（避免PIE里完全没有路网）；都没有则打警告返回；②`new Roadnet`，`DistributeRoadnet`+`AllocateAddress`；③`roadnet->GetHatches()`逐个转发`AddHatch`（目前唯一来源是隧道口，见`Source/Basic/map/roadnet_jing.md`"隧道"），再`ApplyTerrainFits`；④按每个`Intersection`收集相连的`Road`，`RoadJunction::Build`建路口（车行/行人锚点+路缘角点）；⑤给每条`Road`建贯通线、给每个`RoadJunction`建内部连接，一起构成`vehicleNavGraph`/`pedestrianNavGraph`。设计理由见`roadnet.md`。
- **贯通线的锚点来源（`resolveAnchor`三级）**：该端有`RoadJunction`用其逐车道锚点；是地图边缘`extern`则直接用extern自己的`Node`（所有车道退化成一点，边缘不需要精确横向偏移）；都不是则**现算"直接经过"锚点**——`JingRoadnet`的隧道口把一条路拆成三段独立`Road`，中间分段点（`flatNode`/`splitNode`）只是几何过渡，不是`Intersection`。不能把它登记成`Intersection`走`RoadJunction::Build`（会按`setback`裁剪并摆强制水平的路口平面，分段点可能卡在S形下坡中间），也不能像extern那样把车道收成一点（两侧车道数/宽度一致，应按真实宽度摆开）。现算取该端切线的右手垂线，按`LaneCenterOffset`/车道宽度算横向偏移（`setback`=0）；`passthroughAnchorCache`按`(端点id, 车行/行人, side, laneIndex)`缓存，前一段终点和后一段起点用同一key，两条贯通线因此接到同一个`Node*`（两段在分段点切线一致，所以哪一段现算结果都一样，缓存只为保证返回同一个Node）。新`Node`推进`navAnchorNodes`。
- **所有`Node`/`Connection`的所有权**：锚点`Node`都登记在`navAnchorNodes`，`~Map()`统一释放；`Connection`从两张导航图遍历并用set去重后释放（行人边双向插入，同一个`Connection*`会被两条邻接记录引用）。`outsideNode`之类已经在`result.nodes`里的节点不能再`push_back`一次，否则`~Map()`double free。
- **导航图只读访问**：`GetVehicleNavGraph()`/`GetPedestrianNavGraph()`/`GetNavAnchorNodes()`供Forever层可视化（`ForeverRoadnetFrameworkComponent`）和寻路使用。图里的id除了`GetNavAnchorNodes()`能查到，还可能是地图边缘的extern残端（在`GetExterns()`里），调用方要两个列表都查。
- **寻路**：`FindPath(graph, from, to, coords)`是共享核心，边权=`Connection::CalcDistance()`（弧长，比端点欧氏距离准）；`coords`非空时按A*（启发值=到终点的水平直线距离，不超过真实长度，保证最优）。`FindPedestrianPath`用`nodeIndex`缓存（"id->Node*"，由`navAnchorNodes`+`GetExterns()`建，节点数变了重建；线程池里多个任务并发调用，检查/重建时持`nodeIndexMutex`，之后只读）。**不变量**：解锁后读`nodeIndex`没有锁保护，靠"`nodeIndex`只在地图生成阶段重建、那时还没有异步寻路"这一约定成立；任何会增删锚点/externs的改动都必须发生在异步寻路开始之前。不连通/id不存在返回空，调用方（`UForeverPopulaceFrameworkComponent::RequestWalk`）要处理这个兜底，见该文件的`.md`"市民走路"。`FindVehiclePath`不传coords（纯Dijkstra），返回`Connection*`序列（公交线路要拼接真实车道几何，见`traffic/route.md`）；起终点相同视为无路径。
- **`ProjectPointOntoRoad(road,x,y)`**：投影到中心线求弧长比例t（裁剪[0,1]）。没有中间控制点（直线，井字路网绝大多数路段）直接解析，O(1)；有控制点的曲线（如隧道引道S形）退化成粗采样32段+4轮局部细化。

### 导航图与贯通线（AddRoadAccessNode / ThroughLine / BreakThroughLine）

- **`ThroughLine`**（`throughLines[road][4]`，下标0=车行side0、1=车行side1、2=行人side0、3=行人side1）记录每条Road当前的贯通线。**每条物理车道都有自己独立的entry**，不止最内侧或单行道两端车道；`fromAnchor`/`toAnchor`按该方向实际通行方向排列（side0沿Start->End，side1沿End->Start），车行取该车道在`RoadJunctionApproach::vehicleInbound`/`vehicleOutbound`（逐车道`vector`，见`roadnet.md`"车道级导航锚点"）里的专属锚点——只加贯通线而不给独立锚点，几何上仍会因共用端点重叠成一条线。`laneIndex`是该side车道数组里的下标。**同一个laneIndex可以有多个entry，共同构成从t=0到t=1连续无缝的链**：`tLo`/`tHi`记录这一段在road弧长参数上的范围（side0的`fromAnchor`在tLo端、side1相反）。初始建图/`ConnectPathRoad`/`ConnectZoneInternalRoad`时整条车道只有一段（0~1）。
- **`BreakThroughLine(road,isVehicle,side,laneIndex,x,y,z)`**是`AddRoadAccessNode`和`ConnectPathRoad`共用的底层原语：把(x,y)投影成t，在`throughLines`里找laneIndex匹配且`[tLo,tHi]`跨过t的那一段，删掉旧edge（图上的边一起删），在(x,y,z)新建node，换成两段更短的entry，返回新node；断点几乎正好落在这一段某个既有端点上（约10cm）时直接复用那个端点。每次调用自包含（投影t定位->删段->拆两段），**与调用顺序无关**；若实现成"每车道只记一条剩余尾巴、断完回写fromAnchor"，则要求所有break严格按物理顺序发生，而真实调用方（building/zone出入口、小路接大路）的遍历顺序与物理位置无关，会出现node只连一端、旧edge没删干净的错乱。不要改回那种实现。
- **`AddRoadAccessNode(Road*, t, isVehicle, useForwardSide, openingWidth)`**是给Building/Zone用的公开API：在road的t处为车行/行人图新增访问点，车行访问点同时`road->AddOpening`记一个`RoadOpening`（`forwardSide`记真正落到的side，不是请求朝向；Forever层据此只在这一侧开口），**行人访问点不记开口**，只接导航图。参数是`Road*`不是路名——小路（`Lot::SplitWithPath`产物）全部共用名字`"path"`，且不在`roadnet->GetRoads()`里，按名字既找不到也区分不了。先由`ResolveAccessLane`选出目标车道，再把它自己的贯通线在t处切两段：双向路（对侧也有同类车道）固定选最外侧车道（"新访问点代表外侧车道"）；单行路（对侧没有同类车道）按`useForwardSide`选居中后有符号横向偏移最大（`true`=最靠右）/最小（`false`=最靠左）的车道，居中换算同`RoadJunction::Build`（`roadnet.md`"车道居中"）。`useForwardSide`的语义：请求的side本身有车道时`true`=side0（右手边）、`false`=side1；请求的side是单行道空侧时重新解释成"要右边/左边的车道"，从对侧取。`road`为空或两侧都没有对应类别车道返回`nullptr`。`ResolveAccessLane`是从中拆出的纯查询版本，`FlushPendingBuildingRoadAccess`排序前用它预判落点，必须与真正执行一致，所以共用一份实现。**约定**：`PendingRoadAccess.useForwardSide`是building侧原始请求的朝向，`FlushPendingBuildingRoadAccess`分组/排序用解析后的side，但调用`AddRoadAccessNode`时传的仍是原请求朝向，不能由解析后的`side==0`反推（单行道请求落在空侧、回退到对侧时两者相反，会让多车道单行道选到反方向的极值车道、`RoadOpening.forwardSide`取反，与分组预判的车道不一致）。
- **`ComputeLaneAnchorPosition(road,t,isVehicle,side,laneIndex)`**：road在t处某条车道/人行道的中心世界坐标（z调用方取`road->GetPoint(t)`）。人行道物理上在同侧车行道+停车道**外侧**，基准要先加`SumWidths(车行)+SumWidths(停车)`，和`RoadJunction::Build`里`pedestrianSide`锚点同一套算法，不要自己另写偏移公式；车道居中偏移`shift=(side0宽-side1宽)/2`与`roadnet.cpp`/`RoadJunction::Build`一致（`ComputeLaneAnchorPosition`和`ResolveAccessLane`各自按同一公式取`shift`，两处必须保持一致，见`roadnet.md`"车道居中"）；在大路/小路上断出新node必须用它，不能用`road->GetPoint(t)`的纯中轴线坐标，否则新node贴着中轴线，与车道自己的`fromAnchor`/`toAnchor`连起来会锯齿。
- **右手边约定**：本项目世界坐标+Y=南，右手边（`perp0`）公式是`(-tdy, tdx)`，不是+Y=北时的`(tdy, -tdx)`（朝南(0,1)时右手边是西(-1,0)）。`map.cpp`/`roadnet.cpp`里所有用到`perp0`的地方必须一致，否则路口/zone出入口/小路旁路与车道锚点对不上。

### InitZones

- 必须在`InitRoadnet()`之后（要用`GetLots()`）。每个已注册zone类型：对按剩余空闲面积降序排序的lot列表调一次static `ZoneMod::Assign(lots, emit, context)`，一次扫完全图拿到该类型想要的全部`LotPlacementRequest`（不存在任何`ZoneMod`实例）；逐条`lot->RequestPlacement(...)`，**成功才`CreateZone(id)`创建唯一一次、被长期持有的实例**，交给新建的`Zone`。"要不要这块lot"完全不需要构造实例来回答。`EmitPlacementRequest`是`map.cpp`里的静态自由函数（`push_back`到`vector<LotPlacementRequest>`），编译在Core这一侧，mod通过`PlacementEmitFunc`裸函数指针回调它，`push_back`用Core自己的分配器，不会出现"mod分配、Core释放"的跨DLL问题（见`Dependence/map/zone_mod.md`）。
- 落地后：`SetPosition`、`SetParentLot`、从`RequestPlacement`的`outBoundaryRoads`逐个`SetBoundaryRoad`；`zone->Layout(request.direction)`（转发`mod->Layout`，填好`walls`/`gates`/`vehicleEntries`/`vehicleExits`/`pedestrianAccess`/`internalRoads`/`internalBuildings`；`Assign`只管"要不要、往哪摆"，`Layout`只管"摆下去后长什么样"，且只在真正保留的实例上跑一次）；`BuildDoors`；`BuildZoneScript`；出入口逐点`ConnectZoneAccessPoint`、内部道路逐条`ConnectZoneInternalRoad`并`zone->SetInternalRoads`；最后`AddZone`，重名返回false就`delete zone`（`~Zone()`里`DestroyZone(mod)`会释放mod实例）。围墙/大门无需搬运，`Zone::GetWalls()`/`GetGates()`直接转发zone持有的mod。**Zone阶段不对lot做权重/`FillRemainder`填充**。
- **园区内部建筑（`mod->internalBuildings`）不在`InitZones()`里实例化**，统一由`InitBuildings()`遍历`zone->GetMod()->internalBuildings`落地（`Layout`要用`InitBuildings()`开头才加载的`buildingLayoutLibrary`；zone独占持有mod，数据不会失效，Zone不需要另存副本）。`PlaceZoneInternalBuilding`因此只由`InitBuildings()`调用。

### InitBuildings

- 必须在`InitZones()`之后（用到裁剪完的剩余空闲面积）。开头`buildingLayoutLibrary.ReadTemplates(Config::GetLayouts())`——解析磁盘`.layout`模板，不是mod注册，每次生成都读一遍（`BuildingLayoutLibrary`见`building.md`）。
- 与`InitZones`同构，**没有mod实例的预构造**，顺序为：①**显式占位**：每个building类型调`buildingFactory.Assign(id, GetLots(), ...)`拿全部占位请求，逐条`RequestPlacement`，成功才`CreateBuilding(id)`、`SetPosition`+`SetParentLot`+`SetBoundaryRoad`、`Layout(request.direction)`；②**权重登记**：对每个类型、每个lot调`buildingFactory.GetPower(id, lot->GetArea())`（static，`RoadnetMod`已给每个lot用`Lot::SetArea()`标好分区类型），非0权重登记`lot->AddCandidate(id, weight)`；③**园区内部建筑**：遍历`zones`，对每个spec单独`CreateBuilding(spec.type)`出独占实例，`PlaceZoneInternalBuilding`落位，`Layout(spec.direction)`（内部建筑没有`Assign`挑选的方向，由mod声明）；④**FillRemainder**：对每个lot，`randomAcreage`/`acreageMinMax`转发`buildingFactory.RandomAcreage(type)`/`GetAcreageMin/Max(type)`，每个结果`CreateBuilding(result.type)`、`Layout(-1)`（没有方向概念，mod可能兜底选方向），最后`lot->ClearCandidates()`。边界Road：显式占位从`RequestPlacement`的`outBoundaryRoads`拷贝，`FillRemainder`从`FillResult::boundaryRoads`拷贝。
- 每个落地路径之后依次：`MergeBuildingNavigation`、`ForwardBuildingHatches`、`BuildDoors`、`BuildCabins`、`BuildBuildingScripts`、`AddBuilding`（重名返回false就`delete building`，`~Building()`里`DestroyBuilding(mod)`会释放mod实例）。三段落地循环全部结束后依次`FlushPendingPathRoadLinks()`、`FlushPendingBuildingRoadAccess()`（先小路后building access：后者依赖的`BreakThroughLine`同样读写`throughLines`，小路先接好）。
- **`Building`独占一个mod实例，和`Zone`一样**：`Building(BuildingFactory*, BuildingMod*)`构造，`~Building()`里`factory->DestroyBuilding(mod)`；`RandomAcreage`/`GetPower`等"按类型"查询都是不需要实例的static方法。
- **自适应占位**：`request.adaptiveParent`非空时，`request.lot`是`Lot::FindAdaptivePlacement`（在mod DLL内纯读）命中的嵌套子块`usedChild`。`RequestPlacement`成功后调`PromoteAdaptiveRemainder(parent, usedChild)`：把`usedChild`的残余`freeLots`搬回`parent->GetFreeLots()`，把`usedChild`自己从`parent->GetFreeLots()`摘掉（否则它仍带着当初完整未用的尺寸，下一个类型会当成整块可用，重复分配同一块地），并把它新产生的小路（`TakePathRoadLinks()`）记到`parent`身上（Forever层扫的是`GetLots()`里的顶层Lot）。`usedChild`本身不`delete`（`building->SetParentLot(usedChild)`还引用着）：新开局无人持有、泄漏到进程结束，读档时进`orphanLots`由Map持有。这个函数只能在Core编译的`map.cpp`里做，mod DLL不能调`GetFreeLots()`/`TakePathRoadLinks()`这类会分配/移动容器内容的方法（原因见`general_geometry.h`）。子块没有路网分配的地址，`GetAddresses()`为空会让`building->GetAddress()`退化成空路名、按地址找房间失败，所以沿用`adaptiveParent`的地址；`LocateBuilding`用`SameBlock`（指针相同或地址相同）比较所属地块。
- **小路归属**：`Map`自己不持有任何小路`Road*`。小路是`RequestPlacement`/`FillRemainder`裁剪顶层`Lot`空闲空间的副产品，记在那个`Lot`的`pathRoadLinks`里（`GetPathRoadLinks()`/`GetPathRoads()`），由`Lot`析构释放；`Road::IsPathRoad()`标分类；`Map::GetPathRoads()`只遍历`GetLots()`汇总，按值返回。

### Building内部布局（组合/房间/楼层几何/行人导航）

`roomFactory`/`componentFactory`等Factory的mod注册全在`Registry::Get()`（`common/registry.md`），`Map`每次构造/析构都不重新扫描dll。三处`building->Layout(direction, buildingLayoutLibrary, roomFactory, componentFactory, navResult)`除了解析footprint/楼层高度，还按mod声明的`AssignFloor`/`AssignRoom`/`ArrangeRow`实例化每层的`Floor`+`Room`+`Component`，并构建building内部导航图，产出`BuildingNavResult`（新建节点+连接+`"outside"`类型的待接路网端点）。调用完立刻`MergeBuildingNavigation(building, navResult)`：

- `result.nodes`/`result.vehicleNodes`登记进`navAnchorNodes`；`result.connections`双向插入`pedestrianNavGraph`；`result.vehicleConnections`**只按Connection自己的Start->End单向插入`vehicleNavGraph`**（车道有方向性，方向由`Building::BuildVehicleNavigation`按模板数据算好，Map不重新判断）。`Building`侧的实现是`BuildPedestrianNavigation`/`BuildVehicleNavigation`，共用`Building::BuildNavigationGraph`（按`isVehicle`选读行人/车辆导航数据），见`building.md`。
- `result.outsideNodes`（行人）以及`result.vehicleEntranceNodes`/`vehicleExitNodes`（车行，方向由layout的`entrance`/`exit`端点直接给出；同一个node可同时在两个清单里，各产生一个开口）：用`building->GetBoundaryRoad(building->GetDirection())`找到朝向的边界Road，`ProjectPointOntoRoad`求t，用法向偏移符号判断building在哪一侧（`>=0`为side0，`useForwardSide=true`，同`RoadJunction::Build`的`perp0`约定）。**这里不立即`AddRoadAccessNode`**，只记进`pendingBuildingRoadAccess`（`PendingRoadAccess`有`isVehicle`/`isExit`两个字段）。building没有可用边界Road（`GetDirection()`仍为-1，mod没能兜底选方向）时不产生pending项，端点仍通过`result.nodes`登记进`navAnchorNodes`（只是不连路网）。
- **小路接小路的处理顺序（2026-11-08修复）**：`FlushPendingPathRoadLinks`用Kahn拓扑排序决定`ConnectPathRoad`顺序。除了同一(road,side)内按物理位置的依赖，还要保证"host是小路时，host自己的link先于挂在它上面的link"：小路的`throughLines`在它自己的`ConnectPathRoad`里才建，host还没建时`BreakThroughLine`找不到线段返回nullptr，这一端退化成孤立锚点，整条小路和它旁边的建筑从行人图里脱开（修复前约13%的建筑孤立、25%的寻路失败，全是小路上的建筑；EconSim `pathtest` 模式可复现统计，修复后失败为0）。小路没有宿主路的那一端（`hostRoad==nullptr`）是自由端，退化成孤立锚点是预期的。
- **`FlushPendingBuildingRoadAccess()`**（`InitBuildings()`末尾调用）：先用`ResolveAccessLane`预判每项最终落在哪条车道，按`(road, side, laneIndex)`分组（`isVehicle`是第一排序键，车行/人行是独立车道空间），组内按该车道通行方向排序（side0沿Start->End即t升序，side1沿End->Start即t降序）再依次`AddRoadAccessNode`（开口宽度固定`kAccessOpeningWidth=1`）+建`Connection`：人行双向；车行单向，出口`outsideNode->access node`、入口`access node->outsideNode`。**为什么要延后排序**：三段落地循环的遍历顺序（显式占位按注册id、权重按lot、园区内部按`zones`这个`unordered_map`）与building在road上的物理位置无关；若就地断开，物理上靠后的building可能先断，把靠前building还没轮到的"剩余尾巴"抢先切掉，断点次序与物理顺序错位，行人贯通线在可视化上出现连线交叉/跳跃。building内部导航图和路网导航图因此共享同一个断点，不是靠坐标凑近似。

`ForwardBuildingHatches(building)`（每个落地路径里`MergeBuildingNavigation`之后）处理**地下室->世界地形的挖洞**：building有basement（`GetBasementCount()>0`）时，只取`building->GetFloor(-1)`（离地表最近的那层basement，越往下离地表越远，只有最上面那层会凿穿室外地表）的hatch，局部ratio坐标经`building->LocalToWorld`换算成世界中心（尺寸不用换算，旋转由`AddHatch`的`rotation`参数单独处理），逐个`AddHatch(quad, building->GetRotation())`——与roadnet隧道口复用同一套挖洞机制。不用于挖building自己楼层的`Ceiling`/`Ground`（那些照模板原样绘制）。

### ConnectPathRoad（小路正式接导航图）

`ConnectPathRoad(const PathRoadLink& link)`把`Lot::SplitWithPath`产出的一条小路接入导航图。调用点在`InitZones()`/`InitBuildings()`的每次`RequestPlacement`/`FillRemainder`前后各记一次`lot->GetPathRoadLinks().size()`，把新增的link记进`pendingPathRoadLinks`——**不管placement最终成功还是失败都要记**（`SplitWithPath`的小路即使整体请求失败也已是持久化的几何，被某个freeLot的边界引用着）。三段落地循环结束后统一`FlushPendingPathRoadLinks()`。

**为什么不就地调用、要按物理顺序处理**：lot的遍历顺序（`GetLots()`原序、按free acreage降序、`zones`哈希序）与小路在共享host道路上的物理位置（弧长t）无关；物理上靠后的先断会抢切靠前者的剩余尾巴，与`FlushPendingBuildingRoadAccess`同类问题。**这类bug更隐蔽**：导航图仍全联通，不报错不崩溃，只是路径在断点附近"先跳到更远的断点再折返"出现局部绕路。`FlushPendingPathRoadLinks()`因一条link占两个touch（`endRoad1/endT1`、`endRoad2/endT2`，可能在不同host road上）、两端必须在同一次`ConnectPathRoad`里处理（要一起建小路自己的4条贯通线），不能把touch当独立工作项排序，所以：用`ResolveNearSide()`（`ResolvePathEndAnchors`开头近侧判定的纯查询版）预判每个touch落在host的哪一侧，按`(road, side)`分组、组内按通行方向排序，用组内相邻touch构造"link A先于link B"的依赖边，Kahn拓扑排序得到link的全局处理顺序，再依次`ConnectPathRoad`。路网几何很反常时才会出现环，环内剩余部分按原始发现顺序追加（不阻塞不崩溃，环内仍可能有局部绕路，是已知兜底）。调用点在`InitBuildings()`末尾、`FlushPendingBuildingRoadAccess()`之前。

三条规则：

1. 小路横断面固定为中轴线两侧各0.3单位车道、再往外0.2单位人行道（`PathLaneSpec`，`general_geometry.h`）。
2. **小路接"大路"（`endRoad`非空且`!IsPathRoad()`）**：用小路伸向自己另一端的方向与大路在`endT`处`perp0`的点积判断最靠近小路的一侧（`dot>=0`为side0，否则side1）——小路总是往所属Lot空闲空间那一侧延伸，天然指向近侧。只处理这一侧：车行道断出2个node（`fromAnchor->Nin->Nout->toAnchor`，保留大路直行不中断），`Nin`接小路"驶入"方向车道、`Nout`接"驶出"方向车道；人行道同理断出`Pa`/`Pb`接小路两条人行道（人行边双向，不区分驶入/驶出）。近侧选车道用最外侧（下标size-1）；近侧没有该类车道（单行道）而退化用远侧时选下标0（紧贴中心线那条，小路此时紧贴中心线这一侧；若仍选outermost，所有退化到同一远侧的小路会挤到同一条车道上）；两侧都没有就退化成孤立锚点。`Nin`/`Nout`（`Pa`/`Pb`）不能在同一坐标上断两次——会得到两个重合的node，小路自己的side0/side1贯通线首尾重合成一条线（视觉上只有中轴线一条车道）。所以沿小路自身`pathPerp0`方向按车道/人行道半宽各偏移一点，让两个断点落在小路两条车道各自的延长线上。**`pathPerp0`必须用小路自己Start->End方向算，不能用"离开当前端、伸向另一端"的方向**（后者在Start/End端相反，side0/side1会在两端对应物理上相反的两侧，两端同一下标拼成一条线时交叉）。
3. **小路接"小路"（`endRoad`非空且`IsPathRoad()`）**：被连小路的近侧、远侧车行道/人行道各断出2个node，共4个车行node：近侧2个直接按规则2接新小路的2条车道；远侧2个不直接接新小路，而是单向搭桥到近侧对应node（远进入点->近进入点、近合并点->远合并点），让被连小路远侧车流也能绕到近侧拐进新小路，同时不影响其自身两侧车道的直行连接。人行道套用同一近/远侧模型（远侧与近侧对应node之间双向搭桥）。

"断出2个node"就是对同一`(road,isVehicle,side,laneIndex)`连续调两次`BreakThroughLine`，天然形成`fromAnchor->Nin->Nout->toAnchor`，无需额外拼接逻辑。这条路径**不调`Road::AddOpening`**：`SplitWithPath`创建小路时已在大路上标过跨越整个`pathWidth`的开口。

`ResolvePathEndAnchors(path, isStartEnd, hostRoad, hostT, outVeh, outPed)`解出小路在某一端的4个锚点（车行side0/1、人行side0/1）；`hostRoad`为空=孤立端点（4个新node，位置按小路自己的`perp0`偏移）。`ConnectPathRoad`拿两端各4个锚点后建小路自己的4条贯通线（车行side0 Start->End、side1 End->Start，人行两侧各自双向，与`InitRoadnet`对普通Road的规则一致），登记进`throughLines[path]`。小路自己两侧人行道之间不建"穿过小路本身"的横道（真实`RoadJunction`会建，小路不需要）。

### Zone内部布局（围墙/大门/出入口/内部道路/内部建筑）

`ZoneMod`的`walls`/`gates`/`vehicleEntries`/`vehicleExits`/`pedestrianAccess`/`internalRoads`/`internalBuildings`七个成员（字段语义见`zone_mod.md`），`InitZones()`对每个成功落地的`Zone`读一遍。围墙/大门纯数据搬运（渲染在Forever层）；出入口/内部道路/内部建筑需要Core实际接图/实例化：

- **`ZoneLocalToWorld(zone,x,y)`**：zone局部坐标（**原点在zone矩形中心**，与`Lot`局部坐标系原点在WEST-NORTH角不是同一约定，只用于Zone内部布局这几个结构体）转世界坐标，标准2D旋转，与`Lot::GetPosition`同一套cos/sin写法。
- **`ConnectZoneAccessPoint(zone,x,y,width,isVehicle,isEntry,anchorCache)`**：比较到zone四条边的局部距离找最近的一条（对应`FACE_DIRECTION`），取`zone->GetBoundaryRoad(direction)`；没有对应边界Road或该侧没有对应类别车道时返回`nullptr`（出入口一定要连到真实道路，连不上说明mod配置有问题，不静默退化成孤立锚点——与`ResolvePathEndAnchors`遇到无路可退化成孤立锚点不同）。找到host road后：用`ProjectPointOntoRoad`算t（曲线host也准确）；用zone中心相对该点`perp0`的点积判断zone在哪一侧；取该侧最外侧车道，`ComputeLaneAnchorPosition`+`BreakThroughLine`断出node，车行再补一个`RoadOpening`（`forwardSide`=落到的side是否为0；行人不记开口）；再在zone自己的世界坐标点`MakeIsolatedAnchor`一个"zone侧"锚点（登记进`anchorCache`），两个锚点间建一条`Connection`：车行按`isEntry`单向（true=road->zone），行人双向（忽略`isEntry`）。
- **`ConnectZoneInternalRoad(zone,spec,anchorCache)`**：把`ZoneInternalRoadSpec`实例化成真正的`Road`（`mesh=""`/`unit=0.f`，与小路同样"不参与`BuildRoadInstances`铺设、只连导航图"）。`spec`只能是"一条车辆单行道"或"一条人行道"二选一（`isVehicle`+单个`width`），固定用side0：车行沿Start->End单向插入`vehicleNavGraph`，人行双向插入`pedestrianNavGraph`。要双向车行/车行+人行都通，`ZoneMod`需声明多条`ZoneInternalRoadSpec`（端点相同的在`anchorCache`里自动合并）。锚点位置用`ComputeLaneAnchorPosition`（t=0/1，side=0；单车道时偏移恰为0，等价于中轴线坐标，保留调用只为与大路/小路同一套写法）。端点与`anchorCache`里已有世界坐标（含车行/行人类别——同坐标车行和行人各自独立）在`ZONE_ANCHOR_MERGE_RADIUS_SQ`（1个地图单位的平方）内重合就复用同一个node，否则`MakeIsolatedAnchor`新建并登记。容差要大于车道偏移量级（0.15~0.4），才能让出入口锚点（mod声明的原始坐标）与带偏移的车道锚点自动桥接，又远小于zone尺寸（不会误合并），是刻意的近似。
- **`PlaceZoneInternalBuilding(zone,spec,builtInternalRoads,mod)`**：`ZoneLocalToWorld`算世界中心，`SetPosition`，`SetParentZone(zone)`+`SetParentLot(zone->GetParentLot(), spec.relativeRotation)`（`GetRotation()`走`parentLot`转发链，直接复用zone的`parentLot`所以基准天然一致，见`zone.md`），再用`builtInternalRoads`按`spec.roadIndices`（`FACE_DIRECTION`->下标）解析`Road*`逐个`SetBoundaryRoad`。`mod`是调用方为这个spec单独`CreateBuilding(spec.type)`出来的独占实例。内部不调`Layout()`，由`InitBuildings()`拿到返回的`Building*`后统一调。

`anchorCache`（`vector<tuple<float,float,bool,Node*>>`，`InitZones()`里每个zone一份）由出入口和内部道路共用，这是"内部道路端点恰好落在出入口位置时自动接上外部道路"的唯一机制：没有专门的桥接逻辑，纯靠坐标（含类别）重合复用同一个node。

### 寻址（Zone/Building/Room按名字/分层地址查找）

`zones`/`buildings`是`unordered_map<string, Zone*/Building*>`（`GetZones()`/`GetBuildings()`返回map）。

- **名字唯一性由mod自己保证**：`Zone::GetName()`/`Building::GetName()`转发`mod->GetName()`，mod构造函数里用`static int count`编号拼进返回值（每个mod实例独占服务一个Zone/Building，计数器天然对应"第几个真正落地的实例"）。`AddZone`/`AddBuilding`（私有）按`GetName()`插入，重名返回false并`debugf`警告、不生成消歧名字（只是兜底，触发说明某个mod的计数器有问题）；调用方发现false就`delete`刚构造的对象（`~Zone()`/`~Building()`顺带销毁独占的mod实例，不泄漏）。
- `GetZone(name)`/`GetBuilding(name)`是对map的简单`find`。
- **`LocateZone`/`LocateBuilding`**：按"Block->Zone->Building"解析地址，"Block"等价于顶层`Lot`。`Roadnet::AllocateAddress()`已在`InitRoadnet()`给每个顶层Lot登记了所有临街道路的`(road,index)`地址对（角地块可有多个，`Lot::AddAddress`/`GetAddresses()`），`LocateLot(road,index)`转发`Roadnet::LocateLot`——角地块的多个地址都指向同一个Lot，用哪个都查得到。格式：`"<road> <index> <zoneName>"`、`"<road> <index> <buildingName>"`（直接落在这个Lot上、没有`parentZone`的Building）、`"<road> <index> <zoneName> <buildingName>"`（Zone内部的Building，`Zone::GetInternalBuildings()`按name匹配）。`istringstream`按空格切分，找不到返回`nullptr`。
- **`LocateRoom`**：格式末尾再加房间号（`"... <buildingName> <number>"`或`"... <zoneName> <buildingName> <number>"`）；取最后一个token当房间号，其余拼回交给`LocateBuilding`，再在`building->GetRooms()`里线性找`GetNumber()`匹配的Room。没有`LocateComponent`：Component没有`GetAddress`，不参与地址体系。正向查询（对象->地址）是`Lot::GetAddress()`（取`GetAddresses()[0]`）/`Zone::GetAddress()`/`Building::GetAddress()`/`Room::GetAddress()`，每层是"上一层地址 + 空格 + 自己的Name/门牌号"，格式与上面一一对应。

### 人口对接（Map不持有Citizen/Populace）

`Populace`是与`Map`平级的顶层Core类（不知道Map存在），Map只单向读外部传入的`Populace`，详见`Source/Core/populace/populace.md`：

- `ComputeAccommodationTarget()`：遍历所有building的room，`IsResidence()`的累加`ResidenceCapacity()`并数住宅房间数；累计容量小于住宅房间数两倍时目标人数=住宅房间数（每间至少一人），否则=累计容量的一半。`ComputeWorkCapacity()`：全部`IsWorkspace()`房间的`WorkspaceCapacity()×2`之和（容量是满编，岗位最多可超编到2倍，所以劳动力上限按2倍算），不参与人口随机采样，`Populace::Init(accommodation, workCapacity)`在随机扰动之后用它给人数封顶，避免劳动力远多于岗位造成大面积失业。
- `GetAllComponents()`：拍平每个Building的`GetComponents()`，供`EnsureSocietyGenerated()`喂给`Society::Init`（Society不知道Map/Building）。
- `Checkin(populace)`是**两个独立步骤**，"房产归属"决定每个zone/building/room的`owner`/`stated`，"住处分配"决定谁实际住进哪个room（owner可以从没住过，相当于房东），见`room.md`：
  1. **房产归属**：只在有成年citizen时进行。每个zone先`GetRandom(100)`在"公有（`stateOwnedChance`，写在各自mod里，换算百分数）/整个zone归一个随机成年人私有（`kZoneOwnershipChance`=2）/下探到各building"三选一；building一级同样三选一（公有/私有`kBuildingOwnershipChance`=5/下探到各room各自随机`SetOwner`）。**一旦某级判定"整体统一归属"，owner/stated一路级联写到其下所有building/room**；下探分支里上一级保持默认（`nullptr`/`false`），不需要显式重置（只在统一归属分支才调setter）。不属于任何zone的独立building单独走一遍building级roll，**跳过`GetParentZone()`非空的**（避免覆盖zone级联下来的归属），详见`populace.md`"房产归属"。
  2. **住处分配**：遍历所有building的room，`IsResidence()`的每个room贡献1个名额（不按`ResidenceCapacity()`重复贡献，那个只用于城市级目标）。按"一个家庭消费一个名额"算法：只遍历成年且还没房间的citizen，每人`GetRandom(pool.size())`抽一个名额（swap-remove出池）；配偶（`GetSpouse()`非空、还没房间）以90%概率（`GetRandom(10)>0`）跟着搬进同一间；未成年子女（`GetChildren()`里年龄<18且没房间的）无条件一起搬，**不依赖配偶是否同住**（否则单亲家庭的孩子永远分不到房间）。未成年citizen不独立抽房间。`SettleCitizen(citizen, room)`让citizen住进room：家（`SetRoom`/`SetBuilding`/`SetZone`/`SetLot`+`room->AddResident`，户的归并见`populace/household.h`）和当前位置（`SetCurrentRoom`，内部维护`room->occupants`）都设成这里；`Checkin`和`spawn_npc`的home字段共用。**这一步完全不碰`owner`/`stated`**。
- 调用顺序：`EnsureMapGenerated()`（`InitBuildings()`之后）->`EnsurePopulaceGenerated()`里依次`populace->Init(map->ComputeAccommodationTarget(), ...)`->`map->Checkin(*populace)`，靠`BeginPlay()`里显式的调用顺序保证，见`ForeverFrameworkActor.md`。

### 门系统、电梯、实体脚本、ApplyChange

- **门**：`CreateDoor(kind, zone, building, room, tag, spec, x,y,z,width,height,yaw,level,isVehicleGate)`创建一扇真正放了门扇的门（`DoorSpec.mesh`非空才被调用方调用，见`door.h`的`ResolveDoorSpec`）：分配全局唯一id（`"Door"+doorCount`），按"所属对象名+spec.name"分组计数生成交互名（`<所属对象> <spec.name><n>`，n从1开始；所属对象名：Building/Zone用`GetName()`，Room用`GetAddress()`——Room的`GetName()`只是类型标签，不唯一），`spec.name`非空才有交互名/Script，登记进`doorsById`/`doorsByInteractName`/`allDoors`（都不持有，门的生命周期归创建它的Zone/Building/Room的`doorEntities`）。几何由调用方（`Zone::BuildDoors`/`Building::BuildDoors`）算好传入，这里不做几何计算；`tag`只记在Door上供查询。`InitZones()`在`zone->Layout()`之后、`InitBuildings()`各落地路径在`building->Layout()`之后调`BuildDoors(*this)`。完整设计见`door.md`。
- **电梯**：`CreateCabin(building, specIndex, spec)`（`Building::BuildCabins`调用）创建`Cabin`，交互名`"<building名> 电梯<n>"`（n=这栋楼已建电梯数+1），初始停在1楼（楼层夹到`[minFloor,maxFloor]`），登记进`cabinsByInteractName`（不持有，电梯归`Building::cabinEntities`）。`FindCabinByName`按交互名查。见`cabin.md`。
- **`FindAssetByName(name)`**：遍历全部room的资产（含容器内容，`FindDescendant`），找不到返回`nullptr`；玩家身上/地上的见`Player::FindAssetByName`。
- **实体脚本**：`ZoneMod`/`BuildingMod`/`RoomMod`各有两个非虚字段`scriptModName`（默认`"script_empty"`）和`milestoneNames`（里程碑脚本名数组，默认空），约定同`DoorSpec`/`ElevatorCabinSpec`：**`milestoneNames`非空才创建Script**。Zone/Building在`Layout()`里把这两个字段快照到自己身上（Room在构造函数`SetProperty()`之后快照）。`BuildZoneScript(zone)`（`InitZones`里`BuildDoors`之后）、`BuildBuildingScripts(building)`（各落地路径`BuildCabins`之后，同时给这栋楼每个Room建）创建Script并写入变量`name`、`address`（Room另有`building`）；已有Script的实体跳过，重复调用安全。读档不跑`Layout`，Zone/Building的快照字段进存档（`world.map.structures`），Room的由重建时的`SetProperty()`重新得到，`MapSaveCodec::ReadStructures`末尾调`BuildAllEntityScripts()`补建，脚本状态仍由`script_save`恢复（键`zone:<名>`、`building:<名>`、`room:<地址>`）。Script归实体持有，实体析构时释放。**进出事件只发给对应实体自己的Script**：玩家进出zone/building/room时UE层（`BroadcastPlayerLocation`）只发给被进出实体的Script（没挂脚本就不发给任何人），`system.player.zone/building/room`系统变量照常更新；实体脚本仍收到`game_start`、计时器等普通广播。示例见`Forever_UE/Resource/Story/entity_event.script`。
- **`ApplyChange(change, context)`**：认识的Change一律返回true（不管目标找没找到，找不到静默忽略，与`Traffic::ApplyChange`同一宽松约定），否则返回false。目前处理：`AddOptionChange`/`RemoveOptionChange`（按名字依次尝试Door、Cabin、房间资产，三者都可能命中）、`LaunchElevatorChange`（`command`经`ParseElevatorCommand`解析层号：`B`/`b`后跟数字n->-n，否则取第一个数字n->n-1；`cabin->Launch(level)`，真正的移动由UE层`ABuildingElement::Tick`按行程状态推进）、`SetDoorChange`（`access`布尔表达式：true=常开，false=常锁）、`RegisterDoorChange`（`pass`留空按true）、`RecordObjectChange`（按`object`类别（zone/building/room）和`kind`过滤收集候选地址，排序后`policy=="random"`随机取一个、否则取第一个，写进`variable`；变量名规则同`set_value`；对只读变量打警告）。

### 路径规划（测试导航与公交）

- **`PlanVehicleRoute(startRoom,x,y,destRoom)`**转调`PlanVehicleRouteToNode`，返回`VehicleRoutePlan{status, points}`（地图单位折线，z只是图里的参考高度，真实高度由UE侧贴地决定）。起点在房间里（`startRoom`非空）-> 从`startRoom->GetNavigationNode()`出发（房间没有车行出边=`NO_ROAD_CONNECTION`；起点就是目的节点=OK且points为空）；否则在大路+小路+Zone内部路里按垂足距离取最近几条（`kEntryRoadCandidates`=2条"有车道"的路，每条两个行驶方向各调一次`GetLaneSegmentAt`，所以入线寻路`FindVehiclePath`共2~4次），取"车->垂足->车道下一个锚点->寻路到目的节点"总长最短的。找不到通路时退而求其次，用第一个拿到车道段的候选画"接到路上"的短折线，status=`NO_PATH_TO_DESTINATION`；根本接不上路（附近没有可用车道）=`NO_ROAD_CONNECTION`，points为空。
- **车头夹角**：`PlanVehicleRouteToNode(useHeading=true)`不硬过滤反向车道，而是把"车道方向与车头夹角"线性折算成额外距离（180度=`kHeadingPenaltyMapUnits`=1.5即15m）加进候选总代价，所以8m外的锐角车道优先于2m外的钝角车道（窄路上掉不了头）。
- **`PlanVehicleRouteToNode(startRoom,x,y,destNodeId,...)`**：终点是`vehicleNavGraph`节点id或已登记的辅助节点（公交站）id，不要求是房间；`destNodeId`既不在图里也不是辅助节点->`NO_PATH_TO_DESTINATION`且points为空。**`PlanVehicleLeg(fromNodeId,toNodeId)`**：节点到节点（任一端可为辅助节点），公交循环里站到站用；from==to只给一个点（辅助节点坐标）。
- **`GetLaneSegmentAt(road,x,y,dirX,dirY,out)`**：纯只读查询，不修改任何共享状态。用`ProjectPointOntoRoad`求t，用(dirX,dirY)与road在t处切线的点积判断同向侧，经`ResolveAccessLane`+`throughLines`（与`BreakThroughLine`同一套私有机制）找到覆盖t的那一段，返回段两端锚点与车道上的垂足世界坐标。road为空、没有车行道、或t不在任何已知段内返回false。
- **辅助导航节点`AuxNavNode`**（`RegisterAuxNavNode`/`UnregisterAuxNavNode`/`FindAuxNavNode`）：公交站的`stationNode`不在`vehicleNavGraph`里（`Station`自己拥有这些对象，导航图参与存档读写和析构释放，不能塞进去），在Map里单独登记（运行时、不存档、不拥有）。站作终点=路径到`laneFrom`再走`inEdge`；站作起点=先走`outEdge`再从`laneTo`出发；同一段车道上起点在上游的两个站=两个stationNode直线弦（`AppendLegPoints`）；室外起点恰在站所在车道段、位于站上游=垂足直接开到站（`direct`）。`Station::EnsureRoadLink`登记、`~Station`注销，见`traffic/route.md`"公交接入路网"/"同车道特例"。
- **`PlanPedestrianRoute(startRoom,x,y,destRoom)`**：市民导航测试用。`startRoom`有行人导航节点->从该房间节点出发；否则从(x,y)最近（水平距离）的6个有出边的行人图节点里挑第一个能走到目的节点的。OK：points是各导航节点坐标（含起点和目的房间节点），起点==终点时只有一个点；`NO_PATH_TO_DESTINATION`：找到了起点节点但图不连通，points只含起点节点；`NO_ROAD_CONNECTION`：行人图里没有任何可用起点节点（或目的房间没有导航节点），points为空。

### Registry引用成员

`terrainFactory`/`roadnetFactory`/`zoneFactory`/`buildingFactory`/`roomFactory`/`componentFactory`/`scriptFactory`七个Factory不是Map自己持有，而是引用`Registry::Get()`那份整个进程只注册一次的实例（所有concept集中注册的地方，不只是map域，见`common/registry.md`）。mod dll的发现/注册（`LoadLibrary`+`RegisterConcept`）与"实际生成/销毁地图对象"完全分开：Map每次开局构造/结束析构都不会重新扫描dll，也不会丢失已注册的mod类型。引用成员只能在构造函数初始化列表绑定，所以`map.cpp`必须include `common/registry.h`；**声明顺序必须与初始化列表书写顺序一致**（C++按声明顺序初始化）。`scriptFactory`给门/电梯/实体Script创建用。

## 路径边序列（`NavEdge`）

`VehicleRoutePlan` 除了 `points` 还有 `edges`：每项 `{Connection* edge, firstPoint, lastPoint}` 表示 `points[firstPoint..lastPoint]` 这段折线走的是哪条 Connection（车道边和路口连线都有，相邻两段共用端点）。`PlanVehicleRouteToNode`/`PlanVehicleLeg`/`AppendLegPoints` 车行路线填；室外起点接入车道的第一段是车道边的后半段（垂足→下游锚点），起点到垂足那一小段不属于任何 edge。`PlanPedestrianRoute` 也填（用 `FindPedestrianEdge` 回查）。`FindPedestrianEdge(from,to)` 是只读的行人图查边，线程安全。路口通行管理（`traffic/junction.md`）按 `Connection*` 指针识别路口连线。

## 存档

地图域整体由`map_save.h`的`MapSaveCodec`（`Map`的friend）写进存档/按存档重建，读档时不跑`Init*`里的任何生成，详见`map_save.md`。`orphanLots`只在读档时有内容：自适应占位用过的嵌套子块（生成时已从父块摘掉，新开局无人持有），读档后由Map持有、`~Map()`释放。

## 依赖关系

- 依赖：`terrain.h`/`terrain_factory.h`、`roadnet.h`/`roadnet_factory.h`、`zone.h`/`zone_factory.h`、`building.h`/`building_factory.h`、`door.h`、`vegetation.h`、`lamp.h`、`map/room.h`（`Checkin`/容量统计用`IsResidence()`/`ResidenceCapacity()`/`IsWorkspace()`等）、`populace/populace.h`/`populace/citizen.h`（`Checkin`参数类型，单向依赖，Populace/Citizen不反过来include map域头文件）、`general_geometry.h`（`Quad`/`Node`/`Road`/`Lot`等）、`story/change.h`/`script.h`/`script_factory.h`（`ApplyChange`与实体脚本）、`player/asset.h`（`FindAssetByName`）、`common/config.h`、`common/registry.h`、`common/utility.h`（`debugf`/`GetRandom`）。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor`（持有`Map*`，`EnsureMapGenerated`依次调用`InitTerrains`/`InitRoadnet`/`InitZones`/`InitBuildings`/`InitVegetation`/`InitLamps`；`ComputeAccommodationTarget`+`Checkin`在`EnsurePopulaceGenerated`里）、`ForeverTerrainFrameworkComponent`（读格子数据建mesh）、`ForeverRoadnetFrameworkComponent`（`GetRoads()`/`GetJunctions()`/`GetLots()`/`GetPathRoads()`/`GetPathRoadMaterial()`/导航图）、`ForeverZoneFrameworkComponent`（`GetZones()`）、`ForeverBuildingFrameworkComponent`（`GetBuildings()`）、`ForeverPopulaceFrameworkComponent`（寻路）、`ForeverStoryFrameworkComponent`（`GetAllDoors()`/`FindDoorByName`）、`Traffic`（`FindVehiclePath`/`PlanVehicle*`/辅助节点）。

## 待办/后续阶段

- `InitTerrains()`假定调用方（`AForeverFrameworkActor::BeginPlay`）之前已经跑过`Config::ReadConfig`（由`ForeverModSubsystem`在game instance启动时做，早于任何关卡的`BeginPlay`）——这个顺序依赖只是约定，没有代码层面强制。
- `Map::Tick`目前是空实现，等Map域有每帧逻辑再补。

## 导航线相关：外部连接点与到任意点的车行规划

- `Building::GetOutsideNodes()`：行人导航模板里"outside"端点（1楼外部连接点），`Map::MergeBuildingNavigation`在init时从`BuildingNavResult::outsideNodes`记下，节点仍由`navAnchorNodes`持有。进存档：`map_save`的Building段在`scriptMilestones`之后写`U32 count + 每项锚点id`，读档用`AnchorAt`还原（没加版本号，旧存档读不了）。
- `PlanVehicleRouteToPoint(startRoom,x,y,destX,destY,...)`：终点是室外任意点；取离它最近的车道段（两个方向×`kEntryRoadCandidates`条路），先`PlanVehicleRouteToNode`到该车道边的上游锚点，再补一个垂足点（最后一项`edges`是上游锚点→垂足），取总长最短的。导航线用它给"车到不了的房间"画到建筑外部。
