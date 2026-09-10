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
- **路口mesh是直线简化版**（`BuildJunctionMeshes`）：每个`RoadJunction`的`approaches`已经按
  夹角排好序，把`curbRight[i]`/`curbLeft[i]`两两相邻连成边界点序列（`(right_i,left_i)`是
  road i自己的"开口宽度"边，`(left_i,right_{i+1})`是road i与road i+1之间的桥接边），从路口
  中心（`Intersection`自身坐标）扇形三角剖分，贴`RoadPlain`材质。不做圆角/斜切。
- **车道分裂demo是临时验证代码**（`SpawnAccessNodeDemo`，函数注释里明确标注"临时验证"）：
  取`map->GetLots()`第一个lot的边界`Road`映射中任意一条，调一次`Map::AddRoadAccessNode`
  （车行、demo宽度0.6地图单位=6m）。这段demo代码要**在**`BuildRoadInstances`/
  `BuildOpeningMeshes`遍历所有Road**之前**先跑，这样它新增的开口才能被正确画出来（否则
  开口mesh在demo调用之前就已经CreateMeshSection完毕，不会再刷新）。
- **Lot调试可视化（黄色扁cube）和车行/行人导航图可视化都已按用户要求删除**——两者都只是
  阶段性调试手段，确认对应数据（lot几何/地址、导航图节点与边）正确无误后就移除了，不占用
  这个组件的常驻运行开销。`Map::GetVehicleNavGraph()`/`GetPedestrianNavGraph()`/
  `GetNavAnchorNodes()`这几个Core层accessor本身保留（未来Traffic域寻路要用），只是Forever层
  不再消费它们画debug mesh；如果以后又需要类似的可视化，可以参考git历史里这段代码（`AppendNavBox`/
  `AppendQuadDoubleSided`双面出三角形的debug mesh技巧仍然适用）重新加回来。
- **和Terrain一样固定`worldScale=1000.f`（1地图单位=10m=1000cm）**，`Node`/`Connection`/
  `Road`等Core层几何类型的坐标都是地图单位，Forever层建mesh时统一在最后一步乘以这个系数转
  世界坐标，中间计算全部保持地图单位，避免在算式里混用两种单位。

## 依赖关系

- 依赖：`Source/Core/map/map.h`（`Map::GetRoads()`/`GetJunctions()`/`GetLots()`/
  `AddRoadAccessNode`）、`Source/Core/map/geometry.h`（`Road`/`RoadJunction`/`Lot`/`Node`）、
  `ProceduralMeshComponent`模块、`Components/InstancedStaticMeshComponent.h`（Engine模块自带，
  不需要额外启用）。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（`EnsureMapGenerated()`
  在`terrainFramework->GenerateTerrain(map)`之后调用`roadnetFramework->GenerateRoadnet(map)`）。

## 待办/后续阶段

- 阶段4：Zone/Building迁移后，`SpawnAccessNodeDemo`应该被移除或改造成真正由Building/Zone
  在放置建筑时调用`Map::AddRoadAccessNode`。
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
