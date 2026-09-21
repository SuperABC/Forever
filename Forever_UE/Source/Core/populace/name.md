# name.h / name.cpp

## 职责

`Name`是某个具体`NameMod`实例的薄包装——构造时向`NameFactory`要一个实例（按id，如
`"chinese"`），把`GetType`/`GetName`/`GetSurname`/`GenerateName`（两个重载）原样转发出去，
析构时交还给Factory销毁。写法逐字照抄`Core/map/terrain.h/.cpp`的`Terrain`类。**这个id
不再是`Populace::InitNames()`里硬编码的字面量**——`NameFactory`这次改成和
`RoadnetFactory`同一个"单选"模式（`SetConfig`/`GetName`），`config.json`的
`"name_mods"`数组列出哪个id就用哪个，见`populace.md`"InitNames"一节。

## 补做原因：架构一致性

最初迁移Name这个concept时（见`populace.md`"姓名生成"一节）图省事，直接让`Populace`持有
`NameMod* nameMod`，跳过了Core层包装类这一步——这和其它concept的既有约定不一致：`Terrain`/
`Roadnet`/`Zone`/`Building`（Map域）、`Script`（Story域）等聚合类，从来不直接持有/调用
`<Concept>Mod*`，一律通过一个Core层的薄包装类（`Terrain`/`Script`等）转发。`Populace`直接摸
`NameMod*`是这条约定唯一的例外，被指出后按同一个模式补上了这个类，`Populace`现在只持有
`Name* name`，见`populace.md`同一节的"架构修正"说明。

## `ReserveName`：给主线剧情.script的`name_reserve`占位（新增）

`reserve`（`std::unordered_set<std::string>`成员）+`ReserveName(name)`——照抄老工程
`Name::ReserveName`的思路（`E:\Projects\Forever_UE`的`Name`类），但**不**引入老工程
`roll`那一半批内去重（只要求"不能和脚本占位名撞"，不要求"生成的市民之间互不重名"，
维持`populace.md`早先的决定）。`GenerateName`两个重载内部改成"生成一个候选，撞上
`reserve`就重试，最多`kMaxReserveRetryAttempts`（1000）次"——**仍然撞上就直接
`THROW_EXCEPTION(DeadLoopException, ...)`**，不能静默返回一个撞名的结果；
`common/error.h`已有的`DeadLoopException`语义正好贴合"重试循环没能在预期内终止"这个
场景，不需要新增异常类型。这个异常和`AForeverFrameworkActor::
ValidateMainStoryDependencies()`的校验失败一起，统一交给`BeginPlay()`的`try/catch`
处理（打日志+退出游戏）。方法签名不变，两个重载都是`const`，`reserve`只读不写，
`const`语义不受影响。

调用方是`Populace::InitNames()`——`new Name(...)`之后立刻遍历主线剧情`.script`的
`name_reserve`字段（`Script::GetNameReserve(Config::GetMainStoryScriptPath())`）逐个
调`ReserveName`，早于`GenerateCitizens(target)`生成任何citizen，见
`Core/story/script.md`"主线剧情.script新增三个顶层字段"一节、`populace.md`
"InitNames"一节。

## 依赖关系

- 依赖：`name_mod.h`、`name_factory.h`、`common/error.h`（`NullPointerException`/
  `DeadLoopException`）。
- 被谁依赖：`Source/Core/populace/populace.h/.cpp`（`Populace::InitNames()`构造
  `Name*`+调`ReserveName`，`GenerateCitizens()`调`name->GenerateName(...)`/
  `name->GetSurname(...)`）。
