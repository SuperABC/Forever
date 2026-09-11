# ForeverRoadnetFrameworkComponent

## 职责

`UForeverRoadnetFrameworkComponent`是阶段4-1 Roadnet落地的Forever层组件，从空`UCLASS()`升级
成真正的路网渲染组件。老工程`RoadnetBase.UpdateRoadnet()`是纯`BlueprintImplementableEvent`
空实现，所有实际mesh搭建逻辑都在Blueprint里；这次不参考老蓝图（用户10条要求已经把新设计的
车道/开口/路口mesh规则说清楚了），完全是新写的C++ mesh生成逻辑，模式上参照
`ForeverTerrainFrameworkComponent`（procedural mesh、材质加载via`ConstructorHelpers`/
`LoadObject`、组件挂在owner的RootComponent下）。

## 关键设计

- **道路正常路段的可见效果直接来自`Road::mesh`本身**——`mesh`资产（默认`default_1_1`）本身
  已经画好了车道+人行道横断面，`vehicleLanes`/`parkingLanes`/`pedestrianLanes`这几组宽度数据
  只用于结构计算（导航锚点偏移、路口路缘角点、开口cube宽度），不驱动任何单独的procedural
  条带或材质。`BuildRoadInstances`把老工程`UMeshArray::RepeatMesh1D`/`RepeatMeshAlongCurve`
  的分段数量算法（"缩放后的mesh长度落在[0.8,1.2]×原始长度区间内，取最接近total/unit的整数
  段数"）移植到这里，但**没有**重新实现老工程那套自建弧长采样表——因为`Connection::GetPoint(f)`
  本身就是弧长参数化的（`Source/Dependence/map/geometry.cpp`的`ResolveArcLengthParam`，128点
  惰性缓存），直接按`f=k/n`取点就已经是等弧长分段，不需要在这个组件里再建一份256点采样表。
  用`UInstancedStaticMeshComponent`按分段结果摆放`mesh`资产实例——这是要求9"沿路线分隔mesh
  元素"+"曲线道路"两点的落地方式，一个机制同时满足两点（ISM在这里是合理选型：重复摆放同一份
  mesh资产，和Terrain迁移时"挖洞不能用ISM"是两回事，那次禁的是用ISM实现地面挖洞占位）。
- **每条唯一的mesh资产路径对应一个共享的`UInstancedStaticMeshComponent`**（`GetOrCreateRoadISM`
  按路径查/建），不是每条Road各自一个ISM——这次默认所有Road都用同一个`default_1_1`，只会建
  一个ISM，但接口上支持未来mod给不同Road配不同mesh资产。
- **开口（要求5）：整条路横断面宽度都被替换成一块贴`RoadPlain`材质的扁平cube**，不是精细地
  只挖开人行道/停车道那一部分——`BuildRoadInstances`在命中`Road::GetOpenings()`对应弧长区间
  时跳过该分段mesh实例的摆放，`BuildOpeningMeshes`在同一段范围画一块覆盖整条路总宽度
  （车行+停车+人行道两侧加总）的cube。这是要求5"不再指定道路mesh资产，换成贴路面材质的
  cube"的直接体现——没有尝试对不同车道类别做精细的部分挖空，简化成"这一段整体换成扁平材质"。
  两处cube生成共用同一份几何逻辑（`AppendFlatRoadCube`：给定road上的中心弧长比例+沿路长度+
  横向宽度，画一块扁平quad），不是各自重复实现一遍。
- **`BuildRoadInstances`按`unit`铺不出至少一节`default_x_x_x`实例的短缺口，同样退化成
  `RoadPlain`扁平cube（第十二轮迁移）**：`tileRange`原来遇到"这段范围内连一个满足
  `[0.8,1.2]×unit`约束的分段数都凑不出来"（`nLow>nHigh`或`nLow<=0`，比如两个相邻开口之间/
  端点和第一个开口之间只剩很短一截）就直接跳过、什么都不画，导致路面出现视觉空隙；现在改成
  调`AppendFlatRoadCube(road, centerT, rangeLen, road->GetTotalWidth(), ...)`，把这一小段
  整个填成一块和开口处理方式完全一样的扁平cube（顶点数据追加进`BuildRoadInstances`/
  `BuildOpeningMeshes`共用的那份`outVertices`/`outTriangles`/`outUvs`，最终都进
  `openingMesh`那一个mesh section、贴`openingMaterial`），不再留空隙。
- **路口mesh是直线简化版**（`BuildJunctionMeshes`）：每个`RoadJunction`的`approaches`已经按
  夹角排好序，把`curbRight[i]`/`curbLeft[i]`两两相邻连成边界点序列（`(right_i,left_i)`是
  road i自己的"开口宽度"边，`(left_i,right_{i+1})`是road i与road i+1之间的桥接边），从路口
  中心（`Intersection`自身坐标）扇形三角剖分，贴`RoadPlain`材质。不做圆角/斜切。
- **开口cube和路口mesh都带碰撞**——`CreateMeshSection`的`bCreateCollision`参数从`false`改成
  `true`（两处都要改，PIE验证发现最初漏加，玩家会直接从开口/路口掉到地形挖出的洞里）。道路
  本身的ISM实例走`UInstancedStaticMeshComponent`默认碰撞（跟着`default_1_1`资产自带的
  collision setup走，不用额外设置）。
- **`SpawnAccessNodeDemo`这个临时验证demo已删除**（第十一轮迁移，Zone/Building裁剪Lot自由
  空间时接到"大路"的小路现在会真正调用`Road::AddOpening`产出开口，见`Source/Dependence/
  map/geometry.md`"关键设计"一节`Lot::SplitWithPath`——之前那个demo任意挑`map->GetLots()`
  第一个lot的第一条边界Road在`t=0.5`处调`Map::AddRoadAccessNode`，纯粹是给"开口cube这套
  逻辑还没有真正调用方"这个阶段性问题临时找的验证手段，现在有真实调用方了，demo连同它
  临时演示用的车行/行人`AddRoadAccessNode`调用一起删掉）。`Map::AddRoadAccessNode`本身
  （同时断开导航图车道贯通线+标记`RoadOpening`）作为API继续保留，只是暂时没有调用方——
  小路开口这条路径**不**调用它，只直接`endRoad->AddOpening(...)`标记路面缺口，不碰导航图
  （用户明确要求这次先不接导航node，等以后设计好小路的导航接入方式再改）。
  - **`BuildOpeningMeshes`按`t`去重，同一个物理开口位置只画一块cube**——如果同一个位置先后
    被不同来源（`Map::AddRoadAccessNode`/`Lot::SplitWithPath`）各标一次`RoadOpening`、
    `t`/`width`恰好相同，遍历`openings`时按`t`（1e-4误差范围内视为同一个）去重，只画一次，
    不会重叠画出两个完全重合的扁平quad。
- **Lot调试可视化（黄色扁cube）已按用户要求删除，导航图可视化后来又重新加回（第七轮迁移）**——
  两者最初都只是阶段性调试手段，确认对应数据（lot几何/地址、导航图节点与边）正确无误后先
  一起删掉了；后续因为要继续改路网算法（车道居中等）、以及未来Building域接入导航之后还要
  反复核对导航图连接是否正确，导航图可视化这部分又要回来了，这次改成常驻功能、用
  `bShowNavigationDebug`（`EditAnywhere`）开关控制，不是一次性debug代码。**这部分代码在
  被删除时还没有提交过commit，git历史里找不到旧版本**（`ForeverRoadnetFrameworkComponent.cpp`
  只有一个包含完整Roadnet实现的commit，删除发生在那次commit之前的工作区编辑里），这次是
  按照删除前记录在这份文档里的设计描述（node画小box、connection画双面ribbon、车行贴White
  材质、行人贴RoadPlain材质、元素要有实际厚度否则PIE里看不见）重新实现的，不是原样恢复旧
  代码。`BuildNavigationDebugMesh()`（公开方法）+`BuildNavGraphDebugMesh()`（私有实现，
  给车行/行人各调一次）：
  - `AppendQuadDoubleSided`（匿名namespace自由函数）：四个角点无论以什么环绕顺序传入，两个
    方向的三角形都画一遍——debug mesh的box/ribbon朝向五花八门（任意角度的路口连接线、任意
    朝向的道路），不值得为每个面单独推导"哪个环绕顺序才是正面朝上"，双面画一遍最省心，反正
    只是调试用不追求正确光照。
  - `AppendNavBox`：给每个锚点画一个轴对齐的小长方体（6个面都调`AppendQuadDoubleSided`），
    水平半边长`NAV_DEBUG_NODE_HALF_SIZE=40`，竖直范围`[锚点真实Z+ROADNET_HEIGHT_EPSILON,
    该值+NAV_DEBUG_HEIGHT]`——用锚点自己的真实Z（不是固定0），隧道场景下的锚点会正确显示在
    地下。`AppendNavEdgeRibbon`：两端锚点的box顶面高度之间连一条细双面ribbon
    （`NAV_DEBUG_EDGE_HALF_WIDTH=8`），让边看起来是从box顶接出去的。
  - **元素必须有真正的竖直厚度（`NAV_DEBUG_HEIGHT=50`，约5cm），不能是单一Z高度的纯平面**——
    最初尝试过退化成纯平面（和开口/路口mesh一样只在Z上加`ROADNET_HEIGHT_EPSILON`），PIE验证
    完全看不见，换成有真实厚度的3D box/ribbon才稳定可见，具体数值是反复PIE调出来的经验值，
    不是精确物理尺寸。
  - **同一个锚点被多条边引用时只画一次box**（`unordered_set<int> visitedNodes`按Node id去重，
    在遍历图的边时顺带收集）——导航图数据结构本身是`unordered_map<id, vector<pair<id,
    Connection*>>>`，同一个锚点作为多条边的起点/终点很常见（比如路口车行全联通时一个入口
    锚点会连到好几个出口），不去重会画出好几个完全重叠的box。
  - **不需要单独维护一份"锚点id→坐标"的查找表**——每条边的`Connection*`本身就带着两端真实
    `Node`（`GetStart()`/`GetEnd()`），遍历边的同时就能拿到端点坐标，比反查`Map::
    GetNavAnchorNodes()`+处理"extern端点没有生成新锚点、要用原始Node"这种特殊情况简单。
  - `bShowNavigationDebug`为`false`时不是跳过不生成，而是显式`ClearMeshSection(0)`——避免
    "曾经打开过再关掉"时旧的可视化mesh一直残留在场景里。
  - **暴露成公开方法，不是只在`GenerateRoadnet`末尾私下调一次**：以后Building域会在运行时
    继续用类似`Map::AddRoadAccessNode`的接口往导航图里加锚点/边，那时候需要能重新调用
    `BuildNavigationDebugMesh()`刷新可视化、核对新增的导航连接对不对，不能假设只有Roadnet
    自己生成时的那一份数据是唯一需要可视化的时机。
- **和Terrain一样固定`worldScale=1000.f`（1地图单位=10m=1000cm）**，`Node`/`Connection`/
  `Road`等Core层几何类型的坐标都是地图单位，Forever层建mesh时统一在最后一步乘以这个系数转
  世界坐标，中间计算全部保持地图单位，避免在算式里混用两种单位。
- **`BuildPathRoadMeshes`（Zone/Building落地时新增）**：遍历`map->GetPathRoads()`（Zone/
  Building裁剪Lot自由空间时自动生成的小路），每条按起止点+`Road::GetTotalWidth()`生成一个
  贴材质的扁平ribbon，不接入`BuildRoadInstances`那套按`default_x_x_x`资产选mesh的ISM管线——
  小路车道宽度是0.3/0.2这种非整车道宽度，套不进那套命名约定，见`Source/Basic/map/
  roadnet_basic.md`。材质优先按`map->GetPathRoadMaterial()`的字符串路径**运行时**
  `StaticLoadObject`加载（不是`roadPlainBaseMaterial`那种只能在构造函数里用的
  `ConstructorHelpers::FObjectFinder`，因为这个路径是`RoadnetMod`运行时数据，编译期不知道
  具体是哪个资产），取不到就退化用`roadPlainBaseMaterial`。必须在`GenerateRoadnet`里调用
  （紧跟`BuildJunctionMeshes`之后），且`AForeverFrameworkActor::EnsureMapGenerated()`必须
  保证`Map::InitZones()`/`InitBuildings()`已经跑完再调`GenerateRoadnet`，否则
  `GetPathRoads()`还是空的。

## 依赖关系

- 依赖：`Source/Core/map/map.h`（`Map::GetRoads()`/`GetJunctions()`/`GetLots()`/
  `GetPathRoads()`/`GetPathRoadMaterial()`/`AddRoadAccessNode`）、`Source/Core/map/geometry.h`
  （`Road`/`RoadJunction`/`Lot`/`Node`）、`ProceduralMeshComponent`模块、
  `Components/InstancedStaticMeshComponent.h`（Engine模块自带，不需要额外启用）。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（`EnsureMapGenerated()`
  在`terrainFramework->GenerateTerrain(map)`之后、`InitZones`/`InitBuildings`跑完之后调用
  `roadnetFramework->GenerateRoadnet(map)`）。

## 待办/后续阶段

- 阶段4：小路接大路的开口这次只标`RoadOpening`几何标记，不碰导航图（`Map::
  AddRoadAccessNode`那套断线逻辑），等以后设计好小路的导航接入方式再补，见
  `Source/Dependence/map/geometry.md`。
- 阶段4：路口mesh的圆角/斜切、开口cube的精细化（只挖开人行道/停车道而不是整条路宽度）如果
  以后有真实需求，再回来加，这次是明确的简化范围，不是遗漏。
- **已修复（PIE验证发现，共两轮）**：
  1. 开口cube/路口mesh/Lot调试块三处procedural mesh的三角形环绕顺序最初全部写反了——UE
     （左手坐标系）要求从上方看是"顺时针"（标准数学XY凸包意义下的负向面积）才是正面朝上，
     早期实现套用的是"逆时针"顺序（对照`ForeverTerrainFrameworkComponent::BuildLevel`已验证
     过的`(v00,v11,v10)/(v00,v01,v11)`模式重新核对后发现三处都反了），导致这三块mesh从上方
     完全不可见。道路本身用的是真实静态网格资产（`default_1_1`），环绕顺序是资产自带的，不受
     这个bug影响，所以唯独道路是正常的。已在`BuildOpeningMeshes`/`BuildJunctionMeshes`/
     `BuildLotDebugVisualization`（这个函数后来按用户要求整体删除，见上"关键设计"一节的
     导航图可视化说明）三处把三角形顶点顺序都反过来。
  2. 环绕顺序修好后PIE复测又发现两个新问题：①**路口处两条路的`default_1_1`还是会重叠
     z-fighting**——`BuildRoadInstances`原来铺mesh是沿整条`[0,1]`弧长铺到底，两端严丝合缝
     顶到`Intersection`的精确坐标，路口mesh虽然现在可见了，但只是叠加在这堆重叠的路面mesh
     **上面**，并没有让底下两条路的mesh本身互不重叠。修复：`BuildRoadInstances`新增
     `trimStart`/`trimEnd`参数（地图单位），把tiling范围往回收缩这么多再铺；收缩量按
     `GenerateRoadnet`里遍历`map->GetJunctions()`算出的"该road在这一端的横断面总宽度"
     （车行+停车+人行道两侧加总，和`RoadJunction::Build`算路缘角点偏移量用的是同一个量级），
     只有连着真实路口的那一端才收缩，连着`extern`（地图边缘）的那一端不收缩。②**开口cube
     还是空的一段，没画出来**——开口cube和路口mesh都铺在Z=0（地图单位），这一片`"plain"`
     地形本身高度也接近0，和地形网格发生了共面z-fighting（和
     `ForeverTerrainFrameworkComponent.cpp`的`HEIGHT_EPSILON`是同一类问题）。修复：开口cube
     和路口mesh的顶点Z都加一个`ROADNET_HEIGHT_EPSILON`。**第一次PIE验证时用的2mm不够**（路口
     和开口依然被地形盖住看不见），改成5cm（世界单位50）验证有效后，按用户要求收到1cm
     （世界单位10）——相对10m一个地图格子的尺度依然可以忽略不计，足够压过地形高度采样的
     误差范围又不会太明显地"浮空"。
  3. 上面两轮修复之后PIE又发现两个更深层的问题，根源是同一个：**`RoadJunction::Build`算的
     路缘角点最初只沿垂直于道路的方向横向偏移，没有沿道路方向往外移一段进深**——路口多边形
     因此对每条路来说几乎是"零深度"的点状扇形，贴在Intersection的精确坐标上；而
     `BuildRoadInstances`的道路tiling收缩量(`trimStart`/`trimEnd`)当时是单独按该road横断面
     "两侧宽度加总"算的，比路口多边形的实际尺寸大得多——于是出现：①道路两端收缩了一大截
     （比实际需要的多），②路口mesh本身进深几乎为0，完全撑不满收缩出来的空当，看起来路口处
     "什么都没有"，③开口cube的宽度是按`RoadOpening::width`精确计算的，但道路tiling原来是
     按"整段unit为单位、只看每段中点是否落进开口范围"来跳过实例，缺口宽度只能凑整到unit的
     倍数，和cube精确宽度对不上（缺口通常比cube宽）。修复分两处：①`RoadJunctionApproach`
     新增`setback`字段（取该端两侧车道总宽度里较宽的一侧），路缘角点/导航锚点的基准点从
     "Intersection原坐标"改成"沿outward方向外移setback距离后的点"，让路口多边形对每条路都有
     一段真实进深；`BuildRoadInstances`的收缩量直接读同一个`ap.setback`，不再自己另算，保证
     两边严丝合缝。②`BuildRoadInstances`不再是"整体铺一遍再按段中点跳过"，改成先把
     `[tLow,tHigh]`按每个开口的精确`[t-halfFrac,t+halfFrac]`范围切成若干互不重叠的"保留区间"
     （区间减法，支持同一条路多个开口），每个保留区间各自独立铺tiling——铺出来的缺口因此
     精确等于开口本身的宽度，不再受unit分段粒度影响。
  4. **隧道落地后PIE发现路口mesh两个高度问题**：①隧道段内的路口完全按地表高度渲染，看起来
     像是直接贴到地面上而不是在隧道里；②隧道口处可见路面和路口mesh衔接的地方有台阶断层。
     根因是`BuildJunctionMeshes`原来对中心点和所有curb点统一用`ROADNET_HEIGHT_EPSILON`当Z，
     完全不管`Intersection`/curb点实际所在弧长位置的真实高度；curb点的基准点本身在Core层
     （`RoadJunction::Build`）也只是"Intersection坐标+沿切线方向的直线外移"，没有真正采样
     曲线的Z（隧道口那段S形坡道Z沿途连续变化，直线近似完全漏掉这段变化，跟`BuildRoadInstances`
     实际铺出来的路面衔接点对不上）。修复：Core层`RoadJunctionApproach`新增`curbZ`字段
     （详见`Source/Core/map/roadnet.md`"路口高度"一节），`Build()`改成先按`setback`/
     `road->CalcDistance()`算出和`BuildRoadInstances`的`trimStart`/`trimEnd`同一套clamp公式
     的`sampleT`，再用`road->GetPoint(sampleT)`采样真实X/Y/Z作为curb点/导航锚点的基准点。
     Forever层这里对应：中心点Z改用`junction->GetNode()->GetZ() * ROAD_WORLD_SCALE +
     ROADNET_HEIGHT_EPSILON`，每个curb点Z改用`ap.curbZ * ROAD_WORLD_SCALE +
     ROADNET_HEIGHT_EPSILON`——扇形三角化允许每个顶点独立取高度，不要求整个路口多边形共面，
     隧道场景下中心点和各条路的curb点高度本来就可能不同（比如隧道口一侧curb点还在地表附近、
     中心点已经在隧道内下沉）。
  5. **"车道居中"改造（第六轮，详见`Source/Core/map/roadnet.md`"车道居中"一节）顺带简化了
     `BuildOpeningMeshes`**：这个函数一直是按"cube以`road->GetPoint(op.t)`为中心，往
     两侧各展开`totalWidth`的一半"来画的（`cx±perpX*halfWidth`），这个写法本身在
     `Connection`连线被重新定义成整条车道横断面的几何中心之后才是真正成立的——居中之前，
     这其实是一个隐藏的假设错误（默认车道配置两侧对称，连线恰好也在总宽度中点，错误从未
     暴露；单行道等不对称配置会让cube偏出实际路面范围）。居中改造之后不需要改这个函数的
     计算逻辑，只是把`totalWidth`的来源从本地手写的`SumLanes`对六个车道数组分别求和
     （已删除，与`roadnet.cpp`/`roadnet_basic.cpp`里同样内容的本地helper重复三次）换成
     `Road::GetTotalWidth()`（Dependence层收敛出的统一实现）。
