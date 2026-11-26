# ForeverTerrainFrameworkComponent

## 职责

负责动态LOD（clipmap式）地形网格、
隧道口侧壁mesh的生成，以及随玩家位置更新LOD；同时用`FTerrainMaterialBuilder`生成唯一的地形材质实例(见"地形材质"一节)。

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
- **小格拆三角形的对角线自适应**（`BuildLevel`里的`anti`）：和`Map::SampleHeight`的粗高度插值同一个规则（取两端高度差较小的对角线），斜着走的地形交界才会是笔直的斜坡而不是锯齿。L0~L4的小格不跨element：没有细化数组的element直接问`Map::UsesAntiDiagonal`（网格和采样严格同一对角线）；有细化数组的element和更粗的LOD（小格跨多个element）看这个小格4个顶点自己的高度。`clipGridCell`（hatch裁剪）按同一个`anti`选两个三角形，construction整格替换不受影响（平地）。
- **法线放在顶点上、从高度场求**：`BuildLevel`里每个顶点的法线=`SampleHeight`在`NORMAL_SAMPLE_STEP`(0.25 element，各级LOD一致)步长下的中心差分梯度，不再用相邻三角形面法线累加。法线只取决于位置，所以对角线怎么拆、construction整格替换/hatch裁剪交点这些不共享的独立顶点、各级LOD之间，同一位置的法线完全相同，GPU在三角形内插值后连续变化，三角形边界不会出棱角。代价是整数格点处的陡坎被抹开约0.25 element。水面/隧道墙的法线逻辑没动。
- **`UActorComponent`不是Actor**：运行时创建的子组件要挂在`GetOwner()`上
  （`NewObject<...>(owner)`+`SetupAttachment(owner->GetRootComponent())`+`owner->AddInstanceComponent`），不是`this`。
- **`Map`的生命周期不归本组件管**：`AForeverFrameworkActor`持有`Map*`，`Map::InitTerrains()`跑完后调`GenerateTerrain(map)`，
  组件只是非持有地引用它。玩家位置取`GetPlayerPawn(0)`（Terrain不依赖其它concept，项目无分屏）。
- **`IsInsideLodLevel(worldPos, level, scale)`**：判断某点是否在第level级LOD当前覆盖的正方形内（scale<1时按中心缩小，做进入/离开滞回），
  供`ForeverTrafficFrameworkComponent`的公交远近（物理/非物理）切换使用。
- **材质**：所有LOD共用一个`terrainMaterial`(MID，基础材质`/Game/Asset/Terrain/M_Terrain`，`GenerateTerrain`里运行时`LoadObject`——不能在构造函数里FObjectFinder，
  否则编辑器里跑`Tools/TerrainMaterial/build_terrain_material.py`重建材质时它已被CDO引用着，清节点会断言)。侧壁/隧道墙用`Pure`单色材质(`ConstructorHelpers`)。
- `FForeverLodBoundary`是普通C++结构体，不是`USTRUCT`（没有Blueprint消费方）。

### 挖洞（construction格子与hatch）

挖洞在网格生成时用精确凸多边形裁剪完成，不用ISM小立方体。`Map::AddHatch`登记的hatch是旋转矩形（`Quad`+`rotation`），
挖洞条件是"该格子有hatch"(不论地形类型)。隧道口所在格子多半是`mountain`/`plain`，不一定被标成可建设(construction)，
如果只认`construction`，隧道口hatch会被忽略、隧道被山体实心地形挡住。`RoadnetMod::AddHatch`记hatch，`Map::InitRoadnet()`转发进`Map::AddHatch`。

**退化碎片过滤**：裁剪会在"顶点正好落在hatch边的直线上"或"相邻两个hatch共边"(例如twin布局进深取满4时，bg+的6个hatch两两首尾相接)时切出重复点/共线点的零面积碎片。碎片`Num()>=3`却面积为0，建成三角形就是退化三角形，Chaos给地形建碰撞体时会报`Input trimesh contains N bad triangles`。所以收下外侧碎片前用`PolygonAreaAbs`过滤：clipGridCell在世界坐标里小于`kMinWorldPieceArea`(10cm²，世界坐标几十万量级时float精度0.03~0.06产生的噪声碎片就在这个量级)丢掉。只按整块面积过滤不够：碎片里有重复点/共线点时整块面积为正，但扇形三角化里会混进零面积三角形，所以扇形输出时还要逐个三角形判面积(`kMinWorldTriArea`=1cm²)，太小的跳过，交点顶点也只在真正用到时才追加。

**一条路径（2026-11-20起）→`BuildLevel`里的`clipGridCell`**（`levelIdx<=TERRAIN_HATCH_MAX_LEVEL`=4(2026-11-23从3改成4，hatch显示距离翻倍)，L0~L4的网格小格都不跨element，L5起小格跨element不能直接用）：
有hatch的格子(不论地形类型，construction也一样)，只对这个网格小格的两个三角形，按该element全部hatch的旋转矩形做4边半平面裁剪
（`FClipPoint`/`SplitClipPolygonByLine`，Sutherland-Hodgman单边裁剪的双输出版本，保持原绕序，顶点带世界坐标和网格顶点下标）：
落在某条边外侧的确定在矩形外，收进`survivingPieces`；4条边都在内侧的才是洞，丢弃；每个格子独立裁剪，不依赖"hatch完全落在单一格子内"。
没被切到的小格仍是共享网格顶点的普通两个三角形，完全在洞里的丢弃，被切到的保留洞外部分——角点复用网格顶点（无裂缝、法线连续，包含格子里的细化高度），
只有矩形边上的交点追加新顶点，高度是原三角形平面上的3D线性插值（落在原渲染表面上，含LOD接缝修正）。绕序和主网格同一套(`(p0,p[i+1],p[i])`)。
- **`construction`格子(平地)和别的地形一样走稠密网格**，不做"一格一个正方形面片"的简化：简化后的格子边只有四角高度，和邻格的细分顶点对不上，相邻格子之间会出现缝隙(不要再这样做)；hatch由`clipGridCell`在稠密网格上裁剪。代价是近处construction区域的三角形数多一些。
- **不要改回"轴对齐外接矩形减矩形+角落补丁三角形"的近似**：它假定hatch整个落在单一格子内，而隧道口hatch（一段Connection的长度，1.5~2.5地图单位）经常跨2~3个格子，
  会挖出AABB粗糙洞、缺角落垫平，玩家沿路走进隧道口会因缺失的地形碰撞穿模掉下去。

起伏地形上的"路"不靠挖洞：路网通过`RoadnetMod::AddTerrainFit`让地形高度贴到路面高度（`Map::ApplyTerrainFits`直接改`fineHeights`，见`Core/map/map.md`"地形贴合"），
UE这边照常读`SampleHeight`。hatch只用来挖洞。

### 隧道口侧壁（`BuildTunnelWalls`）

地形是单层面片，洞沿没有东西，从侧面/低角度能透过洞沿看到地形背面（被剔除）。`TerrainHatch::wall=true`的hatch
（只有`RoadnetMod::AddTunnelHatch`产出的隧道口）由`BuildTunnelWalls()`在`GenerateTerrain`末尾一次性生成侧壁mesh（`tunnelWallMesh`，和LOD无关），
数据来源`Map::GetTunnelWallHatches()`（取自`Roadnet::hatches`，读档后也在）：

- 洞的两条长边（沿`rotation`，横向±sizeY/2）各一条竖直墙，**不加两个端头**（起点端是和地面齐平的路面入口，终点端是隧道本身，隧道里的墙和顶棚用资产）。
- 沿长度每`TUNNEL_WALL_SEGMENT`(0.25)一个断面；墙顶不低于所在地形(2026-11-20改：墙顶点取前后各半个断面间距内、墙内外两侧`SampleHeight`的最大值，可略突出地面但不留空缺，和端墙一致；原先是`SampleHeight`−5cm，已删`TUNNEL_WALL_TOP_INSET`；2026-11-20再加`TUNNEL_WALL_TOP_MARGIN`=0.005(5cm)余量，侧墙和端墙都加)；
  墙底=洞底（`floorStart→floorEnd`的smoothstep，和S形下坡曲线两端水平一致）−`TUNNEL_WALL_BOTTOM_EXTRA`(埋在路面下)；地形不比洞底高的断面跳过。
- 做成有厚度（`TUNNEL_WALL_THICKNESS`）的小板：朝洞内的内面+朝洞外的外面+顶面，三角形绕序按期望法线校正，`CreateMeshSection`带碰撞。
- 材质用单色`Pure`（`tunnelWallBaseMaterial`，Color参数）：地形材质按世界XY投影采样，竖直面上是拉伸的竖条纹。

## 地形材质

一张材质`M_Terrain`负责所有LOD、所有地形(没有Near/Far两套材质，也没有CPU生成的控制纹理，没有兜底)。
材质**不读网格UV**，全部用世界坐标(`AbsoluteWorldPosition/1000`=element坐标)；`CreateMeshSection`不传UV。数据侧在`TerrainMaterialBuilder.h/.cpp`，材质图是`Tools/TerrainMaterial/terrain_material.hlsl`(单个Custom节点)，生成脚本见`Tools/TerrainMaterial/README.md`。

**材质声明在各地形mod里**：`TerrainMod::SetupTexture()`里调`SetMaterial(...)`声明1~2层(UE贴图资产路径、平铺米数、是否三向投影)和选择器(`None`/`Slope`坡度/`WaterDepth`水深)及范围；
默认地形`plain`(没有mod)在`Map::GetTerrainAppearances()`里只占一个条目，不声明材质。**没声明材质的地形(`plain`、没调`SetMaterial`的mod地形)统一用兜底材质**：`TerrainMaterialBuilder.cpp`的`FallbackTerrainMaterial()`，贴图是`/Game/Asset/Terrain/Plain`下的三张，路径只写在UE层。`Map::GetTerrainAppearances()`向已注册mod收集后缓存(新世界和读档后都可用)，Builder只读它。
新增一个地形mod=在它自己的`SetupTexture`里写`SetMaterial`，不用改任何中央配置。声明不合法/贴图读不到的地形渲染成纯灰并`UE_LOG(Error)`(`LogTerrainMaterial`)；没声明的走上面的兜底材质，只打一条Log。

**`FTerrainMaterialBuilder::Build`(`GenerateTerrain`里一次性，运行中不再重建，headless约2.7秒)**：
1. 扫描地图实际出现的地形id(最多`MaxTerrainTypes`=8，超出打Error)，按id排序编号；层按`name`去重编号。
2. **层贴图数组**`TerrainDiffuseArray`(BC7 sRGB)/`TerrainNormalArray`(BC5，RG=法线xy，DirectX约定)/`TerrainMaskArray`(BC7线性，ORM：R=AO、G=粗糙度)，1024²，11级mip。
   **数据是cook后的压缩块，原样拷进数组切片**(`ReadCompressedMips`经`UTexture2D::GetMipData`取每级mip的原始字节，编辑器走DDC、打包版走资产内联的bulk data，同一条路径；不碰像素，所以打包版也能用)。
   前提由`Tools/TerrainMaterial/prepare_terrain_textures.py`保证：层贴图必须是1024最大尺寸、不流送、漫反射BC7/法线BC5/遮罩BC7(法线的`flip_green_channel`在cook时生效，Megascans的ORM是R=AO、G=粗糙度、B=金属度)。
   格式/尺寸/mip数不符时对应层打Error(提示去跑脚本)、渲染成灰，不退回任何贴图。单独的粗糙度图(Plain)当遮罩用，层表里标志"无AO"(AO=1)。各工程的`pak.bat`cook前自动跑这个脚本。
3. **覆盖图**`TerrainCoverageArray`：`Core/map/terrain_coverage`把每格的one-hot地形展开成2 texel/element，做σ=1 element的高斯模糊，4种地形一个RGBA8切片，带mip，clamp。模糊把阶梯边界(含45°斜线)磨成圆滑曲线。
4. **类型表**`TerrainTypeTable`(2×8，RGBA32F：layerA,layerB,selector,0 / range0,range1)和**层表**`TerrainLayerTable`(1×层数：tile cm,三向标志,遮罩有无AO)；**噪声**`TerrainNoise`(256²四通道可平铺值噪声，固定哈希)。
5. MID参数：纹理同名，`TerrainMapInfo`=(宽,高,种类数,覆盖切片数)，`TerrainWaterEncoding`=(水深编码min,range)。调参向量`TerrainTuning0/1/2`的默认值在材质里(见Tools README)。

**顶点色R=水深**：`GenerateTerrain`用`BuildWaterLevelField`算每格"最近水面高度"(有水的格取自己，其余4邻域向外扩6格取最近，更远=无水)，`BuildLevel`按顶点位置算
`(水位-地面高度)*10`米，编码到[-4,20]米的8 bit(精度约0.09米，`SetMaterial`的水深范围不能超过上限)写进顶点色R(无水=最小值=干)。位置决定值，与LOD/三角形怎么拆无关，hatch裁剪新增的交点顶点也一样。

**着色器(`terrain_material.hlsl`)**：覆盖图用噪声扭曲采样位置+每种地形叠一点噪声再"锐化"(只留离最大权重一个带宽以内的种类)，格内大多数像素只剩1~2种；
每种地形的两层按选择器混合(坡度=`1-顶点法线z`，水深=顶点色解码)，只采样权重非0的层；陡峭层(rock_cliff)近处用三向投影(Golus whiteout)；
距离分档全部连续过渡：≥`TerrainTuning1.x..y`米渐变到"平均色"(采样数组最低mip=1x1，只2次采样，不做细节采样；压缩贴图CPU读不出像素所以不预先算平均色)，细节法线`z..w`米淡出，中距离起丢次要地形。材质输出世界空间法线(`tangent_space_normal=False`)。
所有`SampleGrad`的导数在分支外算好，故可以放进动态分支。

## 水面

水面已移到`ForeverWaterFrameworkComponent`（分级细分+波浪+水下后处理，见其.md），本组件不再生成水面mesh，只向它提供LOD pivot。

## 依赖关系

- 依赖：`Source/Core/map/map.h`（`Map`，非拥有指针；`GetTerrainAppearances`、`GetWater`）、`Source/Core/map/terrain_coverage.h`（覆盖图烘焙、mip降采样，纯标准库）、
  UE的`ProceduralMeshComponent`/`Water`插件模块（见`Forever.Build.cs`）、`Content/Asset/Terrain/M_Terrain.uasset`(脚本生成)、
  各地形mod声明的贴图(`Content/3rdParty/Megascans/Surfaces/*`、`Content/Asset/Terrain/Plain/*`)。
- 被谁依赖：`AForeverFrameworkActor`（`CreateDefaultSubobject`创建、`GenerateTerrainMesh()`里调用`GenerateTerrain`）、
  `AForeverPlayerGameMode`（`GetMapCenterWorldLocation()`算出生点）、`UForeverTrafficFrameworkComponent`（`IsInsideLodLevel`）。

## 补充说明
- **打包版**：层贴图靠cook后的压缩块拷贝，需要`prepare_terrain_textures.py`先处理(已写进`Forever_UE/pak.bat`和`Forever_Mod/{Test,Yuanshen}/UE/*/pak.bat`；mod把地形层贴图放在自己插件的`TerrainLayers/`目录下，pak.bat会自动处理)。
  编辑器/PIE不经pak.bat：`PrepareTextureInEditor`在加载层贴图时按同样规则在内存里修正属性(不存盘不标脏，打Warning带改前属性，首次重新压缩几秒)；规则和脚本各一份，改一边同步另一边。
  mod工程是它自己的迷你UE工程，没有Forever的C++，所以这一步只用引擎Python设置贴图属性，运行时合并由主程序完成。
- 构建在游戏线程里同步跑(约3~6秒，HitchWatchdog会报一次)，只发生在加载阶段。层贴图数组是压缩的：8层×3张×1024²含mip约33MB显存，覆盖图约43MB。

## 隧道口端墙（2026-11-20）
`BuildTunnelWalls`对`startWall/endWall`为真的端头各造一块端墙板(横向盖满洞宽并比侧壁外侧再多一个厚度，沿长度向洞外厚`TUNNEL_WALL_THICKNESS`)：墙顶=端头线段(含外侧)范围内`SampleHeight`最大值+`TUNNEL_WALL_TOP_MARGIN`(5cm)，墙底=`startWallBottom/endWallBottom`，墙顶不高于墙底则不造。内面朝洞内可碰撞。

端墙是完整长方体(内面/外面/顶面/两个侧面/底面)，墙顶突出地面时侧面和顶面都能看到。
