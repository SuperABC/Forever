# ForeverStoryFrameworkComponent

## 职责

Story域的UE层接线点。`Core/story`的`Script`/`Story`只做纯C++的匹配/求值，不感知UE；这个
组件负责把匹配出的`ScriptAction`接到真正的UE表现上——`Dialog`交给`SectionSpeakingWidget`/
`SectionOptionWidget`展示（带排队），`Change`统一转发给`AForeverFrameworkActor::ApplyChange`。
同时承担三类UE相关、Core层做不到的Change执行（切换玩家操控、开小游戏、上车）和三个事件
广播入口（开局/选项点击/市民死亡）。

## 生命周期

`Init(Story*)`由`AForeverFrameworkActor::EnsureStoryGenerated()`在创建完`Story`并调用完
`story->Init()`之后调用一次（只存指针，不持有所有权）。`BroadcastGameStart()`紧接着被同一
处代码调用一次——不放在这个组件自己的`BeginPlay()`里，因为`AForeverFrameworkActor::
BeginPlay()`先级联触发所有子组件的`BeginPlay`、再依次调用7个`Ensure*Generated()`，届时
`story`指针还是空的。

## 三个事件广播入口

- **`BroadcastGameStart()`**：游戏开始时调用一次。除了`story->BroadcastGameStart`本身，还
  要遍历广播给所有"独立持有一份Script、但自己不是Story主线"的对象，否则它们各自的
  `game_start` milestone永远不会被触发：`Society`下每个`Organization`/`Job`各自的Script；
  每个`Citizen`独占持有的`Scheduler`的Script（市民的对话选项如"敲门"全靠这条广播才会被
  加上）；每辆预置车辆的Script（"上车"选项）；`map->GetAllDoors()`里`GetScript()!=nullptr`
  的门（纯自动门没有Script，跳过）。这些广播共用`broadcastOne`辅助lambda和统一的
  `onActions`回调（`ProcessScriptActions`）。
- **`OptionDialog(name, option)`**：玩家点击`MeetOption`选项时调用，构造一个
  `OptionDialogEvent`。依次匹配主线剧情Script，再按`name`分别尝试查找市民（匹配其
  `Scheduler`的Script）、车辆（`Traffic::FindVehicleByName`）、门（`Map::FindDoorByName`）
  三类目标，命中哪个就额外匹配哪个的Script——一个`name`不会同时是其中两类，三个分支互不
  影响。
- **`BroadcastCitizenDecease(name, reason)`**：武器系统命中Citizen致死时调用，构造一个
  `CitizenDeceaseEvent`。只匹配主线剧情Script，不像`OptionDialog`那样额外匹配死者自己的
  Scheduler Script——人已经死了，它自己的日程脚本不再有意义。

三者都复用`ProcessScriptActions`（匹配结果里的`Dialog`转发给`ShowDialog`排队展示，
`Change`转发给`AForeverFrameworkActor::ApplyChange`，不在屏幕上打任何调试消息——早期每个Change打一行
`[变化] <类型名>`，会挤掉其它调试信息，已删除；要看执行了什么可以在脚本里加`debug_print`），以及现场构造的`PostImplement`（供`WrapScript`反查
Core状态，如"随机挑一个citizen"，生命周期只覆盖这次同步广播，不长期持有）。

## 三个UE相关的Change执行

Core层`Story::ApplyChange`不认识`AActor`/`APlayerController`/UMG Widget，以下三类Change
故意不放进它的`dynamic_cast`分派链，由`AForeverFrameworkActor::ApplyChange`统一收口后
转发到这个组件的对应`public`方法：

- **`ApplyControlChange`**：求值`change->GetName()`拿到目标市民姓名，调用
  `UForeverPopulaceFrameworkComponent::FindOrSpawnCitizenByName`（按姓名查找已在场景里的
  `ACitizenElement`，找不到则强制生成，不看玩家距离），再用`APlayerController::Possess`
  切换操控权。真正的移动模式切换/Input Mapping增删由`ACitizenElement::PossessedBy`
  （继承自`AForeverCharacter::PossessedBy`）自动完成。切换之后额外调用
  `UForeverBuildingFrameworkComponent::RequestFreezeUntilLodSettled()`——切换操控权这一刻
  附近building的近处LOD可能还没排队建完，玩家会先掉到还没生成细节的地面上，冻结世界直到
  LOD切换队列清空再自动恢复能避免这个观感问题。
- **`ApplyStartPuzzle`**：求值`change->GetPuzzle()`拿到小游戏id，转发给
  `AForeverPlayerController::GetPuzzleWidget()->StartPuzzle(id)`。
- **`ApplyEnterVehicle`**：求值`change->GetVehicle()`拿到载具名字，转发给
  `UForeverTrafficFrameworkComponent::ApplyEnterVehicle`（真正的查找/隐藏原pawn/Possess
  逻辑在那个组件里，和`ApplyControlChange`转发给PopulaceFramework同一个分工）。

玩家操控权切换**只有`ApplyControlChange`这一条路径**，由剧情脚本触发——没有独立于Story
系统之外的按键能直接切换操控的市民。

## Dialog展示：排队 + 分支选项

`ShowDialog(dialog, context)`不直接打印，而是把`dialog->GetDialogs()`展平成
`FPendingItem`队列（`pendingQueue`）：

- **求值必须在`EnqueueDialog`里同步完成，不能延迟到显示的那一刻**——传入的`context.local`
  指向调用方栈上的运行时`Event`（`GameStartEvent`/`OptionDialogEvent`等），函数返回后就是
  悬垂指针。普通台词当场`EvaluateText`存成`speaker`/`content`字符串；分支选项当场求值每个
  `Option::GetOption()`的显示文本，原始`Option`本身（连同它引用的嵌套`dialogs`/`changes`）
  整份拷贝存进`FPendingItem`供选中后使用，存入队列的`context`强制清空`local`（`self`/
  `system`不变，它们指向长期存活的`Script*`）——代价是分支选项自己的嵌套内容没法再引用
  `local.`前缀变量，这次范围内没有用到这个能力。
- 队列为空时入队立刻显示；非空则排到队尾——多个Script同时广播`game_start`都命中对话
  milestone时，会排队一条条弹出，不互相覆盖。
- `AdvanceDialog()`（`SectionSpeaking`的`ClickCatcher`点击触发）关闭当前台词、推进到下一项。
- `SelectDialogOption(index)`（`SectionButton`点击触发）执行被选中`Option`的`GetChanges()`
  （转发给`ApplyChange`），把它的`GetDialogs()`插到队头紧接着播放（不是排到队尾），多个
  嵌套Dialog之间保持相对插入顺序。
- `ShowCurrentItem()`按队首是台词还是分支分别调用`SectionSpeakingWidget::ShowLine`/
  `SectionOptionWidget::ShowOptions`，并把输入模式切成`UIOnly`（避免Pawn的Enhanced Input
  同时响应游玩动作，例如空格键如果同时绑了Jump和"推进对话"会导致按键同时触发两件事）；
  队列清空时隐藏两个Widget、把输入模式切回`GameOnly`并显式调用一次
  `SetFocusToGameViewport()`（UI点击会把Slate焦点转移到按钮上，光改`SetInputMode`不会
  自动把焦点抢回游戏视口，否则鼠标隐藏后Look这个Enhanced Input Action仍然收不到鼠标移动
  事件）。对话展示期间会把`MeetOptionWidget`临时隐藏，播完恢复可见——避免"靠近市民能问
  什么"的列表和剧情对话同屏抢注意力/抢点击。

## 广播给所有Script

`BroadcastToAllScripts(event, postImplement, includeMain)`把一个Event广播给组织/岗位/市民Scheduler/车辆/门的全部
Script(`includeMain`为true时连主线也算)，匹配结果交给`ProcessScriptActions`。两处在用：
- `BroadcastGameStart`：主线走`Story::BroadcastGameStart`，其余用这个helper(`includeMain=false`)。
- `BroadcastGlobalMessage`：执行`global_message`这个Change时由`AForeverFrameworkActor::ApplyChange`调用，
  构造`GlobalMessageEvent`广播给全部Script(含主线)。嵌套深度(`globalMessageDepth`)超过`kMaxGlobalMessageDepth`(8)
  时丢弃并`UE_LOG`警告，防止两个里程碑互相发消息无限递归。

## 依赖关系

- 依赖：`Core/story/story.h`/`script.h`/`dialog.h`/`change.h`/`event.h`、
  `Core/society/society.h`/`organization.h`/`job.h`、`Core/populace/populace.h`/
  `citizen.h`/`scheduler.h`、`Core/traffic/traffic.h`、`Core/map/door.h`/`map.h`、
  `Core/common/implement.h`（`PostImplement`）、`Framework/ForeverFrameworkActor.h`
  （聚合7个domain指针+`ApplyChange`统一入口）、`Framework/
  ForeverPopulaceFrameworkComponent.h`（`FindOrSpawnCitizenByName`）、`Framework/
  ForeverBuildingFrameworkComponent.h`（`RequestFreezeUntilLodSettled`）、`Framework/
  ForeverTrafficFrameworkComponent.h`（`ApplyEnterVehicle`）、`UI/SectionSpeakingWidget.h`/
  `SectionOptionWidget.h`/`MeetOptionWidget.h`/`PuzzleWidget.h`、
  `Player/ForeverPlayerController.h`（持有上述Widget）。
- 被谁依赖：`Framework/ForeverFrameworkActor.cpp`（`ApplyChange`把
  `ChangeControlChange`/`StartPuzzleChange`/`EnterVehicleChange`三类Change转发到这个
  组件对应的`public`方法）、`UI/MeetOptionWidget.h`（选项点击转发给`OptionDialog`）、
  `Player/ForeverWeaponComponent.h`（击杀市民转发给`BroadcastCitizenDecease`）、
  `UI/SectionSpeakingWidget.h`/`SectionOptionWidget.h`（点击分别转发给`AdvanceDialog`/
  `SelectDialogOption`）。

## 待办/后续阶段

