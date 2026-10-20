# name.h / name.cpp

## 职责

`Name`是某个具体`NameMod`实例的薄包装——构造时向`NameFactory`要一个实例（按id，如
`"name_chinese"`），把`GetType`/`GetName`/`GetSurname`/`GenerateName`（两个重载）原样
转发出去，析构时交还给Factory销毁。写法照抄`Core/map/terrain.h/.cpp`的`Terrain`类
——`Populace`只通过这层转发访问取名算法，不直接持有`NameMod*`，和`Terrain`/
`Roadnet`/`Zone`/`Building`/`Script`等聚合类"上层只操作Core层包装类"的既有架构一致。

`NameFactory`是`RoadnetFactory`同一个"单选"模式（`SetConfig`/`GetName`）——Name这个
concept只需要唯一一个"当前生效"的取名算法给`Populace`用，不是Terrain/Zone/Building
那种按`GetPriority()`/权重多mod叠加。`config.json`的`"name_mods"`数组列出哪个id就用
哪个，见`populace.md`"InitNames"一节。

## 关键设计

- **`ReserveName`**：`reserve`（`std::unordered_set<std::string>`成员）+
  `ReserveName(name)`，要求"不能和脚本占位名撞"（不要求"生成的市民之间互不重名"）。
  `GenerateName`两个重载内部是"生成一个候选，撞上`reserve`就重试，最多
  `kMaxReserveRetryAttempts`（1000）次"——仍然撞上就直接`THROW_EXCEPTION(
  DeadLoopException, ...)`，不能静默返回一个撞名的结果；`common/error.h`已有的
  `DeadLoopException`语义正好贴合"重试循环没能在预期内终止"这个场景。这个异常和
  `AForeverFrameworkActor::ValidateMainStoryDependencies()`的校验失败一起，统一
  交给`BeginPlay()`的`try/catch`处理（打日志+退出游戏）。方法签名不变，两个重载都是
  `const`，`reserve`只读不写，`const`语义不受影响。

  调用方是`Populace::InitNames()`——`new Name(...)`之后立刻遍历主线剧情`.script`的
  `name_reserve`字段（`Script::GetNameReserve(Config::GetMainStoryScriptPath())`）
  逐个调`ReserveName`，早于`GenerateCitizens(target)`生成任何citizen，见
  `Core/story/script.md`"主线剧情.script新增三个顶层字段"一节、`populace.md`
  "InitNames"一节。

## 依赖关系

- 依赖：`name_mod.h`、`name_factory.h`、`common/error.h`（`NullPointerException`/
  `DeadLoopException`）。
- 被谁依赖：`Source/Core/populace/populace.h/.cpp`（`Populace::InitNames()`构造
  `Name*`+调`ReserveName`，`GenerateCitizens()`调`name->GenerateName(...)`/
  `name->GetSurname(...)`）。

## 待办/后续阶段

- `roll`（同一轮生成内部互不撞名）未迁移——姓名生成算法是~200姓氏x最多590个给定名
  字符的真实词库，组合数量级足够大，几百个citizen之间实际撞名的概率本来就很低，如果
  以后确实需要严格去重可以再补。
