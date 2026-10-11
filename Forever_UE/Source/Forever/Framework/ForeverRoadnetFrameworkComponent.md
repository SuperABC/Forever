# ForeverRoadnetFrameworkComponent

## 职责

`UForeverRoadnetFrameworkComponent`是路网渲染组件——道路/路口/路网开口的mesh生成，以及
一份常驻的导航图调试可视化。老工程`RoadnetBase.UpdateRoadnet()`是纯`BlueprintImplementableEvent`
空实现，所有实际mesh搭建逻辑都在Blueprint里；这个组件是全新写的C++ mesh生成逻辑，模式上
参照`ForeverTerrainFrameworkComponent`（procedural mesh、材质加载via
`ConstructorHelpers`/`LoadObject`、组件挂在owner的RootComponent下）。

## 关键设计

- **道路正常路段的可见效果直接来自`Road::mesh`本身**——`mesh`资产（默认`default_1_1`）本身
  已经画好了车道+人行道横断面，`vehicleLanes`/`parkingLanes`/`pedestrianLanes`这几组宽度数据
  只用于结构计算（导航锚点偏移、路口路缘角点、开口cube宽度），不驱动任何单独的procedural
  条带或材质。`BuildRoadInstances`的分段数量算法（"缩放后的mesh长度落在[0.8,1.2]×原始长度
  区间内，取最接近total/unit的整数段数"）照抄老工程`UMeshArray::RepeatMesh1D`/
  `RepeatMeshAlongCurve`，但不需要重新实现老工程那套自建弧长采样表——`Connection::GetPoint(f)`
  本身就是弧长参数化的（`Source/Dependence/map/geometry.cpp`的`ResolveArcLengthParam`，
  128点惰性缓存），直接按`f=k/n`取点就已经是等弧长分段。用`UInstancedStaticMeshComponent`
  按分段结果摆放`mesh`资产实例，同时满足"沿路线分隔mesh元素"+"曲线道路"两点。
- **每条唯一的mesh资产路径对应一个共享的`UInstancedStaticMeshComponent`**（`GetOrCreateRoadISM`
  按路径查/建），不是每条Road各自一个ISM——目前所有Road都用同一个`default_1_1`，只会建
  一个ISM，但接口上支持未来mod给不同Road配不同mesh资产。
- **开口：整条路横断面宽度都被替换成一块贴`DefaultRoad`材质的扁平cube**，不精细地只挖开
  人行道/停车道那一部分——`BuildRoadInstances`在命中`Road::GetOpenings()`对应弧长区间时
  跳过该分段mesh实例的摆放，`BuildOpeningMeshes`在同一段范围画一块覆盖整条路总宽度（车行
  +停车+人行道两侧加总）的cube。两处cube生成共用同一份几何逻辑（`AppendFlatRoadCube`：给定
  road上的中心弧长比例+沿路长度+横向宽度，画一块扁平quad）。`BuildRoadInstances`按`unit`
  铺不出至少一节`default_x_x_x`实例的短缺口（比如两个相邻开口之间只剩很短一截），同样退化
  成`AppendFlatRoadCube`扁平cube，不留视觉空隙。
- **路口mesh是直线简化版**（`BuildJunctionMeshes`）：每个`RoadJunction`的`approaches`已经按
  夹角排好序，把`curbRight[i]`/`curbLeft[i]`两两相邻连成边界点序列（`(right_i,left_i)`是
  road i自己的"开口宽度"边，`(left_i,right_{i+1})`是road i与road i+1之间的桥接边），从路口
  中心（`Intersection`自身坐标）扇形三角剖分，贴`DefaultRoad`材质。不做圆角/斜切。
- **开口cube和路口mesh都带碰撞**——`CreateMeshSection`的`bCreateCollision`传`true`。道路
  本身的ISM实例走`UInstancedStaticMeshComponent`默认碰撞（跟着`default_1_1`资产自带的
  collision setup走）。
- **`Road::AddOpening`接小路开口**：Zone/Building裁剪Lot自由空间时接到"大路"的小路，调用
  `Road::AddOpening`产出开口，渲染这里按`openings`列表（经弧长`t`去重，同一个物理位置只
  画一次cube，`t`在1e-4误差范围内视为同一个）画缺口，见`Source/Dependence/map/
  geometry.md`"关键设计"一节`Lot::SplitWithPath`。小路开口这条路径**不**调用
  `Map::AddRoadAccessNode`标记导航图，只直接`endRoad->AddOpening(...)`标记路面缺口——纯
  几何/渲染标记和"小路接导航图"是两回事，后者由`Map::ConnectPathRoad`单独实现，见
  `Source/Core/map/map.md`"ConnectPathRoad"一节。`Map::AddRoadAccessNode`本身（同时
  断开导航图车道贯通线+标记`RoadOpening`）作为API继续保留，供其它调用方使用。
- **导航图调试可视化**：`BuildNavigationDebugMesh()`（公开方法）+`BuildNavGraphDebugMesh()`
  （私有实现，给车行/行人各调一次），由`bShowNavigationDebug`（`EditAnywhere`）开关控制，
  关闭时显式`ClearMeshSection(0)`（不是跳过不生成，避免旧可视化mesh残留在场景里）。公开成
  独立方法是因为运行时往导航图里新增锚点/边之后需要能重新调用它刷新可视化。
  - `AppendQuadDoubleSided`（匿名namespace自由函数）：四个角点无论以什么环绕顺序传入，两个
    方向的三角形都画一遍——debug mesh的box/ribbon朝向五花八门，不值得为每个面单独推导
    "哪个环绕顺序才是正面朝上"，双面画一遍最省心，只是调试用不追求正确光照。
  - `AppendNavBox`：给每个锚点画一个轴对齐的小长方体（6个面都调`AppendQuadDoubleSided`），
    水平半边长`NAV_DEBUG_NODE_HALF_SIZE=40`，竖直范围`[锚点真实Z+ROADNET_HEIGHT_EPSILON,
    该值+NAV_DEBUG_HEIGHT]`——用锚点自己的真实Z（不是固定0），隧道场景下的锚点会正确显示
    在地下。`AppendNavEdgeRibbon`：两端锚点的box顶面高度之间连一条细双面ribbon
    （`NAV_DEBUG_EDGE_HALF_WIDTH=8`）。元素必须有真正的竖直厚度
    （`NAV_DEBUG_HEIGHT=50`，约5cm）才能在PIE里稳定可见，不能是单一Z高度的纯平面。
  - **同一个锚点被多条边引用时只画一次box**（`unordered_set<int> visitedNodes`按Node id
    去重）——导航图数据结构是`unordered_map<id, vector<pair<id, Connection*>>>`，同一个
    锚点作为多条边的起点/终点很常见。不需要单独维护一份"锚点id→坐标"的查找表——每条边的
    `Connection*`本身就带着两端真实`Node`（`GetStart()`/`GetEnd()`）。
- **和Terrain一样固定`worldScale=1000.f`（1地图单位=10m=1000cm）**，`Node`/`Connection`/
  `Road`等Core层几何类型的坐标都是地图单位，Forever层建mesh时统一在最后一步乘以这个系数转
  世界坐标，中间计算全部保持地图单位，避免在算式里混用两种单位。
- **`BuildPathRoadMeshes`**：遍历`map->GetPathRoads()`（Zone/Building裁剪Lot自由空间时
  自动生成的小路），每条按起止点+`Road::GetTotalWidth()`生成一个贴材质的扁平ribbon，不接入
  `BuildRoadInstances`那套按`default_x_x_x`资产选mesh的ISM管线——小路车道宽度是0.3/0.2这种
  非整车道宽度，套不进那套命名约定，见`Source/Basic/map/roadnet_basic.md`。材质优先按
  `map->GetPathRoadMaterial()`的字符串路径**运行时**`StaticLoadObject`加载（不是
  `roadPlainBaseMaterial`那种只能在构造函数里用的`ConstructorHelpers::FObjectFinder`，
  因为这个路径是`RoadnetMod`运行时数据，编译期不知道具体是哪个资产），取不到就退化用
  `roadPlainBaseMaterial`。必须在`GenerateRoadnet`里紧跟`BuildJunctionMeshes`之后调用，
  且`AForeverFrameworkActor::EnsureMapGenerated()`必须保证`Map::InitZones()`/
  `InitBuildings()`已经跑完再调`GenerateRoadnet`，否则`GetPathRoads()`还是空的。

### 路口/开口几何的几条不显而易见的约束

这几条约束是路口mesh和开口cube正确衔接的前提，改动相关代码时要留意：

- **三角形环绕顺序**：UE（左手坐标系）要求从上方看是"顺时针"（标准数学XY凸包意义下的负向
  面积）才是正面朝上——`BuildOpeningMeshes`/`BuildJunctionMeshes`的顶点顺序要和
  `ForeverTerrainFrameworkComponent::BuildLevel`已验证过的`(v00,v11,v10)/(v00,v01,v11)`
  模式一致。道路本身用真实静态网格资产（`default_1_1`），环绕顺序是资产自带的，不受这条
  约束影响。
- **路口两端的道路tiling必须收缩（`trimStart`/`trimEnd`）**，否则两条路的`default_1_1`会
  在路口处重叠z-fighting；收缩量必须和`RoadJunctionApproach::setback`（该端两侧车道总宽度
  里较宽的一侧，也是路缘角点/导航锚点的基准点外移距离）用同一个值，否则路口多边形撑不满
  收缩出来的空当，看起来"路口处什么都没有"。
- **路口mesh/开口cube的Z要加`ROADNET_HEIGHT_EPSILON`**（当前1cm，世界单位10）避免和Z≈0的
  "plain"地形共面z-fighting——和`ForeverTerrainFrameworkComponent.cpp`的`HEIGHT_EPSILON`
  是同一类问题。隧道场景下中心点和各条路的curb点高度可能不同（一侧curb点还在地表附近、
  中心点已经在隧道内下沉），`BuildJunctionMeshes`扇形三角化允许每个顶点独立取高度：中心点
  Z用`junction->GetNode()->GetZ() * ROAD_WORLD_SCALE + ROADNET_HEIGHT_EPSILON`，每个
  curb点Z用`ap.curbZ * ROAD_WORLD_SCALE + ROADNET_HEIGHT_EPSILON`（`curbZ`在Core层
  `RoadJunction::Build()`按`sampleT`采样`road->GetPoint(sampleT)`得到真实X/Y/Z，不是
  直线近似，否则隧道口S形坡道的高度变化会被漏掉，导致台阶断层，详见`Source/Core/map/
  roadnet.md`"路口高度"一节）。
- **道路tiling的开口缺口按每个开口的精确`[t-halfFrac,t+halfFrac]`区间切分保留区间**（区间
  减法，支持同一条路多个开口），不是"整体铺一遍再按段中点跳过"——否则缺口宽度只能凑整到
  unit的倍数，和开口cube的精确宽度对不上。
- **`BuildOpeningMeshes`的cube以`road->GetPoint(op.t)`为中心、按`Road::GetTotalWidth()`
  对称展开**——这个写法依赖`Connection`连线是整条车道横断面的几何中心（车道居中约定，见
  `roadnet.md`"车道居中"一节），单行道等不对称配置如果连线不在总宽度中点会让cube偏出实际
  路面范围。

## 依赖关系

- 依赖：`Source/Core/map/map.h`（`Map::GetRoads()`/`GetJunctions()`/`GetLots()`/
  `GetPathRoads()`/`GetPathRoadMaterial()`/`AddRoadAccessNode`）、`Source/Core/map/geometry.h`
  （`Road`/`RoadJunction`/`Lot`/`Node`）、`ProceduralMeshComponent`模块、
  `Components/InstancedStaticMeshComponent.h`（Engine模块自带，不需要额外启用）。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（`EnsureMapGenerated()`
  在`terrainFramework->GenerateTerrain(map)`之后、`InitZones`/`InitBuildings`跑完之后调用
  `roadnetFramework->GenerateRoadnet(map)`）。

## 待办/后续阶段

- 路口mesh的圆角/斜切、开口cube的精细化（只挖开人行道/停车道而不是整条路宽度）——明确的
  简化范围，不是遗漏。
