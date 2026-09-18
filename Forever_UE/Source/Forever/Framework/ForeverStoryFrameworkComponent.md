# ForeverStoryFrameworkComponent.h / .cpp

阶段2骨架（空实现）升级为阶段4 Story域的UE层接线点。职责边界：`Core/story`的`Script`/
`Story`只做纯C++的匹配/求值，不感知UE；这个组件负责"游戏开始时广播一次GameStart"+"把匹配出的
Dialog/Change用`GEngine::AddOnScreenDebugMessage`打印在屏幕左上角"这两件跟UE强相关的事。

## 生命周期

`Init(Story*)`由`AForeverFrameworkActor::EnsureMapGenerated()`在创建完`Story`并调用完
`story->Init()`之后调用一次（只存指针，不持有所有权，和其它Framework组件持有`Map*`的方式一致）。
`BroadcastGameStart()`紧接着被同一处代码调用一次——**不是**放在这个组件自己的`BeginPlay()`里，
因为`AForeverFrameworkActor::BeginPlay()`是先调`Super::BeginPlay()`（级联触发所有子组件自己的
`BeginPlay`）、再调`EnsureMapGenerated()`（这里面才会创建`Story`并调`storyFramework->Init(story)`)
——如果广播逻辑放在这个组件自己的`BeginPlay`里，届时`story`指针还是空的。

## 展示逻辑

`Story::BroadcastGameStart`的回调（同步执行，见`Core/story/story.md`"BroadcastGameStart用回调"
一节）里区分`ScriptAction`的两个分支：

- `const Dialog*`：`GetDialogs()`拿到`Section`拷贝列表（延迟求值约定见
  `Dependence/story/dialog.md`），分支选项段（`IsBranch()`为true）没有玩家交互，默认取
  `GetOptions()[0]`的文本打印（对应用户"没有交互方式，默认选第一个选项"的要求）；普通台词段
  调用`EvaluateText(context)`后取`GetSpeaking()`，有发言者就拼"发言者：内容"，没有就只打内容
  （`test.json`的`speaker`是空字符串，走后一种）。
- `const Change*`：转调`story->ApplyChange(change, context)`执行（当前只有`SetValueChange`
  真正生效），再打印一行`[变化] <类型名>`——这次不展示变化的具体字段内容，只标注类型，字段级
  的展示留到该类型需要更丰富调试信息时再加。

所有文本走`GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, ...)`，和
`ForeverZoneFrameworkComponent.cpp`已有的写法保持一致（黄色、5秒、`-1`表示不指定固定key、
每条消息独立）。

## 依赖关系

- 依赖：`Core/story/story.h`/`script.h`、`Dependence/story/dialog.h`/`change.h`、
  `Engine/Engine.h`（`GEngine`）。
- 被谁依赖：`Framework/ForeverFrameworkActor.cpp`。

## 待办/后续阶段

- Dialog分支选项的真正玩家交互UI（阶段5 UMG接入时再做，这次的"默认选第一个"只是占位）。
- Change的字段级展示（目前只打印类型名）。
