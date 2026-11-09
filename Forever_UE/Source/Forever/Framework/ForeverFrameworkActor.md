# ForeverFrameworkActor

## 职责

`AForeverFrameworkActor`是场景里唯一的Framework入口Actor：持有Core的7个顶层域对象(`Map`/`Populace`/`Society`/`Industry`/`Traffic`/`Story`/`Player`)，
按域划分为13个`UForeverFrameworkComponent`子类(Asset/Building/Populace/Roadnet/Room/Sky/Story/Terrain/Traffic/Vegetation/Lamp/Weather/Zone，
前面几个见`ForeverFrameworkComponent.md`对照表)，负责**分阶段生成世界**、**每帧Tick编排**和**Change分发**。`Map`不属于任何单一域组件，
是Terrain/Zone/Building/Roadnet等共同读写的公共数据，所以放在Actor层面持有。没有"Global"域组件——串联各域的编排逻辑就落在这个Actor自己身上。

## 关键设计

- 构造函数里手动创建`USceneComponent`当根组件，**必须设成Static**：否则挂在下面的每个子组件(哪怕自己设成Static)都会被引擎视为Movable，成千上万的墙体/碰撞盒堆进动态八叉树和
  动态broadphase，建筑LOD整层增删越来越卡(见`ForeverBuildingFrameworkComponent.md`"性能")。13个域组件都用`CreateDefaultSubobject`创建，是固定组成部分。
- **7个域指针全部是原生指针，不是`std::unique_ptr`**：头文件里只有前置声明，UHT给UCLASS生成的VTableHelper构造函数(在`.gen.cpp`，看不到完整类型)会在异常展开路径引用`~unique_ptr`而编译失败。
  所以用原生指针+显式析构函数(定义在`.cpp`里)和`EndPlay`里手动`delete`。它们不跨DLL(`Core.lib`静态链接进本模块)，普通`delete`安全；彼此不持有对方的裸指针，删除顺序不影响内存安全。
- **`EndPlay`必须同步`delete`各域**，不能只靠析构函数：析构时机取决于UObject垃圾回收，可能晚于下一次PIE；`Map`持有的`ModLoader`析构时才`FreeLibrary`卸载mod dll，
  没卸载干净时下一次`LoadLibrary`拿到旧dll镜像，表现为"代码改了、编译成功、PIE里跑的还是旧逻辑"。`EndPlay`/析构里都要**先`WaitForPendingPathfinding()`**
  等异步寻路任务跑完(后台线程还持有`map`/`populace`裸指针；不能用"派发GameThread任务再等"，会在GameThread上死锁)。
- **每个`Ensure*Generated()`幂等，只保证自己那个域，不调别的域/步骤**，依赖顺序由`BuildGenerationStages`显式列出：人口要用`map`(`ComputeAccommodationTarget`/`Checkin`)；
  社会要用`map`/`populace`；玩家要用`populace->GetCurrentYear()`(开局时间=那一年1月1日8点，让生日/年龄与时钟自洽，见`populace.md`"生日换算")；
  产业/交通按地图生成；剧情独立。
- **载具在剧情之后生成**：`AVehicleElement::Init`会立刻烘焙`vehicle->GetOptions()`快照，而"上车"选项要等`game_start`广播后才被加上，所以`EnsureTrafficGenerated`只建Core对象，
  `GenerateVehicles`放在"构建载具"阶段(`ACitizenElement`同理等`game_start`后才流式生成)。

## 分阶段生成 + 开局进度(`BuildGenerationStages`/`RunGenerationStep`/`GetLoadProgress`)

整段同步生成会卡死主线程几十秒，开局黑屏上的进度条没机会重绘，所以`BeginPlay`只登记一串阶段(`generationStages`)，`Tick`每帧执行一个。`BeginPlay`先调
`AForeverPlayerController::InitializeOnce`(打包版里本Actor的BeginPlay先于Controller的，game_start里的小游戏/对话要用Widget)。

阶段顺序(新游戏/读档同一套)：加载模组(`Registry::RefreshMods`+`ValidateMainStoryDependencies`) → 预热截屏(`UForeverWeaponComponent::WarmUpScreenshot`，相机拍照第一次GPU回读约0.5s卡顿，趁加载界面先偷偷截一张丢掉) → 构建地形 → 路网 → 园区 → 建筑 → 栽种植被 → 布置路灯 →
构建交通 → 生成地形/道路/园区/建筑/植被/路灯模型 → 构建人口 → 构建社会与产业(Society+Player+Industry) → 执行开局剧情(读档：恢复存档，即`EnsureStoryGenerated`) →
构建载具(读档：恢复载具与玩家，含`RestoreControlledPawn`/`RestoreWorldAssets`) → (进图后)构建场景。

- 地图被拆成`BuildMap*`(Core数据，读档时`BuildMapTerrain`恢复整张地图其余跳过)和`Generate*Mesh`(UE网格)两批：Core数据全部算完才统一生成网格，
  因为Roadnet要采样地形/水面，Zone/Building要用Roadnet产出的Lot，路网网格要等`InitZones`/`InitBuildings`新增的小路。**构建交通排在地形网格之前**，因为instantiate线路(火车)铺轨要请求地形抬升/挖隧道洞。
  植被/路灯要等路网开口/园区/建筑齐全才能避开出入口，网格Z取地形高度所以在地形之后。
- 默认地图`kDefaultMapWidth/Height`=1024：必须是2的整数次幂(LOD网格最粗一级跨度要正好等于边长)，且`w*h/(512*512)`>1才会生成山，见`Source/Basic/map/terrain_basic.md`。
  `GetDefaultSpawnLocation()`给GameMode用(地图中心，地形还没生成)，`AForeverPlayerGameMode`不再提前生成地图。
- 每个阶段占两帧：第一帧只让`GetLoadProgress`的"当前阶段名"被控件读到并画出来，第二帧才执行(阶段会同步卡住主线程)。`LoadMap`结束后先空等2帧让黑屏渲染。
  每个阶段单独`try/catch ExceptionBase`：`UE_LOG`打`GetDetailedInfo()`后`QuitGame`(不用`FPlatformMisc::RequestExit`，它在PIE里可能连Editor一起关)，之后不再继续、不置完成标志。
  所有"生成世界过程中的致命错误"(mod依赖缺失、取名重试耗尽、取名器未启用等)都在Core层`THROW_EXCEPTION`，一路穿到这里处理，中间层不感知。
- 最后一个阶段完成后置`bInitialGenerationDone`(`IsInitialGenerationDone()`只读，开局黑屏`UI/FadeWidget`用它加`IsLodSettled()`判断世界就绪；game_start广播是同步的，这个标志就是"响应完"的等价物)；
  玩家如果还是过渡用的`ADefaultPawn`，会被挪到地形上方的地图中心。生成完成前`Tick`只驱动生成、不推进时钟/调度。
- `GetLoadProgress(done, total, label)`：total=阶段数+1(最后一格"构建场景"，等`IsLodSettled()`)；`BeginPlay`之前total=0。
- 生成完成前其它组件不干活：`ForeverBuildingFrameworkComponent`/`ABuildingElement::Tick`(不做LOD切换，`IsLodSettled()`要生成完再过2帧才为真)、`ForeverTerrainFrameworkComponent`(地形LOD环)、
  `ForeverPopulaceFrameworkComponent`(否则game_start前就流式生成市民，选项快照为空)。

### `ValidateMainStoryDependencies()`

遍历`Config::GetStoryScripts()`(Story screen里玩家添加的脚本列表，为空才退回`Config::GetMainStoryScriptPath()`，保证编辑器直接PIE的工作流)，
对每个脚本的`Script::GetModDependences`里每个mod id调`Registry::CheckModRegistered`，缺失就`THROW_EXCEPTION(RuntimeException)`。这是兜底二次校验，
更早的校验在`ForeverConfigBridgeSubsystem::ValidateAndStartGame`。

## 读档

`FetchActiveLoad()`第一次调用时从`UForeverSaveSubsystem::ConsumePendingLoad()`取走读档请求，之后一直返回同一份(没有读档返回`nullptr`)。各域在自己的生成步骤里按存档恢复，失败时打错误并退回重新生成：
- 地图：`MapSaveCodec::Load`，成功后`SaveGame::RestoreInstanceCounters`恢复各mod类命名计数器(必须在其余域重新生成之前，新建的mod实例才不和恢复出来的名字撞)；失败时还有屏幕提示。
- 人口(`bPopulaceRestored`，连同房产归属/住处)；社会段按市民下标引用市民，**只有人口也是按存档恢复的才读**；人口恢复而社会重新生成时不再跑`GenerateEmploymentHistory`(经历里已有)。
  人口段留给社会段去连的`pendingJobExperienceOrganizations`在社会步骤后清空。
- 玩家(时钟/生命/物品/房间)、产业(按地图重新生成仓库/工坊再放回库存)、交通(站点/线路/车辆)。
- 剧情：`Story::Init`后`ScriptSaveCodec::Load`恢复全部Script状态，成功时**不广播game_start**(`bScriptsRestored`，game_start做过的事在存档里已是结果)；
  之后在"恢复载具与玩家"阶段由`RestoreControlledPawn`按存档UE段让玩家重新操控存档时的市民(开车时先占有市民再上车，位置朝向/视角/肩膀武器/弹匣余量都恢复；线路公交位置以Core的Route为准不覆盖)。

## Tick

生成完成之后每帧：`player->Tick`推进全局时钟 → `UpdateSystemTime`写系统变量 → 左上角固定key=9000显示游戏时间 → `UpdateSky` → 天气(先`ConsumeChanged`广播`weather_change`，再`UpdateWeather`，
**必须在`UpdateWind`之前**，风的倍数由它写入) → `UpdateWind` → `UpdateLamps` → 调度。

调度：`crossedDay = bIsFirstTick || player->CrossDay()`(开局当天时钟刚设好，不会天然跨天，第一帧强制当作跨天，否则第一天的Job/Organization调度表永远生成不出来)。
现场构造一个只覆盖这一帧的`PostImplement`(Job/OrganizationMod通过`Post()`查市民家/工位地址，见`Core/common/implement.md`)，依次`Tick`：map → populace → society(两者的回调
构造per-entity的`ScriptContext`，`context.self`指向`citizen->GetJob()->GetScript()`/`organization->GetScript()`，对每个`Change*`调`ApplyChange`；`Change*`所有权在产出它的mod实例上，这里只读) →
`Economy::SyncIndustry` → industry → 每日经济(`crossedDay`时`Economy::Daily`，货款/服务/房租/工资/补助/月初分红，之后检查`Ledger::Total()==0`否则报错，见`Core/society/economy.md`) →
traffic(并把`ConsumeStopEvents`转成`EnterStopEvent`/`LeaveStopEvent`广播给公交自己的script) → story → `FireExpiredTimers`。新的每帧逻辑加进这同一个`Tick`，不要新开入口。

## 统一的Change消费入口：`ApplyChange`

populace/society的Tick回调、`UForeverStoryFrameworkComponent::BroadcastGameStart`的回调等任何地方产出的`Change`都走这一个函数。顺序：
1. 先判`change->GetCondition()`(非空且求值为假就不执行)，`for_range`的子change等也经过这里。
2. 需要UE层能力的类型直接处理并`return`：`NPCNavigateChange`(按姓名/房间地址反查，查不到再当表达式求值后查，调`RequestWalk`)、`NPCTeleportChange`(destination写`home`=市民的家)、
   `GlobalSettingChange`(目前只认`time_flow_ratio`)、`PlaceHolderChange`(`ScriptMod::FindLabel`的锚点，什么都不做)、`DebugPrintChange`、`ForRangeChange`(递归，上限`kMaxForRangeIterations`=1000，循环变量写进当前脚本变量池)、
   `GlobalMessageChange`、`NPCReviveChange`/`SpawnNpcChange`(`ApplyReviveNpc`/`ApplySpawnNpc`)、计时器/操控权/小游戏/商店/工资/载具上下车等转发给`storyFramework->Apply*`，
   背包四个Change转发给`assetFramework`，测试导航/载具命令转发给`trafficFramework`/`populaceFramework`。这些都需要Core域看不到的`Room*`/`ACitizenElement`/`GEngine`/`APlayerController`/UMG。
3. 其余类型转发给map/populace/society/industry/traffic/story/player七个域的`ApplyChange`，谁认识(返回true)就处理，同一种Change可以有多个域认识(如`add_option`门/市民/车辆各管各的)。
   全部返回false才在这里打一条"未实现"警告(域自己不打，避免重复刷屏)。
4. `add_option`/`remove_option`之后`RefreshInteractOptions(name)`让场景里同名的市民/车辆/公交/资产/门/电梯轿厢刷新交互选项快照，否则要等下次重新生成才看到变化。

`ApplySpawnNpc`：Core里`Populace::SpawnCitizen`建市民，`economy`为真才`SetInEconomy`+`SettleCitizen`(按`home`房间地址)+`RecruitCitizen`，否则只`SetCurrentRoom`；每个NPC都`Settlement::InitFunds`
(钱包不论接不接入经济都开，被玩家操控时直接有钱)；没有`home`则没有位置，要等`npc_teleport`；最后`populaceFramework->AddCitizen`登记进流式生成列表。
`ApplyReviveNpc`：`Citizen::Revive`，之后需要`npc_teleport`才会出现。`GetControlledCitizen()`：玩家Pawn是`ACitizenElement`时返回其市民(玩家的钱就是它在Ledger里的账户)。

`DescribeLocation(worldLocation)`：房间地址 > 建筑地址 > 园区地址 > 地块(顶层Lot旋转矩形包含判断)地址 > "x,y,z"(地图单位)。房间/建筑/园区用触发盒做`ECC_Pawn`小球重叠查询反查，死亡事件的`local.location`用它。

## 依赖关系

- 依赖：13个域组件头文件；Core的`map/populace/society/industry/traffic/story/player`及各自`*_save.h`、`common/registry.h`/`config.h`/`error.h`/`implement.h`/`save.h`、`story/change.h`/`script.h`；
  `Element/CitizenElement`/`VehicleElement`/`BuildingElement`/`DoorComponent`；`Mod/ForeverSaveSubsystem`；`Player/ForeverPlayerController`。
- 被`AForeverPlayerGameMode`引用：场景里没有已放置的实例时动态`SpawnActor`一个兜底，见`ForeverPlayerGameMode.md`。各域组件通过`Cast<AForeverFrameworkActor>(GetOwner())`取域指针。

## 待办/后续阶段

- 一旦有人在`World.umap`里手动放置了真实的`AForeverFrameworkActor`，应该移除`ForeverPlayerGameMode`里的动态兜底生成逻辑，不要误以为是正式生成方案。

## 跳时间(jump_time)
`ApplyChange`的`JumpTimeChange`分支 → `BeginTimeSkip(target)`。流程：
1. `Begin`：`populaceFramework->BeginTimeSkip()`(在途行走全部冲刷到目的地，之后`RequestWalk`同步瞬移+`npc_arrive`)、
   `SetCinematicMode`锁移动/视角、`UFadeSubsystem::BeginHoldBlack()`渐黑；黑屏保持住之后才开始推进(没显示黑屏则直接开始)。
2. 每帧`Tick`在跳期间整体让位给`RunTimeSkipBudget`(预算约8ms墙钟)：小步推进，步长=min(1游戏小时, 下一个0点, 目标)；
   每步`SetTime`→`CrossDay`(这一步跨了0点才为真，保证每日体恰好一天一次)→天气`Tick`+`weather_change`→`StepWorld`
   (map/populace/society/industry/经济+移民/traffic/story，**与常规Tick共用同一份**)→反复排空Job/Scheduler/组织到期项
   (`HasDueTimers`)→`DrainExpiredTimers`(story计时器)。
3. `FinishTimeSkip`：结束跳时间模式、`ResyncBusesAfterTimeSkip`(公交物理车收回，以时刻表为准)、解锁输入、`ReleaseHold`渐隐。
进度：`GetTimeSkipProgress`(已推进游戏秒/总秒)，黑屏进度条直接用。跳期间再触发jump_time被忽略。
局限：走路用瞬移代替；`Traffic::elapsedSeconds`是float，超长跳(>约190天)精度下降；`StepWorld`开头把游戏年份同步给`Populace::currentYear`(`SetCurrentYear`，年龄随之增长)，常规Tick和跳时间共用。
`StepWorld`/`TickPopulaceDomain`/`TickSocietyDomain`是从原Tick里抽出来的，顺序不能变。
