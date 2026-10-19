# script.h / script.cpp

## 职责

剧情作者只编辑json就能表达全部逻辑，C++ mod只用于json表达不了的复杂逻辑。`Script`承担两件事：

1. **`Container`变量池**：`Script : public Container`，`self.`前缀路由的目标就是`this`（见
   `Dependence/story/expression.md`"ScriptContext"一节）。
2. **`Script -> Milestone -> Event/Dialog/Change`架构的调度者**：`ReadMilestones`从json建图
   （顺序解锁），`MatchEvent`拿一个运行时`Event`去匹配所有`actives`里的`Milestone`，命中的
   `Milestone`的`Dialog`/`Change`汇总成`vector<ScriptAction>`返回。

`Script`可以在任意地方单独`new`出来（构造函数只需要一个`ScriptFactory*`+mod id），不假设自己是
全局唯一实例——`Story`域持有的`mainScript`只是"当前唯一在用"的实例，不是"唯一能存在"的实例，
Citizen/Elevator等可以各自持有自己的`Script*`。

## 关键设计

- **`WrapScript`转调`mod->WrapScript`**：`Script::WrapScript`直接把参数原样转给
  `mod->WrapScript`，mod侧默认实现原样透传。`MatchEvent`收尾时调用一次`WrapScript`。
- **`WrapScript`/`MatchEvent`都带`PostHandle* post`参数**：`Script::WrapScript`原样转给
  `mod->WrapScript`；`Script::MatchEvent`把自己收到的`post`原样转给`WrapScript`，二者都只是
  单纯透传，不在`Script`这一层解读`post`的内容——真正使用`post`的是mod侧
  （`ScriptMod::WrapScript`重载），见`Dependence/story/script_mod.md`。`Story::
  BroadcastGameStart`是这条链路最外层的调用方，构造`post`并往下传，见`Core/story/story.md`。
- **JSON根节点固定是`{"milestones": [...]}`这一种对象格式**，`caches`的value类型是一个
  `FileCache`结构体（见下"主线剧情.script新增三个顶层字段"一节）。
- **`BuildEvent`识别`"game_start"`/`"option_dialog"`(name/option)/`"citizen_decease"`(name/reason，留空=不限)/`"speaking_finish"`(label，留空=不限)/`"time_up"`(name，留空=不限)/`"enter_zone"`/`"leave_zone"`(zone)/`"enter_building"`/`"leave_building"`(zone/building)/`"enter_room"`/`"leave_room"`(zone/building/room，都留空=不限)/`"global_message"`(message，留空=不限)，
  `BuildChanges`识别`"set_value"`/`"place_holder"`/`"debug_print"`/`"global_message"`(message)/`"for_range"`(var/from/to/step/do，
  含两端，循环变量写进`$$self.<var>`，由`AForeverFrameworkActor::ApplyChange`递归执行)/`"npc_teleport"`(name/destination，destination是房间地址；有Actor就瞬移并打断正在走的路，没有就只改逻辑位置，被玩家占有的不动)/`"spawn_npc"`(name/gender/birthday，可选scheduler/home，见`story.md`"spawn_npc")/`"create_timer"`(name/time，可选category/label)/`"remove_timer"`(name，计时器见`story.md`"计时器")/`"launch_elevator"`(building/elevator/command，
  由Map执行，见`Source/Core/map/cabin.md`)/`"add_option"`/`"remove_option"`(name/option)/
  `"start_puzzle"`/`"enter_vehicle"`/`"set_door"`(name/access)/`"register_door"`(name/citizen/pass)/`"npc_navigate"`(name/destination=房间地址)/
  `"change_control"`(name)**——`set_door`的`access`、`register_door`的`pass`是布尔表达式，JSON里写`true`/`false`
  或者`"true"`/`"false"`都行(`AsString()`把JSON布尔转成字符串，执行时`EvaluateExpressionBool`求值，也可以写
  `$$self.xxx`这类表达式)：`access`为true=常开、false=常锁；`pass`为true=允许这个人通过、false=不允许，省略按true，
  按人登记优先于门的常开/常锁，见`Source/Core/map/door.md`；`remove_option`对门/市民/车辆都生效；`npc_navigate`的name/destination先按原样查，
  查不到再当表达式求值(mod在C++里生成的是现成的姓名/地址，脚本里可以写`$$self.name`这类表达式)。其余`type`字符串统一
  `THROW_EXCEPTION(RuntimeException, ...)`，等对应类型被点名实现时再插入分支，见
  `Dependence/story/event.md`/`change.md`"JSON分发"一节。`"place_holder"`分支
  `new PlaceHolderChange(obj["label"].AsString())`——`PlaceHolderChange`是一个"占位符"结构性
  节点，本身不代表任何真正的游戏效果，作用是在`test.json`里标记一个位置，供`ScriptMod::
  WrapScript`用`FindLabel`按标签找到并原地替换成mod自己持有的真实`Change`（见
  `Dependence/story/script_mod.md`"典型用法"一节）——如果这个占位符没有被任何`WrapScript`
  重载替换掉，它会原样出现在`MatchEvent`的返回值里，
  `AForeverFrameworkActor::ApplyChange`遇到`PlaceHolderChange`直接跳过(不转给各域，也不打"未实现"日志)。
- **`Change`/`Event`/`Dialog`各字段统一是`std::string`（DSL源码文本），不预先`Parse`成
  `Expression`对象再存成员**——`Expression`树若被一个模块构造、另一个模块求值会导致跨模块
  堆损坏崩溃，完整原因见`Dependence/story/change.md`"字段类型是`std::string`"一节；所有
  调用点直接用`obj["xxx"].AsString()`取原始字符串，求值前才现场`Parse`。
- **`MatchEvent`的触发条件检查在`Milestone::MatchTrigger`内部完成**，不是`Script`这一层单独
  维护的循环，见`Core/story/milestone.md`"MatchTrigger"一节。

## 主线剧情.script的三个顶层字段

`Config::GetMainStoryScriptPath()`（`Config::GetScriptPath("test")`）解析出的那一份
`.script`文件——不是Job/Organization/Scheduler各自读取的那些——额外支持三个和`"milestones"`
平级的顶层字段，均为可选（不写就是空/默认值，不影响老`.script`文件）：

- **`name_reserve`**：字符串数组，剧情作者显式列出脚本里会用到的姓名，
  `Populace::InitNames()`在生成市民之前把这些名字喂给`Name::ReserveName`占位，姓名生成器
  之后不会再生成同名结果，避免"脚本里写死的角色名"和"随机生成的市民名"撞名。
- **`global_settings`**：目前只识别一个子字段`time_flow_ratio`（游戏时钟相对真实时间的
  倍率，默认`2.0`）——`AForeverFrameworkActor::EnsurePlayerGenerated()`读到就调用
  `Player::SetTimeFlowRatio()`覆盖默认值。
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
数据，不需要等`ScriptMod`/`Story`对象就绪，`ReadScript`一次性把milestones+这三个字段
一起解析进`FileCache`（存进`caches`这份静态缓存），`GetNameReserve`/`GetGlobalSettings`/
`GetModDependences`三个静态方法各自"先`ReadScript`（命中缓存直接返回，不重复读盘/解析）
再取对应字段"，供`BeginPlay()`/`Populace::InitNames()`/`EnsurePlayerGenerated()`在
`Story`对象/milestone真正加载之前就能查询。

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

- 其余68种event/change类型的JSON分发分支（`"place_holder"`已接入）。
- 多剧情共享同一份milestone缓存/避免命名冲突的机制（如按名称占用），等真的需要多剧情时再加。
