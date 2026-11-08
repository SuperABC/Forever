# ForeverStoryFrameworkComponent

## 职责

Story域的UE层接线点。`Core/story`的`Script`/`Story`只做纯C++的匹配/求值，不感知UE；这个组件负责：

- 各类事件的广播入口（把Event发给对应的Script，匹配出的`ScriptAction`交给`ProcessScriptActions`：`Dialog`→`ShowDialog`排队展示，`Change`→`AForeverFrameworkActor::ApplyChange`）；
- `Dialog`的排队展示（`SectionSpeakingWidget`/`SectionOptionWidget`）与后台对话；
- Core层做不到（需要Actor/Controller/Widget）的Change执行；
- 计时器的创建/删除/到时触发；
- 系统变量（时间、玩家所在位置）的写入。

## 生命周期

`Init(Story*)`由`AForeverFrameworkActor::EnsureStoryGenerated()`在创建完`Story`并调用完`story->Init()`之后调用一次（只存指针，不持有所有权）。
`BroadcastGameStart()`紧接着被同一处代码调用——不放在本组件`BeginPlay()`里，因为`AForeverFrameworkActor::BeginPlay()`先级联触发所有子组件的`BeginPlay`、再依次调`Ensure*Generated()`，届时`story`还是空的。
`EndPlay`清掉后台对话计时器和`backgroundQueue`。

## 事件广播入口

所有入口都复用`ProcessScriptActions`，并现场构造`PostImplement`（供`WrapScript`反查Core状态，如"随机挑一个citizen"，生命周期只覆盖这次同步广播）。执行Change时不在屏幕上打任何调试消息，要看执行了什么在脚本里加`debug_print`。

- **`BroadcastGameStart()`**：开局调用一次。先`UpdateSystemTime()`（保证第一个事件就能读到时间），主线走`Story::BroadcastGameStart`，
  其余所有"独立持有Script"的对象走`BroadcastToAllScripts(includeMain=false)`，否则它们的`game_start`里程碑永远不会触发（市民"敲门"等选项全靠它）。
- **`BroadcastToAllScripts(event, postImplement, includeMain)`**：依次发给主线（可选）、每个`Organization`/`Job`、每个市民`Scheduler`、每辆车、有Script的门、电梯轿厢（`Cabin`）、建筑/房间/园区实体脚本、
  以及资产脚本（玩家身上/estate/地上`worldAssets`/房间里的资产，含容器内容；`game_start`时对已`IsScriptStarted()`的跳过并置位，之后新建资产靠`StartAssetScript`补发）。
  `BroadcastGameStart`、`BroadcastGlobalMessage`、`BroadcastWeatherChange`、`BroadcastSpeakingFinish`共用。
- **`BroadcastGlobalMessage(message)`**：`global_message` Change触发，发`GlobalMessageEvent`给全部Script（含主线）。嵌套深度`globalMessageDepth`超过`kMaxGlobalMessageDepth`(8)时丢弃并警告，防止里程碑互相触发无限递归。
- **`BroadcastWeatherChange(weather)`**：天气变了（自动随机换或`change_weather`）时由`AForeverFrameworkActor::Tick`调用，发`WeatherChangeEvent`给全部Script（含主线）；读档不广播。
- **`BroadcastSpeakingFinish(label)`**：一句带label的台词播完时由`AdvanceDialog`/`AdvanceBackground`调用，发`SpeakingFinishEvent`给全部Script（含主线）。
- **`OptionDialog(name, option)`**：玩家点击`MeetOption`选项时调用，构造`OptionDialogEvent`。匹配主线Script，再按`name`分别尝试：市民（其`Scheduler`脚本和`Job`脚本，工作中的市民在Job脚本里加选项）、
  车辆（`Traffic::FindVehicleByName`）、门（`Map::FindDoorByName`）、场景里的资产（`AssetFramework::FindElementByName`）、电梯（`Map::FindCabinByName`），命中哪个就额外匹配哪个的Script。
- **`BroadcastCitizenDecease`**（两个重载，字符串版转到事件版）：市民死亡时调用，匹配主线Script + 死者自己的`Scheduler`脚本（死亡只销毁UE侧Actor，Core里的`Citizen`和`Scheduler`还在）。
  不做通用的"任意Event广播给任意Script"机制，原因见`Player/ForeverWeaponComponent.md`。
- **`BroadcastNpcArrive(citizen, room)`**：由`UForeverPopulaceFrameworkComponent::ReportArrival`调用，发`npc_arrive`给该市民自己的`Scheduler`脚本和`Job`脚本，嵌套深度`npcArriveDepth`超过`kMaxNpcArriveDepth`(8)丢弃并警告。
- **`BroadcastToVehicle(event, vehicle)`**：只发给这辆载具自己的Script（`enter_stop`/`leave_stop`/`enter_vehicle`/`leave_vehicle`）。
- **`BroadcastPlayerLocation(event, kind, name, entering, target)`**：玩家进出园区/建筑/房间的Trigger盒回调里调用。先更新系统变量`system.player.zone/building/room`（进入写名字；离开时只有当前值还是这个名字才清空，
  避免"先进B后离开A"的回调顺序把刚写进去的B清掉），再把事件**只发给`target`**（被进出的那个实体自己的Script，为空=没挂脚本）。
- **`NotifyPuzzleFinished(event)`**：小游戏结束时由`UPuzzleWidget`调用，给`ApplyStartPuzzle`记下的启动脚本（`puzzleOwnerKey`）发`puzzle_result`，事件里是已展平的结果字段，脚本用`$$local.<展平名>`读。
- **资产事件**：`StartAssetScript(asset)`给新资产（含容器内容）补发`game_start`；`NotifyAssetEvent(root, bGet, result, sender)`对root及递归容器内每件资产各发`get_asset`/`lose_asset`
  （先摊平成快照再发，事件处理里的Change可能改动容器；`sender`非空发给执行Change的脚本，为空发给资产自己的脚本；屏幕左上角显示"获取/失去 xxx"，嵌套深度`assetEventDepth`上限8）；
  `NotifyAssetResult`用于没有具体资产实例的失败（只按类型发给`sender`）。
- **`UpdateSystemTime()`**：把游戏时钟写进系统变量`system.time.year/month/day/hour/minute/second`；`AForeverFrameworkActor::Tick`每帧在`Player::Tick`推进时钟后调用。

## UE相关的Change执行

Core层`Story::ApplyChange`不认识`AActor`/`APlayerController`/UMG Widget，下列Change不放进它的分派链，由`AForeverFrameworkActor::ApplyChange`统一收口后转发到本组件的`public`方法：

- **`ApplyControlChange`**：求值目标市民姓名，`UForeverPopulaceFrameworkComponent::FindOrSpawnCitizenByName`（找不到则强制生成，不看距离），`APlayerController::Possess`切换操控权，
  移动模式/Input Mapping切换由`ACitizenElement::PossessedBy`自动完成；`Citizen::ActivateStats`实例化被操控市民的动态属性（饱食度）；
  最后`UForeverBuildingFrameworkComponent::RequestFreezeUntilLodSettled()`冻结世界直到建筑近处LOD队列清空，避免玩家先掉到没有细节的地面上。玩家操控权切换只有这一条路径，由剧情脚本触发。
- **`ApplyStartPuzzle`**：求值小游戏id和可选`param`（没写就是空串，不求值），转给`AForeverPlayerController::GetPuzzleWidget()->StartPuzzle(id, param)`，记下启动脚本的稳定键。
  `param`是每次启动由剧情传入的动态参数（经`Puzzle::SetParam`→`PuzzleMod::SetParam`），跟`config.json` `puzzle_mods`的静态args是两回事，也不进存档（谜题进行中不能存档）。
- **`ApplyEnterVehicle` / `ApplyBoardVehicle` / `ApplyLeaveVehicle` / `ApplyForceLeave`**：求值载具名后转发给`UForeverTrafficFrameworkComponent`的同名方法（`ApplyForceLeave`的载具名可为空，`bForce=true`）。
- **`ApplyOpenShop`**：由执行Change的Job脚本反查所属组织，打开商店界面并让它记住该组织（购买时付款给它）。
- **`ApplyPayWage`**：该组织给被操控市民发一笔工资（`Economy::PayWage`，代扣所得税）。玩家有没有真的去上班不影响每日工资结算，这是手动打工额外拿的一份。

## Dialog展示：排队 + 分支选项

`ShowDialog(dialog, context)`把`dialog->GetDialogs()`展平成`FPendingItem`队列（`pendingQueue`）：

- **求值必须在`EnqueueDialog`里同步完成，不能延迟到显示的那一刻**——`context.local`指向调用方栈上的运行时`Event`，函数返回后就是悬垂指针。普通台词当场`EvaluateText`存成`speaker`/`content`/`label`/`background`；
  分支选项当场求值每个`Option::GetOption()`的显示文本，原始`Option`（连同嵌套`dialogs`/`changes`）整份拷贝存进`FPendingItem`供选中后使用。入队的`context`强制清空`local`（`self`/`system`指向长期存活的Script，不受影响），
  代价是分支选项的嵌套内容没法再引用`local.`前缀变量；真需要时得把值提前求值烘焙进某个Script的变量池，而不是延长Event的生命周期。
- **condition**：`Dialog`自己的`condition`为假则整段不展示（求值时机是入队这一刻，所以同一个里程碑里排在前面的changes已生效）；分支的`Option::GetCondition()`为假的选项不出现，全被过滤则整个分支跳过。
  因此`SelectDialogOption(index)`的`index`是过滤后选项数组的下标。
- 队列为空时入队立刻显示；非空则排到队尾——多个Script同时广播`game_start`都命中对话时会一条条弹出，不互相覆盖。
- `AdvanceDialog()`关闭当前台词、推进到下一项；带`label`的台词播完后广播`speaking_finish`（先弹出队首再广播再显示下一项，广播命中的里程碑若又弹对话会排到队尾）。
- `SelectDialogOption(index)`执行被选中`Option`的`GetChanges()`，把它的`GetDialogs()`插到队头紧接着播放（多个嵌套Dialog保持相对顺序，`insertCursor`递增）。队首不是分支或`index`越界时no-op。
- `ShowCurrentItem()`按队首是台词还是分支调`ShowLine`/`ShowOptions`，并切`UIOnly`输入模式（避免Pawn的Enhanced Input同时响应，例如空格同时绑了Jump和"推进对话"）；
  `widgetToFocus`必须是当前显示的Widget，因为空格推进走`NativeOnKeyDown`需要键盘焦点（见`USectionSpeakingWidget.h`）。
  队列清空时隐藏两个Widget、切回`GameOnly`并显式`SetFocusToGameViewport()`——UI点击会把Slate焦点转到按钮上，光改`SetInputMode`不会抢回来，鼠标隐藏后Look收不到鼠标移动。
  对话展示期间`MeetOptionWidget`临时`Collapsed`，播完恢复`Visible`。
- `IsDialogActive()`（队列非空）为真时不允许存档，对话队列的中间状态不进存档。

### 后台对话

对话list里普通台词可写`"background": 秒数`(大于0)：屏幕上方的`UBackgroundSpeakingWidget`显示，不抢操控（不切输入模式、不显示鼠标、不藏MeetOption），只靠计时结束。和前台是两个独立显示槽，可同时显示。

- **等待规则**：后台等前台，前台不等后台（f1 f2 b3 f3：f2结束后b3和f3同时出现）。
- **实现**：后台条目照常按序排在`pendingQueue`；`ShowCurrentItem`开头把队首连续的后台条目转进`backgroundQueue`（能排到队首说明前面的前台都播完了），再显示下一条前台。整个list都是后台条目时不会弹出前台界面。
- **计时**：`ShowCurrentBackground`用世界计时器`backgroundTimer`，跟随时间缩放，暂停时冻结；到时`AdvanceBackground`弹出队首、先显示下一条再广播`speaking_finish`（避免广播命中的里程碑往`backgroundQueue`追加台词时重复启动计时）。控件没配置时照样计时推进队列。
- 后台对话不算`IsDialogActive()`（不阻止存档），存档时丢弃；`TogglePause`也不因它拒绝暂停。

## 计时器

`create_timer`/`remove_timer`由`AForeverFrameworkActor::ApplyChange`转发到`ApplyCreateTimer`/`ApplyRemoveTimer`，`FireExpiredTimers(deltaSeconds)`每帧在`Story::Tick`之后调用（计时器本体和存档见`Core/story/story.md`"计时器"）。

- **归属**：计时器按归属脚本的稳定键（`FindScriptKey`/`FindScriptByKey`，键与`ScriptSaveCodec::CollectScripts`一致）存放，到时只发`TimeUpEvent`给归属脚本，不广播；归属对象不在了就丢弃。
  `ResolveTimerOwner`按`category`（空=self；`global`/`main`/`citizen`/`vehicle`/`door`/`elevator`/`zone`/`building`/`room`）+`label`找目标脚本，建筑/房间/园区的`label`写名字或地址都行。
  岗位/组织脚本数量大（几千），所以`FindScriptKey`/`FindScriptByKey`对`org:`/`job:`键先走不分配的直接扫描/直接解析，再退回`CollectScripts`（它每次重建并排序全部脚本，几千个计时器同时触发会卡很久）。
- **`mode`**：`absolute`（默认，`time`是完整日期，过去时间忽略并警告）、`relative`（当前时间加偏移）、`real`（现实秒数，`Story::CreateRealTimer`，由`AdvanceRealTimers`推进）、`next`（日期是相对天数，钟点是绝对钟点，钟点早于当前钟点就再晚一天）。
  relative/real/next的`time`用`ParseTimerOffset`解析为"天数+钟点"，支持`[Y年]M月D日[ H:M:S]`、`YYYY-MM-DD[ H:M:S]`、纯`H:M:S`；日期按相对天数算（年月日减0年1月1日，日期不限范围，1月40日=39天），所以不用`Time`类解析（它会校验日期范围）。
- 每帧最多处理`kMaxStoryTimersPerFrame`(16)个到时计时器，8点/9点这类几千个岗位脚本同时到时，多出的留到后面几帧。
- `remove_timer`只删执行它的脚本自己名下的同名计时器。

## 依赖关系

- 依赖：`Core/story/story.h`/`script.h`/`dialog.h`/`change.h`/`event.h`/`script_save.h`、`Core/society/society.h`/`organization.h`/`job.h`/`economy.h`、`Core/populace/populace.h`/`citizen.h`/`scheduler.h`、
  `Core/traffic/traffic.h`/`vehicle.h`、`Core/map/map.h`/`door.h`/`cabin.h`/`zone.h`/`building.h`/`room.h`、`Core/player/player.h`/`asset.h`、`Core/common/implement.h`（`PostImplement`）、
  `Framework/ForeverFrameworkActor.h`（聚合各domain指针+`ApplyChange`统一入口）、`Framework/ForeverPopulaceFrameworkComponent.h`（`FindOrSpawnCitizenByName`）、
  `Framework/ForeverBuildingFrameworkComponent.h`（`RequestFreezeUntilLodSettled`）、`Framework/ForeverTrafficFrameworkComponent.h`（载具Change）、`Framework/ForeverAssetFrameworkComponent.h`（按名查资产Actor）、
  `UI/SectionSpeakingWidget.h`/`BackgroundSpeakingWidget.h`/`SectionOptionWidget.h`/`MeetOptionWidget.h`/`PuzzleWidget.h`/`ShopWidget.h`、`Player/ForeverPlayerController.h`（持有上述Widget）。
- 被依赖：`Framework/ForeverFrameworkActor.cpp`（`ApplyChange`把各类Change转发进来；广播天气/全局消息/计时器/系统时间）、`UI/MeetOptionWidget.h`（`OptionDialog`）、`Player/ForeverWeaponComponent.h`（`BroadcastCitizenDecease`）、
  `UI/SectionSpeakingWidget.h`/`SectionOptionWidget.h`（`AdvanceDialog`/`SelectDialogOption`）、`UForeverPopulaceFrameworkComponent`（`BroadcastNpcArrive`）、`UForeverTrafficFrameworkComponent`（`BroadcastToVehicle`）、
  建筑/房间/园区的Trigger盒（`BroadcastPlayerLocation`）、`UPuzzleWidget`（`NotifyPuzzleFinished`）。

## 待办/后续阶段

- 分支选项的嵌套内容不能引用`local.`前缀变量（见"Dialog展示"一节）；暂无脚本用到，真需要时把值烘焙进Script变量池。

## 跳时间相关
- `ResolveJumpTarget(change, context, out)`：jump_time目标时刻(和create_timer共用匿名函数`ResolveTimerTarget`)，返回是否需要跳。
- `DrainExpiredTimers(maxIterations)`：把到期的game计时器全部发完(不推进real计时器)，供跳时间每一步使用；`FireExpiredTimers`也走同一个`FireTimerBatch`。
