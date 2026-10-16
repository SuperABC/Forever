# ForeverFrameworkActor

## 职责

`AForeverFrameworkActor`是场景里唯一的Framework入口Actor，内部按域划分为
`UForeverFrameworkComponent`子类（见`ForeverFrameworkComponent.md`的对照表）。这个Actor
同时持有Map域的`Map`实例（见`Source/Core/map/map.md`）——`Map`不属于任何单一domain组件，
是Terrain/Zone/Building/Roadnet等多个域组件会共同读写的公共地图数据，放在Actor层面比放在
某一个域组件里更合适。

## 关键设计

- 构造函数里手动创建一个`USceneComponent`当`RootComponent`，让这个Actor在编辑器里可以被
  放置、有transform。
- 9个域组件全部用`CreateDefaultSubobject`在构造函数里创建并作为固定组成部分——不做成
  运行时按需添加，因为这个Actor本身就是"场景里唯一一份、职责固定"的单例式入口，不需要动态
  增删域。
- **7个域指针（`map`/`populace`/`society`/`player`/`industry`/`traffic`/`story`）全部是
  原生指针，不是`std::unique_ptr<T>`**——这些类型在这个头文件里只有前置声明，UHT给每个
  UCLASS生成的VTableHelper构造函数（定义在`.gen.cpp`，看不到对应`.h`）会在异常展开路径里
  引用`~unique_ptr<T>`导致编译失败（`static_assert failed: 'can't delete an incomplete
  type'`）。用原生指针+`EndPlay`/析构函数里手动`delete`绕开这个问题——它们都不跨DLL边界
  （`Core.lib`静态链接进本模块），普通`delete`是安全的。为此这个类有一个显式声明+定义的
  析构函数（声明在头文件、定义在`.cpp`里所有对应`.h`已完整include之后）。彼此都不持有对方
  的裸指针（`Citizen`只持有`Zone*`/`Building*`/`Room*`裸指针、析构不解引用它们；`Map`也
  不持有任何`Citizen*`），删除顺序不影响内存安全，`EndPlay`/析构里从`story`往前`delete`
  只是跟`BeginPlay()`创建顺序保持直觉上的对称。
- **7个`Ensure*Generated()`，每个只保证自己那一个域**：`EnsureMapGenerated()`/
  `EnsurePopulaceGenerated()`/`EnsureSocietyGenerated()`/`EnsurePlayerGenerated()`/
  `EnsureIndustryGenerated()`/`EnsureTrafficGenerated()`/`EnsureStoryGenerated()`各自都是
  幂等的（自己的指针已存在就直接返回），函数体内只做自己那部分初始化，**不会调用别的
  `Ensure*Generated()`**——`BeginPlay()`按依赖顺序把全部7个显式列出来顺序调用，一个函数
  体内写出来的调用顺序就是实际执行顺序。依赖关系体现在调用顺序本身：
  `EnsurePopulaceGenerated()`要用到`map`（`Map::ComputeAccommodationTarget()`/
  `Checkin()`），必须排在`EnsureMapGenerated()`之后；`EnsurePlayerGenerated()`要用到
  `populace->GetCurrentYear()`，必须排在`EnsurePopulaceGenerated()`之后；`Society`/
  `Industry`/`Traffic`/`Story`互相独立，顺序上没有硬性要求。`Registry::Get().
  ReloadModArgs()`（按当前已读进内存的config内容刷新全部20个Factory的mod参数表——不同局
  可能用不同的config.json，这一步每局都要重来，跟只做一次的`RegisterConcept`不是同一件事，
  见`Source/Core/common/registry.md`"SetModArgs的调用频率"一节）不属于任何单个域，放在
  `BeginPlay()`里、7个`Ensure*Generated()`之前统一调一次。
  - **`EnsureMapGenerated()`**：`map`已存在直接返回，否则`new Map(1024, 1024)`(默认地图
    尺寸，必须是2的整数次幂，见`ForeverFrameworkActor.cpp`里`kDefaultMapWidth`/
    `kDefaultMapHeight`旁的注释、`Source/Basic/map/terrain_basic.md`的`MountainTerrain`
    密度公式说明)→`InitTerrains()`（跑`DistributeTerrain`+3x3晋升规则；20个concept的mod
    dll发现/注册全部归`Registry::Get()`全局管，只在整个UE进程生命周期里跑一次，见
    `Source/Core/common/registry.md`）→`InitRoadnet()`（这个顺序不能反,
    `RoadnetMod::DistributeRoadnet`要采样已经生成好的地形/水面）→`InitZones()`→
    `InitBuildings()`→`terrainFramework->GenerateTerrain(map)`→
    `roadnetFramework->GenerateRoadnet(map)`→`zoneFramework->GenerateZones(map)`→
    `buildingFramework->GenerateBuildings(map)`。`AForeverGameMode::BeginPlay`和
    `FindPlayerStart_Implementation`都会调用它(见`ForeverGameMode.md`)，只保证Map这一个
    域，不论两者实际调用顺序如何，出生点计算时地形都已经生成好。
  - **`EnsurePopulaceGenerated()`**：`populace`已存在直接返回，否则`new Populace()`+
    `populace->Init(map->ComputeAccommodationTarget())`+`map->Checkin(*populace)`+
    `populaceFramework->GenerateCitizens(map, populace)`。`populace`和`map`平级持有，
    不是`map`的成员——`Populace`是和`Map`同一层级的顶层Core类，不知道`Map`的存在，只通过
    `Map::Checkin(*populace)`单向被`Map`读取，详见`Source/Core/populace/populace.md`。
  - **`EnsureSocietyGenerated()`**：`society`已存在直接返回，否则`new Society()`+
    `society->Init(map->GetAllComponents())`（按地图里所有Component加权随机分配
    Organization，每个Organization自己遍历claimed components设计Job）+
    `society->RecruitCitizens(populace->GetCitizens(), populace->GetCurrentYear())`
    （把成年市民随机匹配到还空缺的Job上）+`society->GenerateEmploymentHistory(
    populace->GetCitizens(), populace->GetCurrentYear())`（四类人际关系生成之一：入职
    历史反推+同事关系，见`Core/society/society.md`"四类人际关系生成：同事关系"一节）。
    假定`map`/`populace`都已经生成好，靠`BeginPlay()`里的调用顺序保证，自己不会去调
    `EnsureMapGenerated()`/`EnsurePopulaceGenerated()`，见`Core/society/society.md`。
  - **`EnsureIndustryGenerated()`/`EnsureTrafficGenerated()`**：`Industry`/`Traffic`
    目前都是只能默认构造的空壳类，各自的`Ensure*Generated()`只是`new`一下，加它们纯粹是
    为了让`PostImplement`（`Core/common/implement.h`，`PostHandle`的第一个具体实现）能
    在构造时拿到Core全部7个domain的真实指针——`PostImplement`目前只真正用到`populace`/
    `player`两个指针（"random citizen"/"game time"两种查询），其余几个只是存着，等对应域
    真正迁移出业务逻辑、需要通过`Post`查询时再用。
  - **`EnsurePlayerGenerated()`**：`player`已存在直接返回，否则`new Player()`+
    `player->Init()`（全局时钟，`Time*`+`Init/Tick/GetTime/SetTime/CrossDay`，见
    `Core/player/player.md`）+`player->SetTime(Time(populace->GetCurrentYear(), 1, 1,
    8))`——假定`populace`已经生成好，靠`BeginPlay()`里`EnsurePopulaceGenerated()`排在
    它前面保证。主线剧情`.script`的`global_settings`字段如果有`time_flow_ratio`，紧接着
    读一次`Script::GetGlobalSettings(Config::GetMainStoryScriptPath())`并调
    `player->SetTimeFlowRatio(...)`覆盖默认值`2.0`，见`Core/player/player.md`"时钟行为"
    一节。
  - **`EnsureStoryGenerated()`**：`story`已存在直接返回，否则`new Story()`+`story->
    Init()`（读取`Resource/Story/test.json`）+`storyFramework->Init(story)`+
    `storyFramework->BroadcastGameStart()`。和`map`/`populace`不互相依赖。

### `ValidateMainStoryDependencies()`+`BeginPlay()`统一`try/catch`

`BeginPlay()`把`Registry::Get().ReloadModArgs()`之后、7个`Ensure*Generated()`那一段
包进一个`try { ValidateMainStoryDependencies(); EnsureMapGenerated(); ... } catch
(const ExceptionBase& e) { ... }`：

- `ValidateMainStoryDependencies()`遍历主线剧情`.script`的`mod_dependences`字段
  （`Script::GetModDependences(Config::GetMainStoryScriptPath())`），对每个mod id调
  `Registry::Get().CheckModRegistered(id)`，有任何一个没注册就`THROW_EXCEPTION(
  RuntimeException, ...)`——放在`try`块最前面，先于任何`EnsureXxxGenerated()`，校验
  失败直接跳过全部世界生成（`map`/`populace`等全部保持`nullptr`），见
  `Core/common/registry.md`"`CheckModRegistered`"一节。
- `catch`分支：`UE_LOG(LogTemp, Error, ...)`把`e.GetDetailedInfo()`（`common/error.h`
  的`ExceptionBase`已有方法，含消息+文件+行号+函数名）打进日志，再调
  `UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(
  GetWorld(), 0), EQuitPreference::Quit, false)`退出——用`QuitGame`不用
  `FPlatformMisc::RequestExit`，因为后者在PIE里可能把整个Editor进程一起关掉，
  `QuitGame`能干净结束这次PIE会话/游戏进程，不影响Editor本体；不用弹窗，这些都是
  内容/配置错误，不是玩家运行时该看到的提示，这个工程也没有`FMessageDialog`一类的
  弹窗先例。
- 这个`try/catch`不是只为`mod_dependences`一个场景设计——`Populace::InitNames()`的
  `chinese`取名器未启用、`Name::GenerateName`重试耗尽仍撞上`name_reserve`占位名（见
  `populace.md`/`name.md`）等所有"生成世界过程中出现的致命错误"都统一
  `THROW_EXCEPTION`，从各自所在的层（可能是`Core`层，不能碰UE）一路不需要任何中间层
  感知，直接穿到这一处`catch`。顺带也兜住了`Script::ReadScript`本来就有的
  `JsonFormatException`/`IOException`，现在统一变成"打日志+退出"。
- **`PrimaryActorTick.bCanEverTick`为`true`，`Tick(float DeltaTime)`**：`player`存在
  时调用`player->Tick(DeltaTime)`驱动全局时钟往前走（`player`在`EnsurePlayerGenerated()`
  跑完之前是`nullptr`，但`BeginPlay`同步跑完全部7个`Ensure*Generated()`后引擎才会开始
  调用`Tick`，理论上不会遇到空指针，判空只是防御性写法）。以后其它域需要每帧更新时也应该
  加进这同一个`Tick`里，不要新开一个"谁来负责每帧驱动"的入口。
  - `player->Tick(DeltaTime)`之后接着驱动Job/Organization两套独立的调度timer——`bool
    crossedDay = bIsFirstTick || player->CrossDay(); bIsFirstTick = false;`
    （`bIsFirstTick`开局当天需要强制当成"跨天"，否则第一天的调度表永远生成不出来：时钟
    是`EnsurePlayerGenerated()`刚设好的，"day"缓存和当前日期本来就相同，`Player::
    CrossDay()`天然不会为true）。现场构造一个`PostImplement postImplement(map, populace,
    society, story, industry, traffic, player);`（栈上对象，生命周期只覆盖这一帧）——
    供`JobMod::DailyPlan`/`ExecNode`通过`Post()`按需查citizen家/工位的具体地址（"citizen
    home address"/"citizen workplace address"两个post类型，见`Core/common/
    implement.md`），和`UForeverStoryFrameworkComponent::BroadcastGameStart`构造
    `PostImplement`同一个"现场构造、只覆盖这次调用"用法。之后按`Ensure*Generated()`的
    依赖顺序，`map`/`populace`/`society`/`industry`/`traffic`/`story`六个Core域挨个
    调用一遍`Tick`（`Player`已经在最前面单独`Tick`过，推进了全局时钟；`Story`/`Map`/
    `Industry`/`Traffic`目前的`Tick`都是空实现，纯粹是为了保持"每个域都有Tick"这个形状
    一致，见下"统一的Change消费入口：`ApplyChange`"一节）。`populace`/`society`的`Tick`
    回调只构造一份per-entity的`ScriptContext`（`context.self`分别指向`citizen->
    GetJob()->GetScript()`/`organization->GetScript()`），然后对每个`Change*`调用
    `this->ApplyChange(change, context)`，不各自手写`dynamic_cast`dispatch。

### 统一的Change消费入口：`ApplyChange`

`AForeverFrameworkActor::ApplyChange(const Change* change, const ScriptContext&
context)`是消费任意`Change*`的唯一入口——`populace->Tick`的回调、`society->Tick`的回调、
`UForeverStoryFrameworkComponent::BroadcastGameStart`的`onActions`回调都统一调这一个
函数，不各自手写dispatch。

内部顺序是两阶段：
1. 先`dynamic_cast`检查这个Actor自己能直接处理的三种类型，命中就处理完直接`return`——
   这三种都需要Core域看不到的UE层能力，只能在这一层做：
   - `NPCNavigateChange`：`Populace::FindCitizenByName(nav->GetName())`按change自带的
     occupant姓名反查`Citizen*`（`nav->GetName()`本来就是occupantName，和
     `Citizen::GetName()`一致），`map->LocateRoom(nav->GetDestination())`解析出
     `Room*`，两者都有效才调`populaceFramework->RequestWalk(citizen, dest)`。
   - `DebugPrintChange`：`EvaluateExpression(debugPrint->GetMessage(), context)`求值后
     `GEngine->AddOnScreenDebugMessage`打印。
   - `ChangeControlChange`：转发给`storyFramework->ApplyControlChange(controlChange,
     context)`——真正的操控权切换逻辑留在`UForeverStoryFrameworkComponent`里不动（它
     本来就知道怎么找/生成citizen Actor、怎么`Possess`），这里只是转发调用。
2. 都不是这三种时，转发给`map`/`populace`/`society`/`industry`/`traffic`/`story`六个
   Core域各自的`ApplyChange`，谁认识就处理——目前只有`Story::ApplyChange`真正执行
   `SetValueChange`并在没人认识时打一条"未实现"警告（见`Core/story/story.md`），其余
   五个域都只是空占位，**故意不打警告**：如果每个域都各自打一条，一次未识别的Change会
   连续刷六条重复日志，保留`Story::ApplyChange`已有的那一条作为唯一兜底就够了。

`UForeverStoryFrameworkComponent::BroadcastGameStart`的`onActions`回调调
`framework->ApplyChange(*changePtr, context)`（`ApplyControlChange`是`public`，供这里
跨类调用），详见`ForeverStoryFrameworkComponent.md`。

`GetMap()`/`GetPopulace()`/`GetSociety()`/`GetIndustry()`/`GetTraffic()`/`GetPlayer()`/
`GetStory()`七个getter齐全，是`UForeverStoryFrameworkComponent::BroadcastGameStart`
构造`PostImplement`时能拿到全部7个指针的前提（该函数通过
`Cast<AForeverFrameworkActor>(GetOwner())`拿到这个Actor后依次调用这7个getter）。
- **植被**——`vegetationFramework`(`UForeverVegetationFrameworkComponent`)在
  `EnsureMapGenerated`最后、`GenerateBuildings`之后调用`GenerateVegetation(map)`：Core里的树在
  `InitRoadnet`/`InitBuildings`时已经展开好，这里一次性加载mesh+批量实例化，见
  `ForeverVegetationFrameworkComponent.md`。

## 依赖关系
- 依赖`Framework/ForeverFrameworkComponent.h`及其9个具体子类头文件、`Source/Core/map/map.h`、
  `Source/Core/populace/populace.h`（持有`Populace*`）、`Source/Core/story/story.h`
  （持有`Story*`）、`Source/Core/society/society.h`/`Source/Core/industry/industry.h`/
  `Source/Core/traffic/traffic.h`/`Source/Core/player/player.h`（持有`Society*`/
  `Industry*`/`Traffic*`/`Player*`——`Industry`/`Traffic`仍是空骨架，`Society`真的做了
  组织分配+招聘，见上"7个`Ensure*Generated()`"一节）、`Source/Core/society/
  organization.h`/`job.h`、`Source/Core/story/change.h`（`NPCNavigateChange`分发用）、
  `Source/Core/story/script.h`（`GetModDependences`/`GetGlobalSettings`）、
  `Source/Core/common/config.h`（`GetMainStoryScriptPath`）、`Source/Core/common/
  error.h`（`ExceptionBase`/`RuntimeException`，`BeginPlay()`的`try/catch`）、
  `Kismet/KismetSystemLibrary.h`（`QuitGame`）。
- 被`AForeverGameMode`引用：`BeginPlay`和`FindPlayerStart_Implementation`都会调用
  `EnsureFrameworkActorExists()`（场景里没有找到已放置的实例时动态`SpawnActor`一个兜底，
  找到/生成后都会调用这个Actor的`EnsureMapGenerated()`），详见`ForeverGameMode.md`。

## 待办/后续阶段
- 一旦有人在`World.umap`里手动放置了真实的`AForeverFrameworkActor`，应该移除
  `ForeverGameMode`里的动态兜底生成逻辑，不要误以为是正式生成方案。
