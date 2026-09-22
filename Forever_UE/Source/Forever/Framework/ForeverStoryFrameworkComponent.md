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
- `const Change*`：这次重构后统一转发给`AForeverFrameworkActor::ApplyChange(*changePtr,
  context)`（`framework`是`onActions`回调顶部已经`Cast`出来的`AForeverFrameworkActor*`），
  不再在这个组件里自己写`dynamic_cast<const ChangeControlChange*>`/`DebugPrintChange`
  这些分支——`ApplyChange`是这次新增的统一Change消费入口，`populace`/`society`两个Tick
  回调也在同一个入口消费Change，三处重复的dispatch逻辑收口成一份，详见
  `ForeverFrameworkActor.md`"统一的Change消费入口：`ApplyChange`"一节。`ChangeControlChange`
  在`ApplyChange`内部还是会转发回这个组件自己的`ApplyControlChange`（见下一节，这次改成
  `public`），行为不变。不管走哪条路径，最后都打印一行`[变化] <类型名>`——这次不展示变化的
  具体字段内容，只标注类型，字段级的展示留到该类型需要更丰富调试信息时再加。

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
处理。这个函数这次从`private`改成`public`——`AForeverFrameworkActor::ApplyChange`统一收口
所有Change的消费入口后，`ChangeControlChange`分支需要从那里转发调用到这里（见
`ForeverFrameworkActor.md`），不再只被这个组件自己的`onActions`回调调用：

1. `EvaluateExpression(change->GetName(), context)`求出目标市民姓名（`GetName()`现在是
   `std::string`类型的DSL源码文本，不是预先解析好的`Expression`对象，`EvaluateExpression`
   现场`Parse`+求值一步到位，原因见`Dependence/story/change.md`"字段类型是`std::string`"
   一节；求值结果`ToString`+`UTF8_TO_TCHAR`转`FString`）。
2. 通过`GetOwner()`转成`AForeverFrameworkActor`，取`GetPopulaceFramework()`，调用
   `UForeverPopulaceFrameworkComponent::FindOrSpawnCitizenByName(name)`——按姓名查找已经
   在场景里的`ACitizenElement`，找不到则强制生成一个（不看玩家距离，见
   `ForeverPopulaceFrameworkComponent.md`），拿不到就打一条`Warning`日志放弃。
3. `UGameplayStatics::GetPlayerController(GetWorld(), 0)->Possess(target)`——把玩家的
   操控权切给这个市民对应的`ACitizenElement`，真正的移动模式切换/Input Mapping增删由
   `ACitizenElement::PossessedBy`（继承自`AForeverCharacter::PossessedBy`）自动完成，
   这个函数本身只负责"找到目标+发起Possess"。
4. `framework->GetBuildingFramework()->RequestFreezeUntilLodSettled()`——游戏刚开始
   （`test.script`的`game_start`milestone配合`EmptyScript::WrapScript`第一次切换控制权
   就发生在这时候）附近building的近处LOD（楼层/房间细节）可能还没排队建完，玩家/市民会
   先掉到还没生成细节的地面上，等建筑加载完才落地。每次切换控制权之后请求冻结世界（UE
   时间倍率归零，物理/移动全部停摆）直到所有building的LOD切换队列清空再自动恢复，见
   `ForeverBuildingFrameworkComponent.md`"冻结世界直到LOD切换队列清空"一节。

**T键这次改成不再切换控制权**（原来`AForeverCharacter::SwitchControlledCitizen`会
`Possess`附近随便一个市民，现在改名`LogNearbyCitizenRelationships`，只把附近市民的
`acquaintances`/`experiences`打到log，见`Player/ForeverCharacter.md`/
`Element/CitizenElement.md`"T键：输出附近市民的人际关系数据"一节）——玩家控制市民的
切换目前**只有这一条`ChangeControlChange`路径**，由剧情脚本触发、目标是"剧情指定姓名
的市民"（这次的`test.json`配合`EmptyScript::WrapScript`，用的是"随机挑一个citizen"，见
`Dependence/story/script_mod.md`"典型用法"一节）。

## 依赖关系

- 依赖：`Core/story/story.h`/`script.h`、`Dependence/story/dialog.h`/`change.h`
  （`ChangeControlChange`）、`Core/common/implement.h`（`PostImplement`）、
  `Framework/ForeverFrameworkActor.h`（`GetMap`/`GetPopulace`/`GetSociety`/`GetIndustry`/
  `GetTraffic`/`GetPlayer`/`GetPopulaceFramework`/`GetBuildingFramework`/
  `ApplyChange`——`onActions`回调的`Change`分支转发给它）、`Framework/
  ForeverPopulaceFrameworkComponent.h`（`FindOrSpawnCitizenByName`）、`Framework/
  ForeverBuildingFrameworkComponent.h`（`RequestFreezeUntilLodSettled`）、
  `Element/CitizenElement.h`、`Engine/Engine.h`（`GEngine`）、`Kismet/GameplayStatics.h`
  （`GetPlayerController`）。
- 被谁依赖：`Framework/ForeverFrameworkActor.cpp`（`AForeverFrameworkActor::ApplyChange`
  处理`ChangeControlChange`时转发调用这个组件的`ApplyControlChange`，这次从`private`
  改成`public`）。

## 待办/后续阶段

- Dialog分支选项的真正玩家交互UI（阶段5 UMG接入时再做，这次的"默认选第一个"只是占位）。
- Change的字段级展示（目前只打印类型名）。
