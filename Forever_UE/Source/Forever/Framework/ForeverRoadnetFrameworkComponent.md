# ForeverRoadnetFrameworkComponent

## 职责

`UForeverRoadnetFrameworkComponent`是路网渲染组件——道路（ISM沿路重复摆放）、路口、开口、小路的
mesh生成，以及可开关的导航图调试可视化。模式上参照`ForeverTerrainFrameworkComponent`
（procedural mesh、材质固定加载走`ConstructorHelpers`、组件挂在owner的RootComponent下）。
`GenerateRoadnet`依次建：道路ISM+开口(无人行道资产+补路面矩形/整宽cube)、路口mesh、小路mesh、导航debug mesh。

## 关键设计

- **道路正常路段的可见效果直接来自`Road::mesh`本身**——`mesh`资产（如`default_1_0_1_1_0_1`，命名规则见`roadnet_jing.md`）本身
  已经画好了车道+人行道横断面，`vehicleLanes`/`parkingLanes`/`pedestrianLanes`这几组宽度数据
  只用于结构计算（导航锚点偏移、路口路缘角点、开口cube宽度），不驱动任何单独的procedural
  条带或材质。`BuildRoadInstances`的分段数量算法是"缩放后的mesh长度落在[0.75,1.5]×原始长度
  区间内，取最接近total/unit的整数段数"；不需要自建弧长采样表——`Connection::GetPoint(f)`
  本身就是弧长参数化的（`Source/Dependence/map/general_geometry.cpp`的`ResolveArcLengthParam`，
  128点惰性缓存），直接按`f=k/n`取点就已经是等弧长分段。用`UInstancedStaticMeshComponent`
  按分段结果摆放`mesh`资产实例，同时满足"沿路线分隔mesh元素"+"曲线道路"两点。
- **每条唯一的mesh资产路径对应一个共享的`UInstancedStaticMeshComponent`**（`GetOrCreateRoadISM`
  按路径查/建），不是每条Road各自一个ISM——车道配置相同的Road共用同一个资产，
  用到几种资产(不同车道配置、隧道`_T`、无人行道变体)就建几个ISM。
- **曲线上的节间补缝**：每节是刚性的直段，曲线上相邻两节朝向差`delta`，外侧会张开楔形缝（内侧重叠）。`BuildRoadInstances`把内部接缝处每节的两端各沿自身轴向多伸`e=(W/2)·tan(delta/2)`（`W`=路总宽，`delta`取该节两端切线的水平夹角，`e`不超过节长一半），外侧的角正好碰上；这一段首尾（靠路口/开口）的那一端不伸。直路`delta=0`不变。内侧会有重叠，路面材质按世界坐标取样所以不闪。**头尾倾斜(2026-11-20)**：每节实例的头抬高、尾压低`ROAD_TILE_TILT`(1e-5地图单位=0.01cm，中心高度不变)，整节微微倾斜，内侧重叠处后一节的头压在前一节的尾上，避免共面z-fighting(开口旁的免人行道单段同样处理)；是权宜办法，要根治得用样条网格(`USplineMeshComponent`，弯路节用、直路仍ISM)或材质WPO形变。ISM是刚性实例，做不到像样条网格那样真正沿曲线弯曲，要彻底消除得改成CPU变形的程序网格。
- **开口按侧处理**（`BuildRoadInstances`一并完成）：同一条路上重叠的`RoadOpening`先合并成一段
  （同一物理开口的车行/行人两条同t同宽，合并后只处理一次），每段记录落在哪一侧（`RoadOpening.forwardSide`，true=side0）。
  - **开口只在一侧、这侧有人行道、且`Road::GetSidewalkFreeMesh(side)`配了无人行道资产（能加载）**：开口段不画整宽cube，
    改成**一个**拉伸到开口长度的无人行道资产实例（开口一般不大，不重复铺），另一侧人行道原样保留，缺的人行道区域
    （宽=该侧人行道总宽，贴路面最外缘）补一块`Road`材质的扁平矩形（`AppendFlatRoadCube`的`lateralOffset`参数）。
    **资产原点在它自己横断面的正中心**，去掉side侧人行道（宽`pedWidth`）后中心比路中心往另一侧挪`pedWidth/2`，
    所以实例要反向补回：side0被去掉实例沿side0方向（右手垂线`(-dy,dx)`）偏`-pedWidth/2`，side1被去掉偏`+pedWidth/2`；
    补的矩形中心在`±(总宽-pedWidth)/2`（沿side0方向为正，side0取正）。横断面布局见`Map::ComputeLaneAnchorPosition`。
  - **其余**（两侧都开口、该侧本来没人行道、资产没配/加载失败）：整条路宽换成一块`Road`材质的扁平cube（旧行为）。
  无人行道资产命名同普通资产（`default_左车行_左停车_左人行_右车行_右停车_右人行`，去掉的那一侧人行数为0），
  由RoadnetMod在建路时用`Road::SetSidewalkFreeMesh`配好，不用旋转；加载失败只警告一次（`failedRoadMeshes`）。
  `BuildRoadInstances`按`unit`铺不出至少一节实例的短缺口，同样退化成`AppendFlatRoadCube`扁平cube，不留视觉空隙。
- **路口mesh是直线简化版**（`BuildJunctionMeshes`）：每个`RoadJunction`的`approaches`已经按
  夹角排好序，把`curbRight[i]`/`curbLeft[i]`两两相邻连成边界点序列（`(right_i,left_i)`是
  road i自己的"开口宽度"边，`(left_i,right_{i+1})`是road i与road i+1之间的桥接边），从路口
  中心（`Intersection`自身坐标）扇形三角剖分，贴`Road`材质。不做圆角/斜切。
- **开口cube和路口mesh都带碰撞**——`CreateMeshSection`的`bCreateCollision`传`true`。道路
  本身的ISM实例走`UInstancedStaticMeshComponent`默认碰撞（跟着路面mesh资产自带的
  collision setup走）。
- **`Road::AddOpening`接小路开口**：Zone/Building裁剪Lot自由空间时接到"大路"的小路，调用
  `Road::AddOpening`产出开口，渲染这里按`openings`列表（经弧长`t`去重，同一个物理位置只
  画一次cube，`t`在1e-4误差范围内视为同一个）画缺口，见`Source/Dependence/map/
  general_geometry.md`"关键设计"一节`Lot::SplitWithPath`。小路开口这条路径**不**调用
  `Map::AddRoadAccessNode`标记导航图，只直接`endRoad->AddOpening(...)`标记路面缺口——纯
  几何/渲染标记和"小路接导航图"是两回事，后者由`Map::ConnectPathRoad`单独实现，见
  `Source/Core/map/map.md`"ConnectPathRoad"一节。`Map::AddRoadAccessNode`本身（同时
  断开导航图车道贯通线+标记`RoadOpening`）作为API继续保留，供其它调用方使用。
- **导航图调试可视化**：`BuildNavigationDebugMesh()`（公开方法）+`BuildNavGraphDebugMesh()`
  （私有实现，给车行/行人各调一次），由`bShowNavigationDebug`（`EditAnywhere`，目前默认false）开关控制，
  关闭时显式`ClearMeshSection(0)`（不是跳过不生成，避免旧可视化mesh残留在场景里）。公开成
  独立方法是因为运行时往导航图里新增锚点/边之后需要能重新调用它刷新可视化。
  - `RoadnetAppendQuadDoubleSided`（匿名namespace自由函数）：四个角点无论以什么环绕顺序传入，两个
    方向的三角形都画一遍——debug mesh的box/ribbon朝向五花八门，不值得为每个面单独推导
    "哪个环绕顺序才是正面朝上"，双面画一遍最省心，只是调试用不追求正确光照。
  - `AppendNavBox`：给每个锚点画一个轴对齐的小长方体（6个面都调`RoadnetAppendQuadDoubleSided`），
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
- **`BuildPiers`**（`GenerateRoadnet`末尾调用）：遍历`map->GetRoadPiers()`（公路桥桥墩，选位在Core，见`Core/map/pier.md`），每根一个ISM实例，复用`GetOrCreateRoadISM`；mesh空串用`/Game/Asset/Common/Mesh/Cylinder`。适配方式与`ForeverTrafficFrameworkComponent::BuildPiers`（火车桥墩）完全一致：墩底`Map::SampleHeight`贴地形，墩顶贴`topZ`，按mesh包围盒把水平直径缩放到`2*radius`、竖直撑满墩高。
- **`BuildPathRoadMeshes`**：遍历`map->GetPathRoads()`（Zone/Building裁剪Lot自由空间时
  自动生成的小路），每条按起止点+`Road::GetTotalWidth()`生成一个贴材质的扁平ribbon，不接入
  `BuildRoadInstances`那套按`default_x_x_x`资产选mesh的ISM管线——小路车道宽度是0.3/0.2这种
  非整车道宽度，套不进那套命名约定，见`Source/Basic/map/roadnet_jing.md`。材质优先按
  `map->GetPathRoadMaterial()`的字符串路径**运行时**`StaticLoadObject`加载（不是
  `roadPlainBaseMaterial`那种只能在构造函数里用的`ConstructorHelpers::FObjectFinder`，
  因为这个路径是`RoadnetMod`运行时数据，编译期不知道具体是哪个资产），取不到就退化用
  `roadPlainBaseMaterial`。必须在`GenerateRoadnet`里紧跟`BuildJunctionMeshes`之后调用，
  且`AForeverFrameworkActor::BuildGenerationStages()`登记的阶段顺序必须保证`Map::InitZones()`/
  `InitBuildings()`已经跑完再调`GenerateRoadnet`，否则`GetPathRoads()`还是空的。

### 路口/开口几何的几条不显而易见的约束

这几条约束是路口mesh和开口cube正确衔接的前提，改动相关代码时要留意：

- **三角形环绕顺序**：UE（左手坐标系）要求从上方看是"顺时针"（标准数学XY凸包意义下的负向
  面积）才是正面朝上——`BuildJunctionMeshes`的顶点顺序要和
  `ForeverTerrainFrameworkComponent::BuildLevel`已验证过的`(v00,v11,v10)/(v00,v01,v11)`
  模式一致。道路本身用真实静态网格资产（如`default_1_0_1_1_0_1`），环绕顺序是资产自带的，不受这条
  约束影响。
- **路口两端的道路tiling必须收缩（`trimStart`/`trimEnd`）**，否则两条路的路面mesh会
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
- **道路tiling的开口缺口按每段合并后开口的精确`[tStart,tEnd]`区间切分保留区间**（区间
  减法，支持同一条路多个开口），不是"整体铺一遍再按段中点跳过"——否则缺口宽度只能凑整到
  unit的倍数，和开口cube的精确宽度对不上。
- **整宽开口cube以`road->GetPoint(t)`为中心、按`Road::GetTotalWidth()`
  对称展开**——这个写法依赖`Connection`连线是整条车道横断面的几何中心（车道居中约定，见
  `roadnet.md`"车道居中"一节），单行道等不对称配置如果连线不在总宽度中点会让cube偏出实际
  路面范围。

## 依赖关系

- 依赖：`Source/Core/map/map.h`（`Map::GetRoads()`/`GetJunctions()`/`GetLots()`/
  `GetPathRoads()`/`GetPathRoadMaterial()`/`AddRoadAccessNode`）、`Source/Core/map/general_geometry.h`
  （`Road`/`RoadJunction`/`Lot`/`Node`）、`ProceduralMeshComponent`模块、
  `Components/InstancedStaticMeshComponent.h`（Engine模块自带，不需要额外启用）。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（`GenerateRoadnetMesh()`阶段
  排在`GenerateTerrainMesh()`之后、全部`BuildMap*`阶段之后，调用
  `roadnetFramework->GenerateRoadnet(map)`）。

## 补充说明
- **路面ISM的自定义数据**：`GetOrCreateRoadISM`给每个ISM设`NumCustomDataFloats=2`，`BuildRoadInstances`每加一个实例写入沿路方向的水平单位向量`(cos, sin)`(索引0/1)。材质里用`PerInstanceCustomData`读出来：沿路方向=`(c, s)`，横向=`(-s, c)`，与世界坐标XY点乘得到顺着路的UV(材质的`TransformVector`在ISM上不带实例旋转，不能用)。
- **坡度+转弯时的接缝台阶与扭转系数（2026-11-25）**：路既转弯又有坡时，相邻两节(各自横轴水平的刚性平面)在接缝线上高度差为`s·delta·w`(`s`=坡度，`delta`=两节转角，`w`=离中轴的横向距离)，两侧符号相反，所以一侧前高后低、另一侧前低后高。**绕中轴线整体转一个角(roll)解决不了**：相邻两节转一样的角，高度差原样保留；要的是沿节长的扭转(z偏移 = `s·kappa·x·y`，x沿路、y横向，以节中心为原点，`kappa`=偏航角变化率，右转为正)。`BuildRoadInstances`算出`k = s·delta/弧长(cm)`写进路面ISM的自定义数据2(`NumCustomDataFloats`=3)，材质WPO里把`k · x · y`加到世界Z上即可：x=`dot(世界位置XY-(自定义数据3,4), (c,s))`、y=`dot(世界位置XY-(自定义数据3,4), (-s,c))`(自定义数据3/4是节中心世界XY；不能用Object Position，它在ISM上是组件原点，x、y会变成到原点的距离、偏移大几十米)，(c,s)是自定义数据0/1(沿路水平方向)，单位cm。材质(`Content/Asset/Roadnet/Default/Material`里的Road/Pavement等)没接这一项前行为不变。
- **扭转改成精确形式（2026-11-25）**：每节两端为补外侧楔形缝会伸出`e`，伸出的端头沿自己的平面延伸，和邻节穿插。点(沿路x，横向y)实际落在中轴线弧长`x/(1-kappa*y)`的截面上，所以WPO的Z偏移取`s*kappa*x*y/(1-kappa*y)`(自定义数据2=`s*kappa`、5=`kappa`，x用世界坐标量因此包含伸出部分)，伸出的端头被拉回真实路面。分母在材质里限幅到不小于0.5。不考虑上下曲率对伸出量的影响(顶棚高度未知，曲率大时伸出量会失控)。
- **单节拉伸范围放宽到[0.75,1.5]*unit（2026-11-25）**：原来是[0.8,1.2]，unit=5m时6~8m(和小于4m)的一段放不下任何整数节，会退化成灰色`AppendFlatRoadCube`矩形。现在由`ROAD_TILE_SCALE_MIN/MAX`控制(unit=5m时每节3.75~7.5m)，能放下的长度区间从第一节起连续(n节可放长度[3.75n,7.5n]，n>=1时相邻区间首尾相接)，只有小于3.75m的一小段还会退化成平板。Core的`Map::LaneSplitKnots`镜像同一套规则，已同步。
- **路口开口边对齐第一节路面（2026-11-27）**：Core的curb点是在缩进点按该点切线取垂线，而贴着路口的那一节路面是刚性实例，端边垂直于本节中点的切线；曲线路上两者差半节转角，路口多边形和路面之间会张开楔形缝(另一侧重叠)。现在`BuildRoadInstances`把贴着路口那一节朝路口的端边两个角点记进`junctionEdges`(键=(road, 是否起点端))，`BuildJunctionMeshes`优先用它们做开口边，没记录(这一端是开口、或这段铺不出单元退化成扁平cube)时仍用Core的curb点。只改路口的可见多边形，导航锚点不变。
- **补缝半宽与坡度取弦（2026-11-25）**：(补缝半宽按mesh包围盒取的做法已撤销，仍按路宽/2。)每节的坡度取弦(首尾两点连线)而不是中点切线，节两端正好落在曲线上，竖向有曲率时相邻节中轴线高度也严格相接；水平朝向仍取切线。
- **路与路交界处的补缝（2026-11-20）**：编辑器把曲线(贴合地形/隧道口)切成很多条首尾相接的短路，交界处和节内接缝一样会张开楔形缝，而之前首尾一律不伸。现在整条路的首/尾若没连路口(`trim==0`且这一节是路的真正首/尾，不是开口旁边的)，也按本节曲率估计的 `e` 伸出去。连路口的一端不伸；死胡同端点只多伸很小一截。
