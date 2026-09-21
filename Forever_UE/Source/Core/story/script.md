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
- **JSON根节点这次固定是`{"milestones": [...]}`这一种对象格式**——老工程`ReadScript`同时
  支持"根节点是数组"和"根节点是`{names, milestones}`对象"两种格式、外加一套"占用名"
  (`ReadNames`)机制，这次简化成只认对象格式，`caches`的value类型这次是一个`FileCache`
  结构体（见下"主线剧情.script新增三个顶层字段"一节），不是老工程那种
  `pair<vector<string>, unordered_map<string,Milestone*>>`。

## 主线剧情.script新增三个顶层字段（这次新增，只对主线剧情这一份.script文件生效）

`Config::GetMainStoryScriptPath()`（`Config::GetScriptPath("test")`）解析出的那一份
`.script`文件——不是Job/Organization/Scheduler各自读取的那些——这次额外支持三个和
`"milestones"`平级的顶层字段，均为可选（不写就是空/默认值，不影响老`.script`文件）：

- **`name_reserve`**：字符串数组，剧情作者显式列出脚本里会用到的姓名，
  `Populace::InitNames()`在生成市民之前把这些名字喂给`Name::ReserveName`占位，姓名生成器
  之后不会再生成同名结果，避免"脚本里写死的角色名"和"随机生成的市民名"撞名。老工程叫
  `"names"`，机制照抄（`E:\Projects\Forever_UE`的`Script::ReadNames`+`Name::
  ReserveName`/`RegisterName`），字段名按这次要求改成`name_reserve`。
- **`global_settings`**：目前只识别一个子字段`time_flow_ratio`（游戏时钟相对真实时间的
  倍率，默认`2.0`）——`AForeverFrameworkActor::EnsurePlayerGenerated()`读到就调用
  `Player::SetTimeFlowRatio()`覆盖默认值。老工程的`global_setting`（单数）是
  **config.json级别**的字段，这次按要求挪到`.script`文件里，是新设计，不是照抄。
- **`mod_dependences`**：字符串数组，元素是mod id（**不带concept前缀**——id的命名不保证
  能反映它属于哪个concept，比如`"building_clean"`名字像Building mod，实际可能是个Job
  mod，不能靠字符串猜），声明主线剧情脚本依赖哪些mod。
  `AForeverFrameworkActor::ValidateMainStoryDependencies()`在`BeginPlay()`最前面
  （生成任何东西之前）对`Registry::CheckModRegistered(id)`做全局OR匹配（20个Factory各查
  一次`CheckRegistered`），只要有一个id没被任何concept注册就`THROW_EXCEPTION`，交给
  `BeginPlay()`统一的`try/catch`处理（打日志+退出游戏，见`ForeverFrameworkActor.md`）。
  已知边界情况：`"empty"`这种几乎每个concept都会注册的占位id，全局匹配下必然"通过"，
  这是"id不带concept信息"这个前提本身带来的局限，不是bug。

**读取时机**：`name_reserve`必须在生成市民之前读到，但milestone的正式加载时机
（`EnsureStoryGenerated()`）在`EnsurePopulaceGenerated()`之后——这三个新字段都是纯JSON
数据，不需要等`ScriptMod`/`Story`对象就绪，`ReadScript`一次性把milestones+这三个新字段
一起解析进`FileCache`（存进`caches`这份静态缓存），`GetNameReserve`/`GetGlobalSettings`/
`GetModDependences`三个静态方法各自"先`ReadScript`（命中缓存直接返回，不重复读盘/解析）
再取对应字段"，供`BeginPlay()`/`Populace::InitNames()`/`EnsurePlayerGenerated()`在
`Story`对象/milestone真正加载之前就能查询，和老工程"分两次读取"（`ReadNames`早、
`ReadMilestones`晚）是同一个思路。
- **`BuildEvent`只识别`"game_start"`一种`type`，`BuildChanges`识别`"set_value"`/
  `"place_holder"`/`"debug_print"`三种`type`**，其余识别到的`type`字符串统一`THROW_EXCEPTION(
  RuntimeException, ...)`，等对应类型被点名实现时再插入分支，见`Dependence/story/
  event.md`/`change.md`"JSON分发"一节。`"place_holder"`分支`new PlaceHolderChange(
  obj["label"].AsString())`——`PlaceHolderChange`是一个"占位符"结构性节点，本身不代表
  任何真正的游戏效果，作用是在`test.json`里标记一个位置，供`ScriptMod::WrapScript`用
  `FindLabel`按标签找到并原地替换成mod自己持有的真实`Change`（这次的例子是`EmptyScript`把它
  换成`ChangeControlChange`，见`Dependence/story/script_mod.md`"典型用法"一节）——如果这个
  占位符没有被任何`WrapScript`重载替换掉，它会原样出现在`MatchEvent`的返回值里，
  `Story::ApplyChange`认不出`PlaceHolderChange`类型，只会打一条"未实现"日志，不会崩溃。
- **`BuildExpression`这层一行包装函数已删除**：`Change`/`Event`/`Dialog`各字段最初设计成
  `Expression`类型时，`BuildEvent`/`BuildChanges`/`BuildDialogs`需要一个`BuildExpression
  (const JsonValue& root)`辅助函数把json节点的字符串值解析成`Expression`对象再传给构造函数。
  后来因为"`Expression`树被一个模块构造、另一个模块求值"导致的跨模块堆损坏崩溃（完整原因见
  `Dependence/story/change.md`"字段类型是`std::string`"一节），所有这些字段改回了
  `std::string`（存原始DSL源码文本，求值前才现场`Parse`）——`BuildExpression`的函数体这时候
  已经退化成`return root.AsString();`一行，纯粹的包装没有意义，直接删掉了这个函数，所有调用点
  直接改成`obj["xxx"].AsString()`。
- **`MatchEvent`的触发条件检查折进了`Milestone::MatchTrigger`内部**，不再是`Script`这一层单独
  维护的循环，见`Core/story/milestone.md`"MatchTrigger"一节。

## 依赖关系

- 依赖：`Dependence/story/{expression,event,dialog,change,script_mod,script_factory}.h`、
  `Dependence/common/handle.h`（`PostHandle`，`WrapScript`/`MatchEvent`透传）、
  `common/{utility,error,json}.h`、`Core/story/milestone.h`。
- 被谁依赖：`Core/story/story.h`（`Story::mainScripts`/`systemScript`，
  `Story::BroadcastGameStart`构造`post`传进`MatchEvent`）、
  `Forever/Framework/ForeverStoryFrameworkComponent.cpp`（间接通过`Story`）、
  `Core/populace/populace.cpp`（`InitNames()`调`GetNameReserve`）、
  `Forever/Framework/ForeverFrameworkActor.cpp`（`ValidateMainStoryDependencies`调
  `GetModDependences`，`EnsurePlayerGenerated`调`GetGlobalSettings`）。

## 待办/后续阶段

- 其余68种event/change类型的JSON分发分支（`"place_holder"`这次已接入）。
- 老工程`ReadScript`的"占用名"机制、数组根节点格式，等真的需要多剧情共享同一份milestone缓存/
  避免命名冲突时再考虑要不要加回来。
