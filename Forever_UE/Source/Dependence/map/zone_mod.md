# zone_mod.h / building_mod.h

两个concept的Mod接口在阶段4-1 Zone/Building落地时一起设计（都要向`Lot`要地，接口形状几乎
一样），放在同一份文档里对照说明，避免两份文档大段重复。和老工程`Map::InitContents`比，
这次是**新设计**：老工程权重表是`GetPowers()`静态编译期常量，这次改成mod被引擎调用时动态
往每个`Lot`上push自己的类型+权重（要求8）；老工程只有"权重+随机面积"一种分配方式，这次新增
"直接指定一块贴着某条路的矩形区域"这种显式占位方式（要求1）。详见用户在Plan Mode里确认的
关键设计决策，`Source/Core/map/map.md`"InitZones/InitBuildings"一节有完整的引擎侧编排流程。

## 职责

- **`LotPlacementRequest`**（定义在`geometry.h`，两个mod头文件共用）：`{Lot* lot, int
  direction, float marginStart, float marginEnd, float depth}`，描述"贴着lot某条边界路、
  沿路方向留出marginStart/marginEnd、往里伸depth"的一块矩形。
- **`PlacementEmitFunc`**（定义在`geometry.h`，和`LotPlacementRequest`放一起）：
  `void(*)(void* context, const LotPlacementRequest& request)`，裸函数指针，和
  `creator`/`deleter`同一个"调用哪个模块编译的代码就用哪个模块的内存管理"机制。
- **`ZoneMod::Assign(lots, emit, context)`/`BuildingMod::Assign(lots, emit, context)`**
  （static方法，不需要任何实例）：引擎一次性把当前全图lot列表（剩余空闲面积降序，
  `Lot::GetFreeAcreage()`排序）传进去，mod在其中自己遍历、自己决定每块lot要不要显式占位、
  往哪摆，想要哪块lot就调一次`emit(context, request)`（`request.lot`必须设成对应的那个
  lot）。`BuildingMod`还有额外的static查询：`RandomAcreage()`/`GetAcreageMin()`/
  `GetAcreageMax()`（供`Lot::FillRemainder`采样目标面积/判断是否够放）、
  `GetPower(AREA_TYPE)`（这个类型在某个分区类型的lot上的权重，替代老工程
  `GetPowers()`/`BuildingAssigner`，供引擎登记`lot->AddCandidate(...)`）。

## 关键设计

- **`Assign`/`RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`/`GetPower`全部是不需要任何
  实例的static方法，不是虚方法**——`ZoneMod`/`BuildingMod`从构造到析构只服务**一个**真正
  落地的Zone/Building（仿照老工程"一个本体独占一个mod实例"模型，详见
  `Source/Core/map/zone.md`/`building.md`），这些查询在真正落地之前就要用到（"要不要这块
  lot"/"这个类型多大"/"这个类型在这个分区权重多少"），如果做成虚方法就必须先构造一个实例
  才能问，问完这个实例很可能根本用不上（这块lot不要/这个候选没被选中），造成"构造了一个
  马上要销毁的假实例"这种浪费。改成static之后，通过`ZoneFactory::RegisterZone`/
  `BuildingFactory::RegisterBuilding`的额外函数指针参数注册（和`creator`/`deleter`同样的
  裸函数指针机制——这几个方法本来就是无捕获的static普通函数，天然能隐式转换成函数指针；
  `AssignFunc`内部通过`PlacementEmitFunc`回调把结果交回调用方，回调函数体本身编译在调用方
  那一侧，不返回/不持有任何容器，避免跨DLL分配器问题；`RandomAcreage`等返回值是纯`float`，
  同样没有容器所有权问题），完全不需要构造实例就能查，只有真正落地成功才会
  `CreateZone`/`CreateBuilding`一次。
- **Zone这次只有显式占位一种方式，没有权重/CDF方法**——`ZoneMod`不像`BuildingMod`那样有
  `RandomAcreage`/`GetAcreageMin`/`GetAcreageMax`/`GetPower`，也不会被喂进
  `Lot::FillRemainder`。这是Plan Mode里用户明确的修正："zone生成阶段不要填满lot"——Zone
  不应该像老工程`ArrangeBlocks`那样把地块"填满"，只应该占据mod自己主动要的那一块。
- **`BuildingMod`两种方式都有**，照抄老工程`Map::InitContents`对Building的处理顺序（Zone先
  占完地，Building再用权重CDF填剩下的空间）。
- **`Layout(direction, quad, boundaryRoads)`（非static虚方法，`ZoneMod`/`BuildingMod`都
  有）**：在`CreateZone`/`CreateBuilding`产出真实例、`SetPosition`/`SetBoundaryRoad`都设好
  之后调用一次，用来配置"摆下去之后长什么样"（`ZoneMod`的`walls`/`gates`/内部道路/内部建筑，
  `BuildingMod`的楼体footprint/楼层/LOD材质，见下）——和`Assign`只决定"要不要、往哪摆"分开，
  且只在真正会被保留的实例上跑一次。`direction`对显式占位落地是`Assign`选中的真实方向，
  `quad`/`boundaryRoads`是真正落地的矩形和四周道路，都是纯只读输入，跨DLL传递安全（`Quad`
  没有动态容器，`unordered_map<int,Road*>`只是读指针值）。
- **`Distribute(lots)`/`Assign(lots,...)`这次暂时不按`lot->GetArea()`筛选/区分权重，
  `ResidenceZone`/`ResidenceBuilding`对所有lot一视同仁**——地块类型本身`RoadnetMod`已经在构造lot时
  用`Lot::SetArea(AREA_TYPE)`标好了（`AREA_TYPE`定义在`geometry.h`），但这两个mod目前都
  只是验证链路用的通用占位类型（一个验证显式占位，一个验证权重CDF），用户明确要求先不区分，
  等以后设计具体建筑/园区类型（对照老工程`ResidentialZone::ZoneAssigner`/各
  `XxxBuilding::GetPowers()`按地块类型区分权重那套）时再按`lot->GetArea()`细化。
- **任何mod需要"通知引擎某个Core层对象该怎么样"的场景，都要走"emit回调/static查询、引擎
  自己处理"这个模式，不能让mod直接调用Core层对象的非虚成员函数写它自己的容器**——`Lot::
  AddCandidate`是非虚成员函数，`Basic.dll`/`Empty.dll`等mod dll和`Forever.dll`各自独立
  编译了一份`Dependence.lib`，如果mod直接调用它，`Lot::candidates`这个`std::vector`的
  内部缓冲会被mod dll的（普通CRT）分配器分配；但`Lot`对象本身是`Forever.dll`分配、也由
  `Forever.dll`（经`Roadnet::~Roadnet()`）析构的，UE给每个模块都覆写了`operator new`/
  `delete`（`PerModuleInline.inl`，走`FMemory`），两边分配器对不上，退出时析构`Lot`会直接
  崩溃（`EXCEPTION_ACCESS_VIOLATION`，PIE验证发现过、已修复）。修复方式：`Map::
  InitBuildings()`（Forever.dll编译的代码）调`BuildingFactory::GetPower(id, area)`查到
  权重之后，自己调用`lot->AddCandidate(...)`——分配器和后续析构完全一致。

- **Zone内部布局（围墙/大门/出入口/内部道路/内部建筑）也走"mod自己的输出表+引擎读回去处理"
  这同一个模式，第十五轮迁移新增**：`ZoneMod`新增`walls`（`vector<ZoneWallSpec>`）、`gates`
  （`vector<ZoneGateSpec>`）、`vehicleEntries`/`vehicleExits`/`pedestrianAccess`
  （`vector<ZoneAccessPoint>`）、`internalRoads`（`vector<ZoneInternalRoadSpec>`）、
  `internalBuildings`（`vector<ZoneInternalBuildingSpec>`）七个`public`成员，和
  `explicitPlacements`平级。**这些字段天然是"按每次落地单独配置"的**——`Map::InitZones()`
  对每个真正落地成功的Zone都单独`CreateZone(id)`一个新mod实例、单独调一次
  `mod->Layout(direction, quad, boundaryRoads)`（见`Source/Core/map/map.md`"InitZones/
  InitBuildings"一节），`mod`本身就是这一个Zone独占的，`Layout()`里完全可以按传入的
  `direction`/`quad`/`boundaryRoads`（这次真正落地的具体情况）决定不同的墙/门/内部布局，
  不再是"整个mod类型统一一份配置"。全是值类型（`string`/`vector`/`unordered_map<int,int>`/
  裸`Lot*`非持有指针），符合跨DLL安全铁律。`walls`/`gates`不需要Core搬运——`Zone`直接持有
  这个mod实例，
  `Zone::GetWalls()`/`GetGates()`就是转发`mod->walls`/`mod->gates`；`vehicleEntries`/
  `vehicleExits`/`pedestrianAccess`/`internalRoads`由`Map::InitZones()`读一遍接图/实例化成
  真正的`Road`；`internalBuildings`推迟到`Map::InitBuildings()`才读（`buildingFactory`此时
  才注册好mod，见`map.md`"InitZones/InitBuildings"一节），具体每个字段的语义/Core怎么消费见
  `Source/Core/map/zone.md`和`Source/Core/map/map.md`"Zone内部布局"一节。
  - `ZoneWallSpec{direction,marginStart,marginEnd,depth,depthInward,mesh,unit}`：沿
    `direction`这条参考边铺设，`marginStart`/`marginEnd`是距参考边两端的距离，`depth`是进深，
    `depthInward`控制进深往矩形内(`true`)还是外(`false`)量——具体哪个是"正确"朝向由围墙
    资产的实际朝向决定，先默认`true`，效果不对由PIE验证后再翻转。`mesh`/`unit`和
    `Road::GetMesh()`/`GetUnit()`同一套语义，沿边重复铺设，算法照抄
    `ForeverRoadnetFrameworkComponent::BuildRoadInstances`的`tileRange`（"scaled unit落在
    `[0.8,1.2]*unit`区间内"这条约束），渲染完全在Forever层(`ForeverZoneFrameworkComponent`)，
    Core只透传数据。
  - `ZoneGateSpec`：字段和`ZoneWallSpec`的位置部分同构，但没有`mesh`/`unit`——这次没有大门
    资产，只存位置数据（供围墙据此让出这段空当），不生成任何渲染。
  - `ZoneAccessPoint{x,y,width}`：局部坐标，**原点在Zone矩形中心**——和`Lot`局部坐标系原点在
    WEST-NORTH角不同，这是专属于这几个zone内部相关结构体的新约定（`ZoneInternalRoadSpec`/
    `ZoneInternalBuildingSpec`同样用这套约定）。`Map::InitZones()`对每个出入口调用新增的私有
    方法`Map::ConnectZoneAccessPoint`——在`zone->GetBoundaryRoads()`里找最近的一条真实道路，
    按`ResolvePathEndAnchors`同一套"点积判断近侧"方法选车道，`BreakThroughLine`断出node，和
    zone侧的孤立锚点建一条`Connection`（车行按`isEntry`单向，行人双向）。车行的"入口"和"出口"
    是两个独立数组（每条边分开指定，不像小路那样绑定成一对），行人只有一个数组（人行边本来
    就双向，不区分入/出）。
  - `ZoneInternalRoadSpec{x1,y1,x2,y2,isVehicle,width}`：两端点+单一车道——**只能是"一条车辆
    单行道"或"一条人行道"二选一**，不支持多车道/双向车行道打包进同一个spec（用户明确要求和
    `vehicleEntries`/`vehicleExits`/`pedestrianAccess`分开指定的模型保持一致：车行入口/出口/
    行人各自独立，内部道路也不该是"一个对象打包多条车道"的真实`Road`那种形状）。要双向车行，
    用两条spec各自反向；要行人+车行都通，用两条spec各给一个类型，端点坐标相同的话会在
    `anchorCache`里自动按坐标+类别合并。`Map::ConnectZoneInternalRoad`实例化成`mesh=""`/
    `unit=0.f`的真正`Road`（和`Lot::SplitWithPath`产的小路同样"不参与`BuildRoadInstances`
    铺设、只连导航图"的约定），固定用side0——`isVehicle=true`时沿Start->End单向，`false`时
    双向（人行边本来就双向）——比照`Map::ConnectPathRoad`给小路建图的轻量方式，不经过
    `RoadJunction`。端点位置用`ComputeLaneAnchorPosition`算（单一车道时这个公式的偏移量正好
    抵消成0，等价于中轴线，但和大路/小路锚点算法保持同一套写法），和`anchorCache`
    (`Map::InitZones()`内的局部变量，出入口和内部道路共用同一份)里已有的世界坐标(含车行/行人
    类别)在容差内重合就直接复用同一个node。
  - `ZoneInternalBuildingSpec{type,x,y,sizeX,sizeY,relativeRotation,direction,roadIndices}`：
    `type`供Core构造`Building(factory,mod)`用，`relativeRotation`是相对zone自身`rotation`
    的附加旋转，`direction`（新增字段，默认`FACE_WEST`，和`ZoneWallSpec`/`ZoneGateSpec`
    同款默认值）传给这个内部建筑自己的`BuildingMod::Layout()`——园区内部建筑没有`Assign`
    挑选的方向，由mod在这里直接声明朝向；`roadIndices`是`FACE_DIRECTION->internalRoads下标`
    的映射(mod执行阶段道路还没实例化，先存下标，Core实例化完`internalRoads`后再解析成
    `Road*`)。`Map::PlaceZoneInternalBuilding`用
    `building->SetParentZone(zone)`+`building->SetParentLot(zone->GetParentLot(),
    spec.relativeRotation)`同时设置`parentZone`(纯反向查询登记)和`parentLot`(旋转转发基准，
    直接复用zone自己的`parentLot`)——`Building`两个字段同时持有，不是二选一，详见
    `Source/Core/map/building.md`。

- **`BuildingMod`的楼体footprint/楼层/LOD字段（这次会话新增，在`Layout(...)`里设置，见上）**：
  `BuildingFootprintSpec footprint`——两个比例float表示楼体中心在Building自身`Quad`里的
  位置（0.5,0.5=居中，同`Quad::posX/posY`"矩形中心点"的语义），两个比例float表示楼体长宽
  相对`Quad`长宽的比例；不复用`Quad`类型本身（那样会把`Quad`正常的"中心点+尺寸"语义挪用成
  "比例袋子"，容易搞混，这是仿照老工程但刻意改掉的一处设计）。`int basements`/`int
  layers`——地下室/地上楼层数。`vector<float> floorHeights`——每层楼的高度（地图单位），
  长度必须是`basements+layers`，从最底下的地下室到最高层由下到上排列；不像老工程那样全楼
  统一一个`height`，每层高度必须显式指定（用户明确要求）；`Building::Layout()`读取校验，
  长度不对会用默认值（0.4f，老工程Hotel同款兜底值）补齐。`std::string lodMaterial`——远处
  LOD单个cube用的材质软路径，留空时渲染层用默认灰色材质（Forever层拿`Pure`材质+
  `SetVectorParameterValue("Color", ...)`染灰，不强制要求mod提供专门的灰色资产）。渲染细节见
  `Source/Forever/Framework/ForeverBuildingFrameworkComponent.md`。

## 依赖关系

- 依赖：`map/geometry.h`（`Lot`、`LotPlacementRequest`、`PlacementEmitFunc`、
  `FACE_DIRECTION`、`AREA_TYPE`）。
- 被谁依赖：`Source/Core/map/zone.h`/`building.h`（`Zone`/`Building`包装类持有一个mod实例，
  `Zone`新增字段存放`ZoneWallSpec`/`ZoneGateSpec`/内部道路/内部建筑）、
  `Source/Core/map/map.h`（`Map::InitZones`/`InitBuildings`及新增的
  `ZoneLocalToWorld`/`ConnectZoneAccessPoint`/`ConnectZoneInternalRoad`/
  `PlaceZoneInternalBuilding`）、`Source/Basic/map/zone_residence.h`/`building_residence.h`
  （`ResidenceZone`/`ResidenceBuilding`默认内容，改名自`ZoneBasic`/`BuildingBasic`，见
  `Source/Core/populace/populace.md`；`ResidenceZone`额外用这些新字段搭了一个围墙+大门+
  内部道路的测试场景）、`Forever_Mod/Empty`的`EmptyZone`/`EmptyBuilding`demo mod、
  `Source/Forever/Framework/ForeverZoneFrameworkComponent`（读`Zone::GetWalls()`渲染围墙）。

## 待办/后续阶段

- Zone内部再对自己的剩余空间跑一次Building分配流程（关键设计决策1明确推迟，仍未做——这次
  新增的`internalBuildings`是`ZoneMod`显式指定的固定建筑列表，不是自动填充剩余空间）。
- 围墙"贴合不满整数unit时"的退化兜底渲染（这次先跳过，见`ForeverZoneFrameworkComponent.md`）。
- 大门资产/渲染（这次`ZoneGateSpec`只存位置数据，没有mesh字段，等有资产了再补）。
- Building自己的出入口逻辑（这次`ConnectZoneAccessPoint`只服务Zone，Building内部逻辑
  留到以后）。
