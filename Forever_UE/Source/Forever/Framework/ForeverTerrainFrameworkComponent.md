# ForeverTerrainFrameworkComponent

## 职责

对应旧Framework Actor `Terrain`（C++ Base: `TerrainBase`）——阶段4-1第一个从
`ForeverFrameworkComponent.md`共用文档独立出来的域组件（该文档待办里已经写明这个流程）。
负责动态LOD（clipmap式）地形网格 + 海面网格的生成与随玩家位置更新，对照老工程
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
  3x3晋升规则一次性做完，原来拆成`InitTerrains`+`InitContents`两个函数，应用户要求合并回
  一个，见`Source/Core/map/map.md`）跑完地形生成后，再调用`GenerateTerrain(map)`把指针交给
  这个组件，组件只是非持有地引用它。
- **要求#2"挖洞不用ISM"的落地方式（经过多轮修正，这里记录最终版本）**：老工程`BuildLevel`对
  `levelIdx<=1`（近处精细LOD）会算出`constructionRegion[4][4]`——某个8x8-quad子区域**全部**
  元素都是`"construction"`类型时，整块跳过不画，视觉空缺交给Blueprint端的ISM小立方体填。这次
  不再有Blueprint/ISM消费方，改用`LookupTerrain`（矩形减矩形分解算法，原样搬
  `ATerrainBase::LookupTerrain`，只是不再是`UFUNCTION(BlueprintCallable)`）算出每个
  construction/挖洞Element的精确几何。
  - **前提：所有挖洞都发生在平地上**——construction Element不需要更细的LOD细分。第一版实现
    仍然沿用主网格的32x32 sub-quad粒度，对每个sub-quad测试"中心是否落在`LookupTerrain`返回的
    矩形内"来决定要不要画——这个近似测试量出来的洞边界只能精确到sub-quad网格的粒度（远小于
    真实矩形边界，一般不对齐），PIE验证发现洞的边缘有明显的格子锯齿。**现在改成直接按Element
    粒度处理**：sub-quad循环遇到construction/有hatch的Element时只负责触发`lookupCached`缓存
    并跳过常规两三角形画法（不再测试中心点），随后统一按`LookupTerrain`返回的`rects`（没有
    hatch命中时就是完整一格，画成2个三角形；有hatch命中时是精确的轴对齐矩形集合）直接建出
    **精确坐标**的geometry，和sub-quad网格粒度无关——这正是"先挖出平行于坐标轴的外接矩形洞，
    再补三角形填gap"这套老工程ISM思路的正确落地方式：`rects`本身就是那个轴对齐外接矩形集合，
    `tris`（角落补丁三角形，只有旋转hatch跨格时才非空）就是"四个三角形填gap"的部分。
  - 没有hatch命中的construction Element现在只贡献2个三角形（1个完整矩形），比第一版的64个
    sub-quad三角形省了大量无意义的细分——纯平地上这些细分视觉完全等价，白白增加顶点/三角形
    数量。
  - 环绕顺序：`rects`产出的矩形quad用(左下,右下,右上,左上)记顶点，三角形顺序
    `(v0,v2,v1)`+`(v0,v3,v2)`，和本函数主网格`(v00,v11,v10)`/`(v00,v01,v11)`是同一套已验证
    过的写法，直接照抄没有另外推导。
  **这条路径最初（Terrain阶段）是死代码，Roadnet隧道落地后已经有真实数据了**——Terrain阶段
  验证时`Map::GetHatches`永远返回空列表，construction格子渲染出来是完整实心地面，和plain
  视觉上没区别；Roadnet阶段实现隧道时，`RoadnetMod::AddHatch`会给隧道口对应的格子记一个
  hatch，`Map::InitRoadnet()`把这些hatch转发进`Map::AddHatch`，这条挖洞路径这才第一次真正
  跑出非空的`rects`/`tris`。
  **挖洞判断条件已经从"只认`construction`"放宽成"`construction`或者这个格子有hatch"**
  （`LookupTerrain`开头的提前返回、`BuildLevel`里调用`lookupCached`前的判断，两处都要一起
  改，只改一处会不一致）——隧道口所在的格子地形类型是`"mountain"`（或紧邻的`"plain"`），
  从来不会被判定成`"construction"`（3x3邻域全plain/construction才晋升，隧道口紧邻`mountain`
  格子，邻域条件必然不满足），如果挖洞逻辑还锁死在只认`"construction"`，隧道口的hatch会被
  白白忽略、隧道段被山体实心地形完全挡住看不见——这正是PIE验证时发现的问题，倒推出这个放宽。
  放宽之后不影响原本construction格子的挖洞行为（`construction`这个条件本身没变，只是多了一个
  "或"）。
- **删掉了老工程整套ISM流式窗口机制**——`terrainInstances`/`idList`/`SetInstance`/
  `RemoveInstance`/`UpdateTerrain`（`BlueprintImplementableEvent`）、13x13 Element的加载
  窗口计算，这些全部是Blueprint端ISM cube生成/销毁用的簿记，ISM本身不要了就整套删掉，不留
  痕迹。LOD网格本身的重建节奏（`needsRebuild[]`按pivot变化触发）已经足够让construction格子
  的挖洞几何跟着LOD重建自动刷新，不需要额外的流式窗口驱动。
- **玩家位置改用`UGameplayStatics::GetPlayerPawn(GetWorld(), 0)`**，不是老工程的
  `global->GetLocation()`——Global域组件要到阶段4-8才会有实际逻辑，Terrain作为Map域第一个
  落地的concept不能依赖它，这也是要求#1"Terrain不依赖其它concept"的具体体现。取的是本地
  玩家0号，和项目现状（无分屏/多人支持）一致。
- **材质参数名严格照抄老工程字符串**：`terrainMaterial`用`TerrainDiffuseArray`/
  `TerrainIndexMap`；`fineMaterial0/1/2`用`FineIndexMap`/`FinePowerMap`/
  `TerrainDiffuseArray`。这些名字要和复制过来的`TerrainTemplate.uasset`/`FineTemplate.uasset`
  材质图里的参数名对上才会生效——`SetTextureParameterValue`设错名字不报错只是静默不生效，
  PIE里看贴图是否正确显示是唯一的验证手段。
- **固定的基础材质走`ConstructorHelpers::FObjectFinder`（构造函数里加载，`EditDefaultsOnly`
  可覆盖），data-driven的每个地形Mod实例的`diffusePath`走运行时`LoadObject<UTexture2D>`**——
  前者是编译期已知的路径（`TerrainTemplate`/`FineTemplate`/Water插件材质），后者是运行时
  才知道具体是哪个Mod、路径是字符串（`TerrainMod::diffusePath`）。不迁移老工程的
  `UAssetLoader::LoadAssetFromPath`工具类，两种场景各自用引擎自带的对应机制就够。
- **`FRect2D`/`FTri2D`/`FFineCell`/`FForeverLodBoundary`都是普通C++结构体，不是`USTRUCT`**——
  老工程里`FRect2D`/`FTri2D`是`USTRUCT(BlueprintType)`（供Blueprint端ISM消费），这次没有
  Blueprint消费方了，不需要UHT反射。`FForeverLodBoundary`加`Forever`前缀是为了和老工程同名
  的`LodBoundary`区分（纯风格考虑，两边不会同时编译，不存在真实冲突）。

## 依赖关系

- 依赖：`Source/Core/map/map.h`（`Map`，非拥有指针）、UE的`ProceduralMeshComponent`/`Water`
  插件模块（见`Forever.Build.cs`）、`Content/Asset/Materials/{TerrainTemplate,FineTemplate}
  .uasset`、`Content/Asset/Textures/Terrain/*`（从老工程原样复制的美术资产）。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`
  （`CreateDefaultSubobject`创建、`EnsureTerrainGenerated()`里调用`GenerateTerrain`）、
  `Source/Forever/Player/ForeverGameMode.cpp`（`GetMapCenterWorldLocation()`算出生点）。

## 待办/后续阶段

- 阶段4（Building）：一旦Building也开始调用`Map::AddHatch`，construction格子的挖洞会自动
  跟着生效，不需要改这个组件的代码。角落三角形（`tris`）目前只有Roadnet隧道口这一个真实数据
  来源验证过，PIE看一下绕序/UV是否符合预期，如果Building场景下发现问题再回来微调。
- 阶段4：`TerrainTemplate`/`FineTemplate`材质图内部除了这几个已知贴图参数外是否还引用了别的
  纹理资产（比如法线/粗糙度贴图），只能在编辑器里打开材质图确认，如果发现引用了没有复制过来
  的资产需要补充复制。
