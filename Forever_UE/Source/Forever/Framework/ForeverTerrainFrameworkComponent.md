# ForeverTerrainFrameworkComponent

## 职责

对应旧Framework Actor `Terrain`（C++ Base: `TerrainBase`）。负责动态LOD（clipmap式）地形网格、
水面网格、隧道口侧壁mesh的生成，以及随玩家位置更新LOD；同时维护近/远两套地形材质的贴图数组和控制纹理。

## 关键设计

- **LOD网格**：`TERRAIN_LOD_GRID_SUBDIV`（.cpp顶部宏，当前64，即65x65个顶点）对所有级别统一，
  级与级之间只有物理尺寸`size`（`4·2^i`个element）不同。128实测太卡。`BuildLevel`里跟它绑定的数
  （顶点数组大小、行跨度、父子边界拼接偏移`idxX/idxY * (SUBDIV/4)`、sub-quad循环边界）都由它推导；
  `childPos`的16位掩码表示固定4x4子级覆盖关系，与细分数无关，所以`quadrantSize = SUBDIV/4`。
  `TickComponent`按玩家位置算各级pivot（夹在地图范围内），`boundaryChanged`/`needsRebuild`从粗到细传播并重建，
  让父级修正过的边界顶点（`lodBoundaries`）传给子级做接缝修正（奇数边界顶点取相邻偶数顶点平均）。
  世界还在分阶段生成（`IsInitialGenerationDone()`为假）时不更新，LOD环要围着真正的玩家转。
- **顶点高度走`Map::SampleHeight`**（含细化高度数组）：主网格顶点、construction/挖洞三角形顶点、出生点都走它。
  第i级LOD每段长`4·2^i/64`个element（L0=1/16、L1=1/8、L2=1/4、L3=1/2、L4起≥1），起点都是整数element坐标，
  所以各级顶点正好落在17x17细化网格的下标上，只点采样，不平均；L4及以上只读到角点（=粗高度）。
  细化数组的约定见`Source/Core/map/map.md`"细化高度"一节、`terrain_mod.md`。
- **小格拆三角形的对角线自适应**（`BuildLevel`里的`anti`）：和`Map::SampleHeight`的粗高度插值同一个规则（取两端高度差较小的对角线），斜着走的地形交界才会是笔直的斜坡而不是锯齿。L0~L3的小格不跨element：没有细化数组的element直接问`Map::UsesAntiDiagonal`（网格和采样严格同一对角线）；有细化数组的element和更粗的LOD（小格跨多个element）看这个小格4个顶点自己的高度。`clipGridCell`（hatch裁剪）按同一个`anti`选两个三角形，construction整格替换不受影响（平地）。
- **法线放在顶点上、从高度场求**：`BuildLevel`里每个顶点的法线=`SampleHeight`在`NORMAL_SAMPLE_STEP`(0.25 element，各级LOD一致)步长下的中心差分梯度，不再用相邻三角形面法线累加。法线只取决于位置，所以对角线怎么拆、construction整格替换/hatch裁剪交点这些不共享的独立顶点、各级LOD之间，同一位置的法线完全相同，GPU在三角形内插值后连续变化，三角形边界不会出棱角。代价是整数格点处的陡坎被抹开约0.25 element。水面/隧道墙的法线逻辑没动。
- **`UActorComponent`不是Actor**：运行时创建的子组件要挂在`GetOwner()`上
  （`NewObject<...>(owner)`+`SetupAttachment(owner->GetRootComponent())`+`owner->AddInstanceComponent`），不是`this`。
- **`Map`的生命周期不归本组件管**：`AForeverFrameworkActor`持有`Map*`，`Map::InitTerrains()`跑完后调`GenerateTerrain(map)`，
  组件只是非持有地引用它。玩家位置取`GetPlayerPawn(0)`（Terrain不依赖其它concept，项目无分屏）。
- **`IsInsideLodLevel(worldPos, level, scale)`**：判断某点是否在第level级LOD当前覆盖的正方形内（scale<1时按中心缩小，做进入/离开滞回），
  供`ForeverTrafficFrameworkComponent`的公交远近（物理/非物理）切换使用。
- **材质参数名**：`terrainMaterial`（远处，`TemplateFar`）用`TerrainDiffuseArray`/`TerrainNormalRoughArray`/
  `TerrainIndexMap`/`TerrainMapSize`；`nearMaterial`（近处，`TemplateNear`）用`TerrainDiffuseArray`/
  `TerrainNormalRoughArray`/`NearIndexMap`/`NearWeightMap`/`NearRegion`。名字要和材质图里的参数名一致，
  设错不报错只是静默不生效，PIE看贴图是否正确显示是唯一验证手段。
- **固定基础材质**（`TemplateFar`/`TemplateNear`/`Pure`/Water插件材质）走`ConstructorHelpers::FObjectFinder`（`EditDefaultsOnly`可覆盖）；
  每个地形Mod实例的diffuse/normal/roughness贴图路径是运行时字符串，走`LoadObject<UTexture2D>`。
- `FForeverLodBoundary`是普通C++结构体，不是`USTRUCT`（没有Blueprint消费方）。

### 挖洞（construction格子与hatch）

挖洞在网格生成时用精确凸多边形裁剪完成，不用ISM小立方体。`Map::AddHatch`登记的hatch是旋转矩形（`Quad`+`rotation`），
挖洞条件是"该格子有hatch"(不论地形类型)。隧道口所在格子是`mountain`/`plain`，永远不会被晋升成`construction`，
如果只认`construction`，隧道口hatch会被忽略、隧道被山体实心地形挡住。`RoadnetMod::AddHatch`记hatch，`Map::InitRoadnet()`转发进`Map::AddHatch`。

**退化碎片过滤**：裁剪会在"顶点正好落在hatch边的直线上"或"相邻两个hatch共边"(例如twin布局进深取满4时，bg+的6个hatch两两首尾相接)时切出重复点/共线点的零面积碎片。碎片`Num()>=3`却面积为0，建成三角形就是退化三角形，Chaos给地形建碰撞体时会报`Input trimesh contains N bad triangles`。所以收下外侧碎片前用`PolygonAreaAbs`过滤：clipGridCell在世界坐标里小于`kMinWorldPieceArea`(10cm²，世界坐标几十万量级时float精度0.03~0.06产生的噪声碎片就在这个量级)丢掉。只按整块面积过滤不够：碎片里有重复点/共线点时整块面积为正，但扇形三角化里会混进零面积三角形，所以扇形输出时还要逐个三角形判面积(`kMinWorldTriArea`=1cm²)，太小的跳过，交点顶点也只在真正用到时才追加。

**一条路径（2026-11-20起）→`BuildLevel`里的`clipGridCell`**（`levelIdx<=TERRAIN_HATCH_MAX_LEVEL`=3，L0~L3的网格小格都不跨element）：
有hatch的格子(不论地形类型，construction也一样)，只对这个网格小格的两个三角形，按该element全部hatch的旋转矩形做4边半平面裁剪
（`FClipPoint`/`SplitClipPolygonByLine`，Sutherland-Hodgman单边裁剪的双输出版本，保持原绕序，顶点带世界坐标和网格顶点下标）：
落在某条边外侧的确定在矩形外，收进`survivingPieces`；4条边都在内侧的才是洞，丢弃；每个格子独立裁剪，不依赖"hatch完全落在单一格子内"。
没被切到的小格仍是共享网格顶点的普通两个三角形，完全在洞里的丢弃，被切到的保留洞外部分——角点复用网格顶点（无裂缝、法线连续，包含格子里的细化高度），
只有矩形边上的交点追加新顶点，高度是原三角形平面上的3D线性插值（落在原渲染表面上，含LOD接缝修正）。绕序和主网格同一套(`(p0,p[i+1],p[i])`)。
- **已删除**：原先对`construction`格子(平地)在`levelIdx<=1`用`LookupTerrain`整格替换成"一格一个正方形面片"的简化(没有hatch的construction格子只有2个三角形)。
  它的格子边只有四角高度，和邻格的细分顶点对不上，相邻格子之间会出现缝隙；`clipGridCell`的裁剪已经能在稠密网格上正确处理hatch，所以整套`LookupTerrain`/`FTerrainTri2D`/`FanTriangulate`都去掉了，
  construction格子现在和别的地形一样走稠密网格(代价：近处construction区域的三角形数比原来多，要看PIE帧率)。
- **不要改回"轴对齐外接矩形减矩形+角落补丁三角形"的近似**：它假定hatch整个落在单一格子内，而隧道口hatch（一段Connection的长度，1.5~2.5地图单位）经常跨2~3个格子，
  会挖出AABB粗糙洞、缺角落垫平，玩家沿路走进隧道口会因缺失的地形碰撞穿模掉下去。

起伏地形上的"路"不靠挖洞：路网通过`RoadnetMod::AddTerrainFit`让地形高度贴到路面高度（`Map::ApplyTerrainFits`直接改`fineHeights`，见`Core/map/map.md`"地形贴合"），
UE这边照常读`SampleHeight`。hatch只用来挖洞。

### 隧道口侧壁（`BuildTunnelWalls`）

地形是单层面片，洞沿没有东西，从侧面/低角度能透过洞沿看到地形背面（被剔除）。`TerrainHatch::wall=true`的hatch
（只有`RoadnetMod::AddTunnelHatch`产出的隧道口）由`BuildTunnelWalls()`在`GenerateTerrain`末尾一次性生成侧壁mesh（`tunnelWallMesh`，和LOD无关），
数据来源`Map::GetTunnelWallHatches()`（取自`Roadnet::hatches`，读档后也在）：

- 洞的两条长边（沿`rotation`，横向±sizeY/2）各一条竖直墙，**不加两个端头**（起点端是和地面齐平的路面入口，终点端是隧道本身，隧道里的墙和顶棚用资产）。
- 沿长度每`TUNNEL_WALL_SEGMENT`(0.25)一个断面；墙顶不低于所在地形(2026-11-20改：墙顶点取前后各半个断面间距内、墙内外两侧`SampleHeight`的最大值，可略突出地面但不留空缺，和端墙一致；原先是`SampleHeight`−5cm，已删`TUNNEL_WALL_TOP_INSET`)；
  墙底=洞底（`floorStart→floorEnd`的smoothstep，和S形下坡曲线两端水平一致）−`TUNNEL_WALL_BOTTOM_EXTRA`(埋在路面下)；地形不比洞底高的断面跳过。
- 做成有厚度（`TUNNEL_WALL_THICKNESS`）的小板：朝洞内的内面+朝洞外的外面+顶面，三角形绕序按期望法线校正，`CreateMeshSection`带碰撞。
- 材质用单色`Pure`（`tunnelWallBaseMaterial`，Color参数）：地形材质按世界XY投影采样，竖直面上是拉伸的竖条纹。

## 近处地形混合

近处（细LOD）与远处使用不同材质，材质**不读网格UV，全部用世界坐标**（`AbsoluteWorldPosition/1000`=地图element坐标`P`）：
`BuildLevel`的`CreateMeshSection`不传UV，主网格与construction/挖洞三角形因此天然统一（混用两种UV会采到回绕后的随机内容）。

- **控制纹理区域=`nearControlLevel`那一级LOD的覆盖区域**（`EditDefaultsOnly`，默认3，夹到`[0,num-1]`，即32个element）：
  `NearIndexMap`/`NearWeightMap`两张持久纹理（`GenerateTerrain`创建一次，`RebuildNearControl`原地更新），
  分辨率`size×nearTexelsPerElement`（默认16 texel/element）。内容只取决于地图数据和texel的世界位置，区域平移/重建时同一个世界点颜色不跳变。
  算法见`Source/Core/map/terrain.md`"近处地形混合"一节，UE侧只是把`NearTerrainBlend::BuildRegion`的输出字节数组拷进纹理。
  `nearControlLevel`取大一些把近/远缝合线挪远，缓解K在两种材质里算法不一致导致的法线接缝；`nearBlendRadius`（默认2，夹到[1,3]）决定近处过渡带宽度。
- **只在`nearControlLevel`自己的pivot变化时重建纹理**：`TickComponent`从粗到细的重建循环里，`i==nearControlLevel`那次迭代在`BuildLevel`之前先调`RebuildNearControl`；
  粗一级pivot变化时更细级必然也重建，所以纹理和网格总是同一帧更新。`RebuildNearControl`内部还有`nearRegionPivot`比较，没变化直接跳过。
- **`levelIdx<=nearControlLevel`的所有LOD共用同一个`nearMaterial`**。`nearSliceIds`/`nearIsConstruction`（整张地图，`GenerateTerrain`里和`terrainIndexMap`同一次扫描填好）
  是`BuildRegion`的输入，不用每次重扫全图。
- **法线+粗糙度**：`TerrainNormalRoughArray`（BGRA8，sRGB关，和`TerrainDiffuseArray`槽位对齐），每个槽位把法线源的R/G和粗糙度源的R打包进B/G/R通道，A=255。
  近/远材质都只采样"权重最高的那个地形"的法线粗糙度（近处是槽位0，远处是四个角里总权重最高的id），省采样次数，交界处细节借助基于主导权重的淡入系数淡成平的。
  源贴图路径为空/加载失败时，该槽位整层填平法线`(128,128)`+中性粗糙度`204`（约0.8）并打Warning；diffuse读不到时填中性灰。
- **远处的2×2双线性**：`TemplateFar`继续用全图一张`terrainIndexMap`（PF_G8，每像素=地形槽位下标），在着色器里对相邻4个element的id双线性混合；
  `TerrainMapSize`材质参数（真实地图宽高）供着色器越界时夹到边缘element。

## 水面（`BuildOceanMesh`）

筛选条件是`map->GetWater(x,y).first`（有没有水），不检查地形类型，所以海洋和`RiverTerrain`通过Core的per-cell`setWater`设了水面的河流格子都在同一张水面mesh里
（见`terrain_mod.md`"水面"一节、`terrain_*.md`）。共享顶点网格（`(width+1)*(height+1)`个格点），有水的格子把4个角写成各自的水位，相邻格子水位不同时共享顶点被后写的覆盖（不是平均），
有轻微接缝，暂不处理。河流水面会倾斜，所以按面法线面积加权累加到顶点、归一化提供法线/切线。函数名沿用`BuildOceanMesh`。

## 依赖关系

- 依赖：`Source/Core/map/map.h`（`Map`，非拥有指针）、`Source/Core/map/terrain.h`（`NearTerrainBlend`，纯标准库）、
  UE的`ProceduralMeshComponent`/`Water`插件模块（见`Forever.Build.cs`）、`Content/Asset/Materials/{TemplateFar,TemplateNear,Pure}.uasset`
  （编辑器里手工搭建的材质图）、`Content/Asset/Textures/Terrain/*`（diffuse+normal+roughness三套贴图）。
- 被谁依赖：`AForeverFrameworkActor`（`CreateDefaultSubobject`创建、`EnsureTerrainGenerated()`里调用`GenerateTerrain`）、
  `AForeverPlayerGameMode`（`GetMapCenterWorldLocation()`算出生点）、`UForeverTrafficFrameworkComponent`（`IsInsideLodLevel`）。

## 待办/后续阶段

- `clipGridCell`假定每个hatch是矩形（`Quad`+`rotation`），不支持任意多边形；Building场景出现非矩形hatch时再扩展。
- **贴图数组构建依赖源贴图未压缩**：`LoadRawBGRA8`在编辑器下读`Source`原始字节，要求未压缩BGRA8；打包版读`PlatformData`首个mip，同样要求未压缩。
  正式打包前若源贴图被压缩，对应槽位会整层填后备色，需要把这几张贴图的压缩设置改成未压缩。分辨率不是硬性要求：按源贴图实际尺寸最近邻重采样到`DIFFUSE_TEX_SIZE`(2048)方形。
- **近远接缝的软边差异**：`nearControlLevel`内是近处核的距离加权软过渡（约1.4个element宽），其外是远处2×2双线性的较硬过渡（1个element宽），
  只在这条边界线附近、地形交界恰好经过时能看出，未彻底解决。

## 隧道口端墙（2026-11-20）
`BuildTunnelWalls`对`startWall/endWall`为真的端头各造一块端墙板(横向盖满洞宽并比侧壁外侧再多一个厚度，沿长度向洞外厚`TUNNEL_WALL_THICKNESS`)：墙顶=端头线段(含外侧)范围内`SampleHeight`最大值，墙底=`startWallBottom/endWallBottom`，墙顶不高于墙底则不造。内面朝洞内可碰撞。

端墙是完整长方体(内面/外面/顶面/两个侧面/底面)，墙顶突出地面时侧面和顶面都能看到。
