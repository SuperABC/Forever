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
  `ForeverFrameworkActor.md`），`BeginPlay`时调用`Map::InitTerrains()`+`InitContents()`
  跑完地形生成后，再调用`GenerateTerrain(map)`把指针交给这个组件，组件只是非持有地引用它。
- **要求#2"挖洞不用ISM"的落地方式**：老工程`BuildLevel`对`levelIdx<=1`（近处精细LOD）会算出
  `constructionRegion[4][4]`——某个8x8-quad子区域**全部**元素都是`"construction"`类型时，
  整块跳过不画，视觉空缺交给Blueprint端的ISM小立方体填。这次不再有Blueprint/ISM消费方，改成
  **按quad粒度**判断：每个quad算出中心所属的地图`Element`，非construction照常画两个三角形；
  是construction则查（惰性缓存）该Element的`LookupTerrain`结果（矩形减矩形分解算法，原样搬
  `ATerrainBase::LookupTerrain`，只是不再是`UFUNCTION(BlueprintCallable)`），quad中心落在
  剩余矩形范围外才跳过——这就是"洞"。每个涉及到的construction Element还会额外补一次
  `LookupTerrain`返回的角落三角形（`tris`，处理旋转hatch和轴对齐AABB减法留下的缝隙），作为
  独立的、非网格对齐的顶点追加进mesh。
  **这条路径目前实际上是死代码**——因为`Map::GetHatches`永远返回空列表（Roadnet/Building还
  没迁移，没有任何系统调用`Map::AddHatch`），`LookupTerrain`对任何construction格子都只会
  返回"整格一个矩形、tris为空"，所以construction格子现在渲染出来是**完整实心地面**，和
  plain格子视觉上没有区别（贴图也一样，都是`PlainDiffuse`，见`map.md`）。等以后Roadnet/
  Building阶段开始调用`Map::AddHatch`，这里会自动开始产生真正的洞，**不需要再回来改这段代码**。
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

- 阶段4（Roadnet/Building）：一旦这两个系统开始调用`Map::AddHatch`，construction格子的挖洞
  会自动生效，不需要改这个组件的代码，但建议届时补一轮PIE验证确认挖洞的视觉效果符合预期
  （角落三角形的绕序/UV这次没有实际数据可以验证，可能需要微调）。
- 阶段4：`TerrainTemplate`/`FineTemplate`材质图内部除了这几个已知贴图参数外是否还引用了别的
  纹理资产（比如法线/粗糙度贴图），只能在编辑器里打开材质图确认，如果发现引用了没有复制过来
  的资产需要补充复制。
