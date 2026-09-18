# script.h / script.cpp

阶段4新落地（新工程首次迁移这个文件），结构照抄老工程`Core/story/script.h/.cpp`，但按用户这次
的顶层原则和确认的范围做了几处关键调整（见下）。

## 顶层原则的落地

用户要求"剧情作者只编辑json就能表达所有逻辑，C++ mod只用于json表达不了的复杂逻辑"。`Script`
在这次设计里承担两件事：

1. **`Container`变量池**：`Script : public Container`，`self.`前缀路由的目标就是`this`（见
   `Dependence/story/expression.md`"ScriptContext"一节）。
2. **`Script -> Milestone -> Event/Dialog/Change`架构的调度者**：`ReadMilestones`从json建图
   （顺序解锁），`MatchEvent`拿一个运行时`Event`去匹配所有`actives`里的`Milestone`，命中的
   `Milestone`的`Dialog`/`Change`汇总成`vector<ScriptAction>`返回。

`Script`可以在任意地方单独`new`出来（构造函数只需要一个`ScriptFactory*`+mod id），不假设自己是
全局唯一实例——`Story`域这次持有的`mainScripts`只是"当前唯一在用"的实例，不是"唯一能存在"的
实例，后续Citizen/Elevator等会各自持有自己的`Script*`。

## 和老工程的关键差异

- **`WrapScript`转调`mod->WrapScript`**：`Script::WrapScript`直接把参数原样转给
  `mod->WrapScript`，mod侧默认实现原样透传。`MatchEvent`收尾时调用一次`WrapScript`（"搭建空
  函数+调用点"的落地）。给`ScriptMod`新增这个虚方法前，先验证并重新编译了`Forever_Mod`下
  `Empty`/`Wxdj`/`Test`三个dll，确保vtable槽位和新头文件对得上，见
  `Dependence/story/script_mod.md`"新增虚方法前先确认过跨DLL兼容性"一节。
- **`WrapScript`/`MatchEvent`都新增了`PostHandle* post`参数**：`Script::WrapScript`原样转给
  `mod->WrapScript`；`Script::MatchEvent`把自己收到的`post`原样转给`WrapScript`，二者都只是
  单纯透传，不在`Script`这一层解读`post`的内容——真正使用`post`的是mod侧
  （`ScriptMod::WrapScript`重载），见`Dependence/story/script_mod.md`。`Story::
  BroadcastGameStart`是这条链路最外层的调用方，构造`post`并往下传，见`Core/story/story.md`。
- **JSON解析只认`"milestones"`这一种数组结构**——老工程`ReadScript`同时支持"根节点是数组"和
  "根节点是`{names, milestones}`对象"两种格式、外加一套"占用名"(`ReadNames`)机制，这次简化成
  只支持`{"milestones": [...]}`这一种（`Story::GetMainScripts`不需要"占用名"去重这类高级功能，
  当前阶段只有一份主线剧情），少了`caches`里的`vector<string>`那一半（占用名列表），只保留
  "路径->里程碑表"这一半。
- **`BuildEvent`只识别`"game_start"`一种`type`，`BuildChanges`识别`"set_value"`/
  `"place_holder"`两种`type`**，其余识别到的`type`字符串统一`THROW_EXCEPTION(
  RuntimeException, ...)`，等对应类型被点名实现时再插入分支，见`Dependence/story/
  event.md`/`change.md`"JSON分发"一节。`"place_holder"`分支`new PlaceHolderChange(
  BuildExpression(obj["label"]))`——`PlaceHolderChange`是一个"占位符"结构性节点，本身不代表
  任何真正的游戏效果，作用是在`test.json`里标记一个位置，供`ScriptMod::WrapScript`用
  `FindLabel`按标签找到并原地替换成mod自己持有的真实`Change`（这次的例子是`EmptyScript`把它
  换成`ChangeControlChange`，见`Dependence/story/script_mod.md`"典型用法"一节）——如果这个
  占位符没有被任何`WrapScript`重载替换掉，它会原样出现在`MatchEvent`的返回值里，
  `Story::ApplyChange`认不出`PlaceHolderChange`类型，只会打一条"未实现"日志，不会崩溃。
- **`BuildCondition`改名`BuildExpression`**，语义不变（把json节点的字符串值解析成一个
  `Expression`），呼应`Condition`→`Expression`的整体改名。
- **`MatchEvent`的触发条件检查折进了`Milestone::MatchTrigger`内部**，不再是`Script`这一层单独
  维护的循环，见`Core/story/milestone.md`"MatchTrigger"一节。

## 依赖关系

- 依赖：`Dependence/story/{expression,event,dialog,change,script_mod,script_factory}.h`、
  `Dependence/common/handle.h`（`PostHandle`，`WrapScript`/`MatchEvent`透传）、
  `common/{utility,error,json}.h`、`Core/story/milestone.h`。
- 被谁依赖：`Core/story/story.h`（`Story::mainScripts`/`systemScript`，
  `Story::BroadcastGameStart`构造`post`传进`MatchEvent`）、
  `Forever/Framework/ForeverStoryFrameworkComponent.cpp`（间接通过`Story`）。

## 待办/后续阶段

- 其余68种event/change类型的JSON分发分支（`"place_holder"`这次已接入）。
- 老工程`ReadScript`的"占用名"机制、数组根节点格式，等真的需要多剧情共享同一份milestone缓存/
  避免命名冲突时再考虑要不要加回来。
