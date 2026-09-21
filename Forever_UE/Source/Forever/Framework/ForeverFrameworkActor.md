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
- `BeginPlay`里打一条临时日志列出9个已初始化的域组件名字,随后先调一次`Registry::Get().ReloadModArgs()`(见下"7个`Ensure*Generated()`"一节),再按依赖顺序显式依次调用全部7个`Ensure*Generated()`。这条日志是阶段2专属的验证手段,其余7个域组件填入真实逻辑前会一直保留。
- **7个域指针（`map`/`populace`/`society`/`player`/`industry`/`traffic`/`story`）全部是
  原生指针，不是`std::unique_ptr<T>`**——试过`unique_ptr`，但这些类型在这个头文件里只有
  前置声明，UHT给每个UCLASS生成的VTableHelper构造函数（定义在`.gen.cpp`，看不到对应`.h`）
  会在异常展开路径里引用`~unique_ptr<T>`导致编译失败（`static_assert failed: 'can't
  delete an incomplete type'`）。改用原生指针+`EndPlay`/析构函数里手动`delete`绕开这个
  问题——它们都不跨DLL边界（`Core.lib`静态链接进本模块），不属于`REFACTOR_PLAN.md`说的那种
  需要走deleter的跨模块new/delete场景，普通`delete`是安全的。为此这个类现在有一个显式
  声明+定义的析构函数（声明在头文件、定义在`.cpp`里所有对应`.h`已完整include之后），不再
  用编译器隐式生成的析构函数。彼此都不持有对方的裸指针（`Citizen`只持有`Zone*`/
  `Building*`/`Room*`裸指针、析构不解引用它们；`Map`也不持有任何`Citizen*`），删除顺序
  不影响内存安全，`EndPlay`/析构里按和`BeginPlay()`创建顺序相反的直觉从`story`往前
  `delete`只是保持直觉。
- **7个`Ensure*Generated()`，每个只保证自己那一个域（这次拆分之前，全部7个域都挤在
  `EnsureMapGenerated()`一个函数里，见下）**：`EnsureMapGenerated()`/`EnsurePopulaceGenerated()`/
  `EnsureSocietyGenerated()`/`EnsurePlayerGenerated()`/`EnsureIndustryGenerated()`/
  `EnsureTrafficGenerated()`/`EnsureStoryGenerated()`各自都是幂等的（自己的指针已存在就
  直接返回），函数体内只做自己那部分初始化，**不会调用别的`Ensure*Generated()`**——
  `BeginPlay()`里已经按依赖顺序把全部7个显式列出来顺序调用了，一个函数体内写出来的调用
  顺序就是实际执行顺序，不存在"调用方可能不按顺序调"这回事，函数体里再调一遍上游没有意义。
  拆分前的隐藏依赖现在变成显式的调用顺序：`EnsurePopulaceGenerated()`要用到`map`
  （`Map::ComputeAccommodationTarget()`/`Checkin()`），必须排在`EnsureMapGenerated()`
  之后；`EnsurePlayerGenerated()`要用到`populace->GetCurrentYear()`，必须排在
  `EnsurePopulaceGenerated()`之后；`Society`/`Industry`/`Traffic`/`Story`这次互相独立，
  顺序上没有别的硬性要求。`Registry::Get().ReloadModArgs()`（按这一局当前已经读进内存的
  config内容刷新全部20个Factory的mod参数表——不同局可能用不同的config.json，参数因此也
  可能不同，这一步必须每局重来，跟只做一次的`RegisterConcept`不是同一件事，见
  `Source/Core/common/registry.md`"SetModArgs的调用频率"一节）不属于任何单个域，放在
  `BeginPlay()`里、7个`Ensure*Generated()`之前统一调一次，不属于哪一个`Ensure*Generated()`
  自己的职责。
  - **`EnsureMapGenerated()`**（阶段4-1 Roadnet落地时从`EnsureTerrainGenerated`改名——现在
    编排的不只是地形）：`map`已存在直接返回，否则`new Map(1024, 1024)`(默认地图尺寸,必须
    是2的整数次幂——原因和选1024而不是512的经过见`ForeverFrameworkActor.cpp`里
    `kDefaultMapWidth`/`kDefaultMapHeight`旁的注释、`Source/Basic/map/terrain_basic.md`的
    `MountainTerrain`密度公式说明)→`InitTerrains()`（跑`DistributeTerrain`+3x3晋升规则；
    20个concept的mod dll发现/注册全部归`Registry::Get()`全局管，只在整个UE进程生命周期里
    跑一次，见`Source/Core/common/registry.md`）→`InitRoadnet()`（这个顺序不能反,
    `RoadnetMod::DistributeRoadnet`要采样已经生成好的地形/水面）→`InitZones()`→
    `InitBuildings()`→`terrainFramework->GenerateTerrain(map)`→
    `roadnetFramework->GenerateRoadnet(map)`→`zoneFramework->GenerateZones(map)`→
    `buildingFramework->GenerateBuildings(map)`。`AForeverGameMode::BeginPlay`和
    `FindPlayerStart_Implementation`都会调用它(见`ForeverGameMode.md`)，语义不变——只保证
    Map这一个域，保证不论两者实际调用顺序如何,出生点计算时地形都已经生成好。
  - **`EnsurePopulaceGenerated()`（进入populace域新增）**：`populace`已存在直接返回，否则
    `new Populace()`+`populace->Init(map->ComputeAccommodationTarget())`+
    `map->Checkin(*populace)`+`populaceFramework->GenerateCitizens(map, populace)`。
    `populace`和`map`平级持有，不是`map`的成员——`Populace`是和`Map`同一层级的顶层Core类，
    不知道`Map`的存在，只通过`Map::Checkin(*populace)`单向被`Map`读取——和老工程
    `GlobalBase`同时持有`map`/`populace`两个顶层对象、由它做两者之间编排是同一个分工，
    详见`Source/Core/populace/populace.md`。
  - **`EnsureSocietyGenerated()`（进入society域新增，不再是空壳）**：`society`已存在
    直接返回，否则`new Society()`+`society->Init(map->GetAllComponents())`（按地图里
    所有Component加权随机分配Organization，每个Organization自己遍历claimed
    components设计Job）+`society->RecruitCitizens(populace->GetCitizens(),
    populace->GetCurrentYear())`（把成年市民随机匹配到还空缺的Job上）。假定`map`/
    `populace`都已经生成好，靠`BeginPlay()`里的调用顺序保证，自己不会去调
    `EnsureMapGenerated()`/`EnsurePopulaceGenerated()`，见`Core/society/society.md`。
  - **`EnsureIndustryGenerated()`/`EnsureTrafficGenerated()`（阶段4 Story落地新增）**：
    `Industry`/`Traffic`目前都是只能默认构造的空壳类，各自的`Ensure*Generated()`只是
    `new`一下，加它们纯粹是为了让`PostImplement`（`Core/common/implement.h`，
    `PostHandle`的第一个具体实现）能在构造时拿到Core全部7个domain的真实指针，不是提前
    实现这几个域的业务逻辑——`PostImplement`目前只真正用到`populace`/`player`两个指针
    （"random citizen"/"game time"两种查询），其余几个只是存着，等对应域真正迁移出
    业务逻辑、需要通过`Post`查询时再用。
  - **`EnsurePlayerGenerated()`（阶段4 Story落地新增）**：`player`已存在直接返回，否则
    `new Player()`+`player->Init()`（这次额外迁移了"全局时钟"这一小块，`Time*`+
    `Init/Tick/GetTime/SetTime/CrossDay`，见`Core/player/player.md`）+
    `player->SetTime(Time(populace->GetCurrentYear(), 1, 1, 8))`——假定`populace`已经
    生成好，靠`BeginPlay()`里`EnsurePopulaceGenerated()`排在它前面保证，自己不会去调
    `EnsurePopulaceGenerated()`。
  - **`EnsureStoryGenerated()`（阶段4 Story落地新增）**：`story`已存在直接返回，否则
    `new Story()`+`story->Init()`（读取`Resource/Story/test.json`）+
    `storyFramework->Init(story)`+`storyFramework->BroadcastGameStart()`。和`map`/
    `populace`不互相依赖。
- **`PrimaryActorTick.bCanEverTick`这次从`false`改成`true`，新增`Tick(float DeltaTime)`
  覆写**：`player`存在时调用`player->Tick(DeltaTime)`驱动全局时钟往前走（`player`在
  `EnsurePlayerGenerated()`跑完之前是`nullptr`，但`BeginPlay`同步跑完全部7个
  `Ensure*Generated()`后引擎才会开始调用`Tick`，理论上不会遇到空指针，判空只是防御性
  写法）。以后其它域需要每帧更新时也应该加进这同一个`Tick`里，不要再新开一个"谁来负责
  每帧驱动"的入口。
  - **进入society域新增：`player->Tick(DeltaTime)`之后接着驱动Job/Organization两套
    独立的调度timer**——`bool crossedDay = bIsFirstTick || player->CrossDay();
    bIsFirstTick = false;`（`bIsFirstTick`是新增成员，开局当天`Player::CrossDay()`
    永远不会天然为true——时钟是`EnsurePlayerGenerated()`刚设好的，"day"缓存和当前日期
    本来就相同，不强制第一帧当成跨天的话第一天的调度表永远生成不出来，等价于老工程
    `Populace::Tick`里`currentTime.GetYear()==0 || player->CrossDay()`这个bootstrap
    特判，PIE验证过：不加这一行时市民永远不会在第一天上下班）。现场构造一个
    `PostImplement postImplement(map, populace, society, story, industry, traffic,
    player);`（栈上对象，生命周期只覆盖这一帧）——`postImplement`供`JobMod::DailyPlan`/
    `ExecNode`通过`Post()`按需查citizen家/工位的具体地址（"citizen home address"/"citizen
    workplace address"两个post类型，见`Core/common/implement.md`），和
    `UForeverStoryFrameworkComponent::BroadcastGameStart`构造`PostImplement`同一个
    "现场构造、只覆盖这次调用"用法。之后按`Ensure*Generated()`的依赖顺序，`map`/
    `populace`/`society`/`industry`/`traffic`/`story`六个Core域挨个调用一遍`Tick`
    （`Player`已经在最前面单独`Tick`过，推进了全局时钟；`Story`/`Map`/`Industry`/
    `Traffic`目前的`Tick`都是空实现，纯粹是为了保持"每个域都有Tick"这个形状一致，见下
    "统一的Change消费入口：`ApplyChange`"一节）。`populace`/`society`的`Tick`回调这次
    简化成只构造一份per-entity的`ScriptContext`（`context.self`分别指向`citizen->
    GetJob()->GetScript()`/`organization->GetScript()`），然后对每个`Change*`调用
    `this->ApplyChange(change, context)`，不再各自手写`dynamic_cast` dispatch。

### 统一的Change消费入口：`ApplyChange`

这次重构之前，"怎么消费一个`Change*`"这段逻辑在三个地方各写了一份几乎相同的代码——
`populace->Tick`的回调、`society->Tick`的回调、`UForeverStoryFrameworkComponent::
BroadcastGameStart`的`onActions`回调，都手写了`dynamic_cast<const DebugPrintChange*>`
+"否则转给`story->ApplyChange`"这套逻辑，新增一种Change类型的处理要同时改三个地方。这次
新增`AForeverFrameworkActor::ApplyChange(const Change* change, const ScriptContext&
context)`收口成唯一入口，今后任何地方产出的`Change`都应该调这一个函数消费，不要再各自
手写dispatch。

内部顺序是两阶段：
1. 先`dynamic_cast`检查这个Actor自己能直接处理的三种类型，命中就处理完直接`return`——
   这三种都需要Core域看不到的UE层能力，只能在这一层做：
   - `NPCNavigateChange`：`Populace::FindCitizenByName(nav->GetName())`按change自带的
     occupant姓名反查`Citizen*`（`nav->GetName()`本来就是occupantName，和
     `Citizen::GetName()`一致——之前`populace->Tick`的回调是直接从lambda形参拿到
     `Citizen*`，改成统一签名后不再有实体指针，只能反过来按名字查），
     `map->LocateRoom(nav->GetDestination())`解析出`Room*`，两者都有效才调
     `populaceFramework->RequestWalk(citizen, dest)`。
   - `DebugPrintChange`：`EvaluateExpression(debugPrint->GetMessage(), context)`求值后
     `GEngine->AddOnScreenDebugMessage`打印。
   - `ChangeControlChange`：转发给`storyFramework->ApplyControlChange(controlChange,
     context)`——真正的操控权切换逻辑留在`UForeverStoryFrameworkComponent`里不动（它
     本来就知道怎么找/生成citizen Actor、怎么`Possess`），这里只是转发调用。
2. 都不是这三种时，转发给`map`/`populace`/`society`/`industry`/`traffic`/`story`六个
   Core域各自的`ApplyChange`，谁认识就处理——目前只有`Story::ApplyChange`真正执行
   `SetValueChange`并在没人认识时打一条"未实现"警告（见`Core/story/story.md`），其余
   五个域这次都只是空占位，**故意不打警告**：如果每个域都各自打一条，一次未识别的
   Change会连续刷六条重复日志，保留`Story::ApplyChange`已有的那一条作为唯一兜底就够了。

`UForeverStoryFrameworkComponent::BroadcastGameStart`的`onActions`回调这次也改成调
`framework->ApplyChange(*changePtr, context)`（`ApplyControlChange`从`private`改成
`public`，供这里跨类调用），是这次唯一被"收口进统一入口"改到的第三处，详见
`ForeverStoryFrameworkComponent.md`。
- **补上一个此前缺失的`GetStory()`**：`story`成员本身在阶段4 Story落地时就已经加入，但当时
  漏加了对应的getter（`GetMap()`/`GetPopulace()`都有，`GetStory()`没有）——这次和
  `GetSociety()`/`GetIndustry()`/`GetTraffic()`/`GetPlayer()`一起补齐，是`UForeverStoryFrameworkComponent::
  BroadcastGameStart`构造`PostImplement`时能拿到全部7个指针的前提（该函数通过
  `Cast<AForeverFrameworkActor>(GetOwner())`拿到这个Actor后依次调用这7个getter）。

## 依赖关系
- 依赖`Framework/ForeverFrameworkComponent.h`及其9个具体子类头文件、`Source/Core/map/map.h`、
  `Source/Core/populace/populace.h`（进入populace域新增，持有`Populace*`）、
  `Source/Core/story/story.h`（持有`Story*`）、`Source/Core/society/society.h`/
  `Source/Core/industry/industry.h`/`Source/Core/traffic/traffic.h`/
  `Source/Core/player/player.h`（持有`Society*`/`Industry*`/`Traffic*`/`Player*`——
  `Industry`/`Traffic`仍是空骨架，`Society`这次真的做了组织分配+招聘，见上
  "7个`Ensure*Generated()`"一节）、`Source/Core/society/organization.h`/`job.h`、
  `Source/Core/story/change.h`（`NPCNavigateChange`分发用）。
- 被`AForeverGameMode`引用:`BeginPlay`和`FindPlayerStart_Implementation`都会调用
  `EnsureFrameworkActorExists()`(场景里没有找到已放置的实例时动态`SpawnActor`一个兜底,
  找到/生成后都会调用这个Actor的`EnsureMapGenerated()`),详见`ForeverGameMode.md`。

## 待办/后续阶段
- 阶段4:按系统迁移进度,逐个把对应域组件从空实现填成真正的逻辑(读取Core层对应Base类+对应Framework蓝图dump)。`UForeverTerrainFrameworkComponent`/`UForeverRoadnetFrameworkComponent`已经完成,其余7个还是空实现。
- 阶段4/关卡搭建阶段:一旦有人在`World.umap`里手动放置了真实的`AForeverFrameworkActor`,应该移除`ForeverGameMode`里的动态兜底生成逻辑,不要误以为是正式生成方案。
