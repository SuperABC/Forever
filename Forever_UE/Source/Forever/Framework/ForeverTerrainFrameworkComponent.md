# ForeverTerrainFrameworkComponent

## 职责

对应旧Framework Actor `Terrain`（C++ Base: `TerrainBase`）。负责动态LOD（clipmap式）
地形网格 + 海面网格的生成与随玩家位置更新，对照老工程
`E:\Projects\Forever_UE\Source\Forever\Base\TerrainBase.h/.cpp`迁移。

## 关键设计

- **`UActorComponent`不是Actor，运行时创建的子组件要挂在`GetOwner()`上，不是`this`**——老
  工程`ATerrainBase`自己就是Actor，`NewObject<UProceduralMeshComponent>(this, ...)`+
  `SetupAttachment(RootComponent)`+`AddInstanceComponent(comp)`里的`this`/`RootComponent`
  都是自己。这里全部换成`GetOwner()`/`GetOwner()->GetRootComponent()`/
  `GetOwner()->AddInstanceComponent(comp)`——这是相对老工程唯一的机械性改动点，LOD/接缝/
  纹理数组构建等算法逻辑本身原样搬。
- **`Map`的生命周期不归这个组件管**——`AForeverFrameworkActor`持有`Map*`（见
  `ForeverFrameworkActor.md`），`BeginPlay`时调用`Map::InitTerrains()`（注册mod+跑地形生成+
  3x3晋升规则一次性做完，见`Source/Core/map/map.md`）跑完地形生成后，再调用`GenerateTerrain(map)`把指针交给
  这个组件，组件只是非持有地引用它。
- **挖洞不用ISM**：老工程`BuildLevel`对`levelIdx<=1`（近处精细LOD）会算出
  `constructionRegion[4][4]`——某个8x8-quad子区域**全部**元素都是`"construction"`类型时，
  整块跳过不画，视觉空缺交给Blueprint端的ISM小立方体填。这里没有Blueprint/ISM消费方，改用
  `LookupTerrain`算出每个construction/挖洞Element的精确几何，按Element粒度处理：sub-quad
  循环遇到construction/有hatch的Element时触发`lookupCached`缓存并跳过常规两三角形画法，
  随后统一按`LookupTerrain`返回的三角形列表直接建出**精确坐标**的geometry，和sub-quad
  网格粒度无关（前提：所有挖洞都发生在平地上，construction Element不需要更细的LOD细分）。
  没有hatch命中的construction Element只贡献2个三角形（1个完整矩形）。
  - **`LookupTerrain`的挖洞算法是精确凸多边形裁剪，不是"外接矩形减矩形+角落补丁三角形"的
    近似算法**——后者（每个hatch先按旋转矩形算轴对齐外接矩形(AABB)，对格子做"T形分割"挖洞，
    再在"hatch中心所在的那一个格子"里补4个角落三角形垫平AABB和真实旋转矩形之间的缝隙）隐含
    一个前提：hatch整个矩形必须完全落在单一格子内。**不要用这种近似**——隧道口的hatch(道路
    一段Connection的长度，1.5~2.5地图单位)经常比1个地形格子(1地图单位)长，会跨越2~3个格子，
    这种情况下"角落三角形只在hatch中心所在格子补"不成立，hatch经过的其它格子只挖出AABB这个
    粗糙矩形洞、没有角落三角形垫平多挖出来的那一圈本该是实心的地形，玩家沿路走进隧道口会
    因为这圈缺失的地形碰撞直接穿模掉下去。
    - **现在的算法**：每个格子独立地、精确地按hatch矩形自己的4条边(半平面)做凸多边形裁剪——
      `workPolys`(这个格子里"还没被判定为实心、还需要继续跟下一个hatch比对"的剩余区域，可能
      不止一块)初始是整个格子`[0,1]x[0,1]`。对每个hatch依次用它的4条边裁剪`workPolys`里的
      每一块：落在某条边"外侧"的部分直接确定在hatch矩形外面，收进下一轮`survivingPieces`；
      "内侧"的部分继续拿下一条边判断。4条边都判断完还留在"内侧"的部分，就是被这个hatch矩形
      整个挖穿的洞，丢弃。不管hatch跨了几个格子，每个格子都独立精确裁剪，互相之间不需要协调，
      天然不依赖"hatch完全落在单一格子内"这个前提。
    - 所有hatch处理完之后，`workPolys`剩下的就是这个格子里真正的实心地形区域(可能是好几块
      互不相连的多边形)，扇形三角化(`FanTriangulate`)成最终输出的`FTerrainTri2D`列表——
      没有hatch命中时`workPolys`就是初始的整格方块，输出2个三角形。
    - `FTerrainTri2D{A,B,C}`（任意三角形的3个顶点）是唯一的三角形结构体；
      `SplitConvexPolygonByLine`(Sutherland-Hodgman单边裁剪的双输出版本，同时产出"在裁剪边
      内侧"/"在裁剪边外侧"两部分，各自保持原多边形的绕序不变)和`FanTriangulate`是裁剪用的
      两个匿名namespace辅助函数。
    - 绕序：`workPolys`的初始方块用(左下,右下,右上,左上)CCW记顶点，和本函数主网格quad
      `(v00,v11,v10)`/`(v00,v01,v11)`同一套"CCW四边形要用`(v0,v2,v1)`+`(v0,v3,v2)`朴素fan
      对调后两个顶点才能让法线朝上"的约定——`SplitConvexPolygonByLine`按遍历顺序插入交点，
      天然保持输入多边形的绕序不变，`FanTriangulate`对任意点数的凸多边形套用同一个"朴素fan、
      后两个顶点对调"的规则(`(v0,v[i+1],v[i])`)，消费方(`BuildLevel`)按`FTerrainTri2D`
      存好的`(A,B,C)`顺序直接建三角形，不用再另外对调。
  **挖洞判断条件是"`construction`或者这个格子有hatch"**（`LookupTerrain`开头的提前返回、
  `BuildLevel`里调用`lookupCached`前的判断，两处必须保持一致）——隧道口所在的格子地形类型
  是`"mountain"`（或紧邻的`"plain"`），从来不会被判定成`"construction"`（3x3邻域全plain/
  construction才晋升，隧道口紧邻`mountain`格子，邻域条件必然不满足），如果挖洞逻辑锁死在
  只认`"construction"`，隧道口的hatch会被忽略、隧道段被山体实心地形完全挡住看不见。
  `RoadnetMod::AddHatch`给隧道口对应的格子记一个hatch，`Map::InitRoadnet()`把这些hatch
  转发进`Map::AddHatch`，这条挖洞路径靠这份数据跑出非空的`rects`/`tris`。
- **删掉了老工程整套ISM流式窗口机制**——`terrainInstances`/`idList`/`SetInstance`/
  `RemoveInstance`/`UpdateTerrain`（`BlueprintImplementableEvent`）、13x13 Element的加载
  窗口计算，这些全部是Blueprint端ISM cube生成/销毁用的簿记，ISM本身不要了就整套删掉，不留
  痕迹。LOD网格本身的重建节奏（`needsRebuild[]`按pivot变化触发）已经足够让construction格子
  的挖洞几何跟着LOD重建自动刷新，不需要额外的流式窗口驱动。
- **玩家位置改用`UGameplayStatics::GetPlayerPawn(GetWorld(), 0)`**，不是老工程的
  `global->GetLocation()`——Terrain不依赖其它concept，不能反过来依赖一个可能还没初始化的
  域。取的是本地玩家0号，和项目现状（无分屏/多人支持）一致。
- **材质参数名**：`terrainMaterial`（远处，`TemplateFar`）用`TerrainDiffuseArray`/
  `TerrainNormalRoughArray`/`TerrainIndexMap`/`TerrainMapSize`；`nearMaterial`（近处，
  `TemplateNear`）用`TerrainDiffuseArray`/`TerrainNormalRoughArray`/`NearIndexMap`/
  `NearWeightMap`/`NearRegion`。这些名字要和材质图里的参数名对上才会生效——
  `SetTextureParameterValue`/`SetVectorParameterValue`设错名字不报错只是静默不生效，
  PIE里看贴图是否正确显示是唯一的验证手段。
- **固定的基础材质走`ConstructorHelpers::FObjectFinder`（构造函数里加载，`EditDefaultsOnly`
  可覆盖），data-driven的每个地形Mod实例的`diffusePath`/`normalPath`/`roughnessPath`走
  运行时`LoadObject<UTexture2D>`**——前者是编译期已知的路径（`TemplateFar`/`TemplateNear`/
  Water插件材质），后者是运行时才知道具体是哪个Mod、路径是字符串
  （`TerrainMod::diffusePath`等）。不迁移老工程的`UAssetLoader::LoadAssetFromPath`工具类，
  两种场景各自用引擎自带的对应机制就够。
- **`FTerrainTri2D`/`FForeverLodBoundary`都是普通C++结构体，不是`USTRUCT`**——没有
  Blueprint消费方，不需要UHT反射。`FForeverLodBoundary`加`Forever`前缀是为了和老工程
  同名的`LodBoundary`区分（纯风格考虑，两边不会同时编译，不存在真实冲突）。

## 近处地形混合

近处（细LOD）材质重做，替换掉老工程照抄过来的"ISM cube矩阵、每个cube固定UV"那一套——
新工程近处地形早就改成了和远处一样的程序化网格，UV含义变了，材质没跟着改，之前L0~L2
主网格用归一化`[vx/32,vy/32]`UV、L3+用地图坐标UV，而construction/挖洞三角形（只在
`levelIdx<=1`时生成）始终用地图坐标UV，和L0/L1同一层主网格的归一化UV混在同一个mesh
section、同一个材质里——这些三角形会用几百上千的UV值去采样32×32的控制图，采到回绕后的
随机内容，是一个确定性bug。

- **材质不读网格UV，全部用世界坐标**：`AbsoluteWorldPosition/1000`就是地图element坐标
  `P`。`BuildLevel`不再给顶点算UV（`CreateMeshSection`的UV参数直接传空数组），彻底消除
  上面那个bug，而不是修补它——没有UV数据，就不存在"两种UV混用"的可能。
- **控制纹理区域=`nearControlLevel`那一级LOD的覆盖区域**（`EditDefaultsOnly`，默认2，即
  16个element=160米）：`NearIndexMap`/`NearWeightMap`两张持久纹理（`GenerateTerrain`里
  创建一次，`RebuildNearControl`原地更新，不是每次`CreateTransient`），分辨率
  `size×nearTexelsPerElement`（默认16个texel/element）。内容只取决于地图数据和texel的
  世界位置，和区域怎么划分无关，区域平移/重建时同一个世界点的颜色不会跳变——算法细节见
  `Source/Core/map/terrain_blend.md`，UE侧只是把`NearTerrainBlend::BuildRegion`的输出
  字节数组拷进纹理。
- **只在`nearControlLevel`自己的pivot变化时重建纹理**：`TickComponent`的从粗到细重建
  循环里，`i==nearControlLevel`那一次迭代会在调用`BuildLevel`之前先调
  `RebuildNearControl`——粗一级pivot变化时所有更细级必然也要重建（`boundaryChanged`的
  传播已经保证），所以纹理和网格总是同一帧一起更新，不会有"网格已经移动但控制纹理还没
  跟上"的一帧。`RebuildNearControl`内部还有一道`nearRegionPivot`比较，真正没变化时
  直接跳过，不会被外层循环"这一帧要重建"的粗粒度判断误触发。
- **`levelIdx<=nearControlLevel`的所有LOD共用同一个`nearMaterial`**——不再是L0/L1/L2各自
  一份`fineMaterial0/1/2`+各自一对32×32控制图。`nearSliceIds`/`nearIsConstruction`（整张
  地图，`GenerateTerrain`里和`terrainIndexMap`同一次扫描顺手填好）是`NearTerrainBlend::
  BuildRegion`的输入，不用每次重建纹理时重新扫全图。
- **法线+粗糙度**：新增`TerrainNormalRoughArray`（BGRA8，sRGB关，和`TerrainDiffuseArray`
  槡位对齐），每个槡位把法线源贴图的R/G（法线xy）和粗糙度源贴图的R（灰度）打包进
  B/G/R三个输出通道，A固定255。近/远两个材质都只采样"权重最高的那个地形"的法线粗糙度
  （近处是槡位0，远处是四个角里总权重最高的那个id），省采样次数，代价是交界处细节会
  借助一个基于主导权重的淡入系数自然淡成平的，不会出现硬线。源贴图路径为空/加载失败/
  分辨率不是2048×2048时，该槡位整层填平的法线`(128,128)`+中性粗糙度`204`（约0.8），同时
  打一条Warning——`TerrainDiffuseArray`原来检查不过就直接留黑不报错，这次顺手给diffuse
  也加上同样的检查+失败时填中性灰的后备行为。
- **远处的2×2双线性**：远处材质`TemplateFar`不新增CPU端数据，继续用现成的一张全图
  `terrainIndexMap`，在着色器里对相邻4个element的id做双线性混合，边界不再是10米一格的
  硬边；`TerrainMapSize`材质参数（`GenerateTerrain`里设成真实地图宽高）供着色器算越界
  时夹到边缘element。

## 依赖关系

- 依赖：`Source/Core/map/map.h`（`Map`，非拥有指针）、
  `Source/Core/map/terrain_blend.h`（`NearTerrainBlend`，近处混合权重算法，纯标准库，
  见`terrain_blend.md`）、UE的`ProceduralMeshComponent`/`Water`插件模块（见
  `Forever.Build.cs`）、`Content/Asset/Materials/{TemplateFar,TemplateNear}.uasset`
  （编辑器里手工搭建的材质图，节点表见仓库根目录的河流/地形相关计划文档）、
  `Content/Asset/Textures/Terrain/*`（diffuse+normal+roughness三套贴图资产）。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`
  （`CreateDefaultSubobject`创建、`EnsureTerrainGenerated()`里调用`GenerateTerrain`）、
  `Source/Forever/Player/ForeverGameMode.cpp`（`GetMapCenterWorldLocation()`算出生点）。

## 待办/后续阶段

- 一旦Building也开始调用`Map::AddHatch`，construction格子的挖洞会自动跟着生效，不需要改
  这个组件的代码。精确凸多边形裁剪算法目前只有Roadnet隧道口这一个真实数据来源验证过（含
  单格/跨格两种情况），当前`LookupTerrain`假定每个hatch都是一个矩形(`Quad`+`rotation`)，
  不支持任意多边形——如果Building场景下出现更复杂的hatch形状（比如非矩形）再回来微调。
- **贴图数组构建依赖源贴图未压缩**：`LoadRawBGRA8`要求源贴图分辨率恰好是
  `DIFFUSE_TEX_SIZE`（2048）且是未压缩BGRA8格式才会被接受，PIE和编辑器里没问题；正式
  打包前源贴图如果被压缩会导致对应槡位整层填成后备色，需要在打包前把这几张贴图的压缩
  设置改成未压缩，或者换成别的构建方式（不在这次范围）。
- **近远接缝的软边差异**：`nearControlLevel`内是近处核的距离加权软过渡（约1.4个element
  宽），`nearControlLevel`外沿往外是远处2×2双线性的硬一些的过渡（1个element宽），只在
  这条边界线附近、地形交界恰好经过那里时能看出差异——不在这次范围内彻底解决，`terrain_
  blend.md`"风险"一节（如果有）或原始计划文档里有更详细的讨论。
