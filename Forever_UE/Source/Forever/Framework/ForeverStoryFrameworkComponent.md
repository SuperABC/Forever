# ForeverStoryFrameworkComponent.h / .cpp

阶段2骨架（空实现）升级为阶段4 Story域的UE层接线点。职责边界：`Core/story`的`Script`/
`Story`只做纯C++的匹配/求值，不感知UE；这个组件负责"游戏开始时广播一次GameStart"+"把匹配出的
Dialog/Change用`GEngine::AddOnScreenDebugMessage`打印在屏幕左上角"这两件跟UE强相关的事。

## 生命周期

`Init(Story*)`由`AForeverFrameworkActor::EnsureStoryGenerated()`在创建完`Story`并调用完
`story->Init()`之后调用一次（只存指针，不持有所有权，和其它Framework组件持有`Map*`的方式一致）。
`BroadcastGameStart()`紧接着被同一处代码调用一次——**不是**放在这个组件自己的`BeginPlay()`里，
因为`AForeverFrameworkActor::BeginPlay()`是先调`Super::BeginPlay()`（级联触发所有子组件自己的
`BeginPlay`）、再依次调用7个`Ensure*Generated()`（`EnsureStoryGenerated()`排在最后，这里面
才会创建`Story`并调`storyFramework->Init(story)`)——如果广播逻辑放在这个组件自己的
`BeginPlay`里，届时`story`指针还是空的。

## 展示逻辑

`Story::BroadcastGameStart`的回调（同步执行，见`Core/story/story.md`"BroadcastGameStart用回调"
一节）里区分`ScriptAction`的两个分支：

- `const Dialog*`：`GetDialogs()`拿到`Section`拷贝列表（延迟求值约定见
  `Dependence/story/dialog.md`），分支选项段（`IsBranch()`为true）没有玩家交互，默认取
  `GetOptions()[0]`的文本打印（对应用户"没有交互方式，默认选第一个选项"的要求）；普通台词段
  调用`EvaluateText(context)`后取`GetSpeaking()`，有发言者就拼"发言者：内容"，没有就只打内容
  （`test.json`的`speaker`是空字符串，走后一种）。
- `const Change*`：先`dynamic_cast<const ChangeControlChange*>`判断是不是"切换控制"这个
  特殊类型——是的话转调这个组件自己的`ApplyControlChange`（见下一节），不经过
  `Story::ApplyChange`；不是的话才走`story->ApplyChange(change, context)`（当前只有
  `SetValueChange`真正生效，`PlaceHolderChange`如果没被`ScriptMod::WrapScript`替换掉，也会
  落到这个分支，只打一条"未实现"日志）。不管走哪条路径，最后都打印一行`[变化] <类型名>`
  ——这次不展示变化的具体字段内容，只标注类型，字段级的展示留到该类型需要更丰富调试信息时
  再加。

所有文本走`GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, ...)`，和
`ForeverZoneFrameworkComponent.cpp`已有的写法保持一致（黄色、5秒、`-1`表示不指定固定key、
每条消息独立）。

## `PostImplement`：`BroadcastGameStart`现场构造，只活这一次广播

`Story::BroadcastGameStart`这次多了一个`PostHandle* post`参数（供`ScriptMod::WrapScript`
反向查询Core状态，见`Core/story/story.md`），这个组件的`BroadcastGameStart()`负责现场构造
一个`PostImplement`（`Core/common/implement.h`）传进去：从`GetOwner()`（就是
`AForeverFrameworkActor`）取`GetMap()`/`GetPopulace()`/`GetSociety()`/`GetIndustry()`/
`GetTraffic()`/`GetPlayer()`这六个指针，加上自己已经持有的`story`，一共聚合Core的全部
7个domain指针。`PostImplement`是一个栈上局部对象，生命周期只需要覆盖这次同步广播（`Story::
BroadcastGameStart`的回调是同步执行的，见`Core/story/story.md`"用回调而不是直接返回"一节），
不用像`story`那样长期持有。

## `ApplyControlChange`：`ChangeControlChange`不走`Story::ApplyChange`，在这一层直接处理

`change_control`类型的变化（切换玩家操控的市民，见`Dependence/story/change.md`"切换控制"
一节）**故意不**放进`Story::ApplyChange`的`dynamic_cast`分派链——`Core/story`是纯C++层，
不知道`AActor`/`APlayerController`的存在，没法执行"把玩家操控权切给某个Actor"这件事。这次
在`UForeverStoryFrameworkComponent`（UE层，天然能拿到`AActor`/`APlayerController`）拦截
处理：

1. `change->GetName().EvaluateValue(context)`求出目标市民姓名（`ToString`+`UTF8_TO_TCHAR`
   转`FString`）。
2. 通过`GetOwner()`转成`AForeverFrameworkActor`，取`GetPopulaceFramework()`，调用
   `UForeverPopulaceFrameworkComponent::FindOrSpawnCitizenByName(name)`——按姓名查找已经
   在场景里的`ACitizenElement`，找不到则强制生成一个（不看玩家距离，见
   `ForeverPopulaceFrameworkComponent.md`），拿不到就打一条`Warning`日志放弃。
3. `UGameplayStatics::GetPlayerController(GetWorld(), 0)->Possess(target)`——把玩家的
   操控权切给这个市民对应的`ACitizenElement`，真正的移动模式切换/Input Mapping增删由
   `ACitizenElement::PossessedBy`（继承自`AForeverCharacter::PossessedBy`）自动完成，
   这个函数本身只负责"找到目标+发起Possess"。

这条路径和`AForeverCharacter::SwitchControlledCitizen`（T键）最终都是调用某个
`AController::Possess(ACitizenElement*)`，但触发方式不同：T键由玩家主动触发、目标是"附近
随便一个市民"；`ChangeControlChange`由剧情脚本触发、目标是"剧情指定姓名的市民"（这次的
`test.json`配合`EmptyScript::WrapScript`，用的是"随机挑一个citizen"，见
`Dependence/story/script_mod.md`"典型用法"一节）。

## 依赖关系

- 依赖：`Core/story/story.h`/`script.h`、`Dependence/story/dialog.h`/`change.h`
  （`ChangeControlChange`）、`Core/common/implement.h`（`PostImplement`）、
  `Framework/ForeverFrameworkActor.h`（`GetMap`/`GetPopulace`/`GetSociety`/`GetIndustry`/
  `GetTraffic`/`GetPlayer`/`GetPopulaceFramework`）、`Framework/
  ForeverPopulaceFrameworkComponent.h`（`FindOrSpawnCitizenByName`）、
  `Element/CitizenElement.h`、`Engine/Engine.h`（`GEngine`）、
  `Kismet/GameplayStatics.h`（`GetPlayerController`）。
- 被谁依赖：`Framework/ForeverFrameworkActor.cpp`。

## 待办/后续阶段

- Dialog分支选项的真正玩家交互UI（阶段5 UMG接入时再做，这次的"默认选第一个"只是占位）。
- Change的字段级展示（目前只打印类型名）。
