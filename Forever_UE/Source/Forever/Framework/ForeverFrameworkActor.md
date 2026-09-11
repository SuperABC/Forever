# ForeverFrameworkActor

## 职责
`AForeverFrameworkActor`是场景里唯一的Framework入口Actor,对应`REFACTOR_PLAN.md`阶段2(需求#6)"把9个各自独立的Framework Actor收敛成一个C++驱动的Actor"的目标。内部按域划分为`UForeverFrameworkComponent`子类(见`ForeverFrameworkComponent.md`的对照表),对应旧的Asset/Building/Populace/Roadnet/Room/Story/Terrain/Traffic/Zone这9个Framework Actor中的8个。

**不含Global域组件（阶段4-1移除）**：旧工程的`GlobalBase`本身就是"放在关卡里、串起其它Framework Actor"的那个入口，这个角色现在整个由`AForeverFrameworkActor`自己承担了——不需要再在它内部嵌一个"Global"子组件重复扮演"可放置入口"这件事。`GlobalBase`真正的业务逻辑（`GlobalPause`/`DrawMap`/`InitPhone`等）将来会直接落在这个Actor自己身上，不会有对应的域组件，详见`PHASE4_PLAN.md`阶段4-8一节。

阶段2只搭了域划分骨架,所有域组件都是空实现。阶段4-1(Terrain落地)开始,这个Actor额外
承担了一件事:持有Map域的`Map`实例(见`Source/Core/map/map.md`),因为`Map`不属于任何单一
domain组件——它是Terrain/Zone/Building/Roadnet等多个域组件将来会共同读写的公共地图数据,
放在Actor层面比放在某一个域组件里更合适。

## 关键设计
- 构造函数里手动创建一个`USceneComponent`当`RootComponent`,让这个Actor在编辑器里可以被放置、有transform(旧Framework Actor作为场景里放置的对象,新Actor延续这个可放置的定位)。
- 9个域组件全部用`CreateDefaultSubobject`在构造函数里创建并作为`AForeverFrameworkActor`的固定组成部分——不做成运行时按需添加,因为这个Actor本身就是"场景里唯一一份、职责固定"的单例式入口,不需要动态增删域。
- `BeginPlay`里打一条临时日志列出9个已初始化的域组件名字,随后调用`EnsureMapGenerated()`。这条日志是阶段2专属的验证手段,其余7个域组件填入真实逻辑前会一直保留。
- **`map`是原生指针（`Map*`），不是`std::unique_ptr<Map>`**——试过`unique_ptr`，但`Map`在这个头文件里只有前置声明，UHT给每个UCLASS生成的VTableHelper构造函数（定义在`.gen.cpp`，看不到`map/map.h`）会在异常展开路径里引用`~unique_ptr<Map>`导致编译失败（`static_assert failed: 'can't delete an incomplete type'`）。改用原生指针+析构函数里手动`delete map`绕开这个问题——`Map`不跨DLL边界（`Core.lib`静态链接进本模块），不属于`REFACTOR_PLAN.md`说的那种需要走deleter的跨模块new/delete场景，普通`delete`是安全的。为此这个类现在有一个显式声明+定义的析构函数（声明在头文件、定义在`.cpp`里`map/map.h`已完整include之后），不再用编译器隐式生成的析构函数。
- **`EnsureMapGenerated()`**（阶段4-1 Roadnet落地时从`EnsureTerrainGenerated`改名——现在编排的不只是地形）**是幂等的**:`map`已存在直接返回,否则`new Map(1024, 1024)`(默认地图尺寸,必须是2的整数次幂——原因和选1024而不是512的经过见`ForeverFrameworkActor.cpp`里`kDefaultMapWidth`/`kDefaultMapHeight`旁的注释、`Source/Basic/map/terrain_basic.md`的`MountainTerrain`密度公式说明)→`InitTerrains()`（注册地形mod+跑`DistributeTerrain`+3x3晋升规则一次性做完，原来拆成`InitTerrains`+`InitContents`两个函数，应用户要求合并回一个，见`Source/Core/map/map.md`）→`InitRoadnet()`（这个顺序不能反,`RoadnetMod::DistributeRoadnet`要采样已经生成好的地形/水面）→`InitZones()`→`InitBuildings()`→`terrainFramework->GenerateTerrain(map)`→`roadnetFramework->GenerateRoadnet(map)`→`zoneFramework->GenerateZones(map)`→`buildingFramework->GenerateBuildings(map)`。`AForeverGameMode::BeginPlay`和`FindPlayerStart_Implementation`都会调用它(见`ForeverGameMode.md`),保证不论两者实际调用顺序如何,出生点计算时地形都已经生成好。

## 依赖关系
- 依赖`Framework/ForeverFrameworkComponent.h`及其9个具体子类头文件、`Source/Core/map/map.h`。
- 被`AForeverGameMode`引用:`BeginPlay`和`FindPlayerStart_Implementation`都会调用
  `EnsureFrameworkActorExists()`(场景里没有找到已放置的实例时动态`SpawnActor`一个兜底,
  找到/生成后都会调用这个Actor的`EnsureMapGenerated()`),详见`ForeverGameMode.md`。

## 待办/后续阶段
- 阶段4:按系统迁移进度,逐个把对应域组件从空实现填成真正的逻辑(读取Core层对应Base类+对应Framework蓝图dump)。`UForeverTerrainFrameworkComponent`/`UForeverRoadnetFrameworkComponent`已经完成,其余7个还是空实现。
- 阶段4/关卡搭建阶段:一旦有人在`World.umap`里手动放置了真实的`AForeverFrameworkActor`,应该移除`ForeverGameMode`里的动态兜底生成逻辑,不要误以为是正式生成方案。
