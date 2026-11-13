# change.h / change.cpp

## 职责

`Change`是纯数据类——没有`Apply`之类的虚方法，执行逻辑在调用方按`GetType()`/`dynamic_cast`分派。基类只有
`GetType()`纯虚 + `condition`（控制条件，`std::string`，`GetCondition`/`SetCondition`）。目前共54个具体子类，
每个子类只有字段 + getter/setter，`GetType()`返回的snake_case字符串就是JSON里的`type`。

## 执行分派（谁来执行哪个Change）

入口是`AForeverFrameworkActor::ApplyChange`（先求`change->GetCondition()`，为假直接丢弃）：

1. 只有UE能做的先在这里拦截：`StartPuzzle`/`OpenShop`/`PayWage`/`EnterVehicle`/`BoardVehicle`/`LeaveVehicle`/`ForceLeave`/
   `TestNavigate*`/`VehicleCommand`/`ChangeControl`/`DebugPrint`/`GlobalSetting`/`GlobalMessage`/`CreateTimer`/`RemoveTimer`/
   `SpawnNpc`/`NPCRevive`/`NPCTeleport`/`NPCNavigate`/资产四种/`ForRange`等（Core不认识Actor/Controller/GEngine）。
2. 其余转发给各域`ApplyChange`，谁认识谁处理：`Map`（`AddOption`/`RemoveOption`/`LaunchElevator`/`SetDoor`/`RecordObject`/
   `RegisterDoor`）、`Player`（`ChangeWeather`/资产上的`AddOption`/`RemoveOption`）、`Populace`/`Traffic`（各自对象上的选项）、
   `Story`（`SetValue`）。
3. `PlaceHolderChange`没有任何执行效果；它是留给`ScriptMod::WrapScript`替换的锚点（见下）。没被替换就原样落到这里，不崩溃但等于
   剧情作者忘了配对应的mod逻辑。

## JSON分发

`Core/story/script.h`的`Script::BuildChanges`识别34种`type`，未识别的`type`直接`THROW_EXCEPTION`（剧情写错应尽早报错）；
`ApplyChange`链上没人认识的Change则只是不生效（运行时每次匹配后都会走，比解析阶段宽松）。**没有JSON分支的20种**
（只能由C++构造，或尚未实现）：`GameEnd`/`RemoveValue`/`DeactivateMilestone`/`RemoveNpc`/`TeleportPlayer`/`PlayVideo`/`PlayBgm`/
`StopBgm`/`BankTransaction`/`GiveEstate`/`RemoveEstate`/`GiveVehicle`/`RemoveVehicle`/`PlayerInjured`/`PlayerCured`/`PlayerIll`/
`PlayerRecover`/`PlayerSleep`/`ChangeTime`/`ChangeWanted`。

## 字段类型是`std::string`（DSL源码），不是`Expression`

所有字段统一是`std::string`（原始DSL源码文本，不预先解析），唯一的`vector`字段是`SpawnNpcChange::jobs`
（`vector<string>`）。`Expression::Parse`+求值延后到真正要用的那一刻，用`expression.h`的
`EvaluateExpression(source, context)`/`EvaluateExpressionBool(source, context)`现场完成。

**原因（一次真实崩溃换来的教训，不要再把这类字段改回预先`Parse`好的`Expression`对象）**：
`Expression`树的求值是虚函数调用，走的是节点对象自己的vtable，vtable由构造它的那个模块钉死；
如果一个模块（如`Basic.dll`）构造`Expression`树、另一个模块（如`Forever.dll`）对它求值，
虚调用实际执行的是构造方模块的机器码，返回值的堆分配也发生在构造方的CRT堆，但UE给
`UnrealEditor-Forever.dll`单独重载了全局`operator new`/`delete`走`FMemory`——字符串在
`Basic.dll`的CRT堆分配、在`Forever.dll`侧被`FMemory`释放，堆损坏崩溃（PIE里复现过：
`NPCNavigateChange::destination`装了真实房间地址，超出`std::string`的SSO阈值才第一次踩上）。
只要`Expression`树被一个模块构造、被另一个模块求值就有这个风险，是"把预先解析好的`Expression`对象当存储字段"
这个设计本身的通病；改成`std::string`之后，`Parse`+`Evaluate`永远发生在同一次函数调用里，`Expression`树从不跨模块存活。

`NPCNavigateChange`特殊：Basic的Job构造时传入的是已经算好的姓名/房间地址（不是表达式），脚本里的`npc_navigate`写的
可能是表达式（如`$$self.name`），所以执行端先按原样查，查不到再当表达式求值后查。

## 所有权：`ForRangeChange`

`ForRangeChange::changes`（循环体）是`ForRangeChange`自己持有的本体，析构时逐个`delete`（`Script::BuildChanges`解析`do`
时`new`出来的）。其余Change的本体都由`Milestone::changes`持有（`OBJECT_HOLDER`）；`Option::changes`引用的嵌套Change
由包含该分支段的`Dialog`析构时回收，见`dialog.md`；`Script::BuildChanges`解析中途抛异常时已`new`出的Change由函数内的guard释放。

## `SetValueChange`

`variable`（要被赋值的变量名本身，不是内容，规则见`expression.md`的`ResolveVariableName`；只有`self.`和全局变量能写）+
`value`（DSL源码，求值后赋给对应变量池）。`Story::ApplyChange`里调`context.self->SetValue(...)`。

## `PlaceHolderChange`

只有一个`label`字段（DSL源码，可能引用变量）。它不是给玩家看的内容，是留给`ScriptMod::WrapScript`在`actionStack`里按标签查找、
原地替换成mod自己持有的真实`Change`的"锚点"（`ScriptMod::FindLabel`，见`script_mod.md`）。

## `ChangeControlChange`（change_control）

把玩家操控权切到指定姓名（`name`，表达式）的市民。Core不知道`AActor`/`APlayerController`，所以由Forever层拦截处理
（`UForeverStoryFrameworkComponent::ApplyControlChange`：求值姓名 → `FindOrSpawnCitizenByName` → `Possess`），
见`Forever/Framework/ForeverStoryFrameworkComponent.md`。典型用法是`test.json`里放`{"type":"place_holder","label":"control"}`，
`EmptyScript::WrapScript`向Core要一个随机citizen姓名后构造`ChangeControlChange`替换占位（`Forever_Mod/Empty/Cpp/Empty/empty_mods.h`）；
被替换进去的对象必须由mod长期持有（`EmptyScript`的成员），不能是每次现场`new`的临时对象。

## `DebugPrintChange`（debug_print）

只带`message`（DSL源码）。语义是开发期调试输出，不是游戏内广播，所以不复用`GlobalMessageChange`。打印要调用
`GEngine::AddOnScreenDebugMessage`，所以在Forever层（`ForeverFrameworkActor.cpp`）拦截，`EvaluateExpression`后打印到屏幕左上角。
`Resource/Story/job_shop_saler.script`/`organization_shop.script`的`game_start`里程碑用它验证Job/Organization的Script能否广播
`game_start`、`$$self.name`能否取到唯一的`GetName()`。

## `RecordObjectChange`（record_object）

`{"type":"record_object","object":"\"building\"","kind":"\"building_wxdj\"","policy":"\"first\"","variable":"wxdj_home"}`
——脚本里拿园区/建筑/房间地址的唯一途径，不用C++ mod发`post "list buildings"`。`object`(表达式)是类别，目前
`zone`/`building`/`room`，以后可扩展；`kind`是mod type(留空不限)；`policy`是`first`(默认，候选地址排序后取第一个)或`random`；
`variable`是结果变量(规则同`set_value`)，值为`GetAddress()`，找不到写空串并打警告。执行在`Map::ApplyChange`。
房间地址用隐式拼接：`$$wxdj_home + " 00001"`。

## 载具相关Change

- `enter_vehicle`(vehicle)：驾驶某辆载具。
- `board_vehicle`(vehicle)：搭乘线路上的公交——不是驾驶：玩家Possess公交Pawn后只有鼠标视角、WASD无效，车自己跑。UE层执行
  （`UForeverTrafficFrameworkComponent::ApplyBoardVehicle`）。
- `leave_vehicle`(vehicle)：玩家下车(驾驶和搭乘都行)；驾驶者要过下车点遮挡判定，搭乘者不判定。vehicle非空时只在玩家正在这辆里才执行。
- `force_leave`(vehicle，可空)：强制下车(比如公交收班)，驾驶和搭乘都生效，不做遮挡判定；留空=玩家当前所在的任何载具。
- `vehicle_command`(vehicle, command)：`"run"`=随机选一条公交环线的随机站，先导航过去再开始循环；`"stop"`=刹停并保持，线路公交同时退出
  时刻表（`StartBusTestRun`/`StopBusRun`）。
- `vehicle_navigate`(vehicle, destination)：让载具由Controller开到`destination`（房间地址表达式；字面量`"random"`=随机一个有车行节点的地下室房间，目前只有工厂）。
  到达后给这辆车自己的script发`vehicle_arrive`事件，在该事件里再发`vehicle_navigate`就是连续随机导航（可用来写随机导航压力测试）。
  UE层`UForeverTrafficFrameworkComponent::NavigateVehicle`执行：请求入队、每帧按`maxVehicleSpawnsPerFrame`处理，必要时生成车辆Actor。
- `visualize_navigation`(address)：给玩家显示到某个房间(目前只支持房间地址，表达式)的导航线。玩家步行按人行道画，开车按车道画（车到不了的房间画到该建筑1楼外部连接点附近的车道），其它Pawn（公交乘客等）不画；切换操控自动刷新，玩家进入目标房间后消失。UE层`UForeverTrafficFrameworkComponent::StartGuide`执行。合并了原来的`test_navigate_vehicle`/`test_navigate_citizen`。
- 以上全部只能在UE层执行，`AForeverFrameworkActor::ApplyChange`里`dynamic_cast`转发给对应Component。
- **注意**：同名的`enter_vehicle`/`leave_vehicle`也是事件类型（见`event.md`），事件是"发生了"的通知，Change是"去做"的指令，
  `BuildEvent`/`BuildChanges`各自识别，互不冲突。

## 市民相关

- `spawn_npc`：字段全是表达式；`economy`（布尔表达式，空=false）决定是否接入经济系统；`jobs`是职业mod id列表，`scheduler`是调度mod id。
  目前真正生效的字段见`Core/story/story.md`"spawn_npc"。
- `npc_revive`(name, health可选，留空=满血)：UE层`AForeverFrameworkActor::ApplyReviveNpc`执行。只对已死亡的市民生效；复活后没有房间和位置，
  需接`npc_teleport`。
- `npc_teleport`(name, destination)：destination写`"home"`=该市民的家(`Citizen::GetRoom()`)。

## 资产Change

- `pick_asset`(asset可选，缺省=执行脚本所属的资产，即`$$self.name`)：捡起地上(房间根/室外)的资产。顺序：没背包且它可背
  (`GetBackpack`、Object、count==1)就背上；有背包且放得下(size*count，堆叠整体算)放包里；否则右手空放右手、再左手(手只收count==1的单件)；
  都不行失败，资产保持原状。堆叠资产整体考虑，不会只捡一部分。
- `grant_asset`(asset_type、count缺省1)：凭空给资产（没有场景里的Actor要删），放置顺序同上，多个作为一个堆叠整体，放不下整体失败。
- `deduct_asset`(asset_type、count)：递归左右手和背包(不含肩膀)里指定类型够数才扣并销毁，否则整体失败。
- `drop_asset`(asset_type、count)：同样判定，够数就把它们放到玩家附近地面(在房间里进房间，否则进`Player::worldAssets`)，否则整体失败。
- 结果通过`get_asset`/`lose_asset`事件发回执行Change的脚本，见`event.md`。
- JSON里的`type`是Change类型本身，资产类型字段叫`asset_type`（C++侧的getter是`GetAssetType()`，因为`GetType()`已被Change类型占用）。
  UE层执行：`UForeverAssetFrameworkComponent::ApplyPickAsset/ApplyGrantAsset/ApplyDeductAsset/ApplyDropAsset`。

## 天气(change_weather)

`change_weather`(weather，表达式，结果是天气名`sunny/cloudy/overcast/light_rain/heavy_rain/light_snow/heavy_snow`)：强制换天气，同时
**重置系统自动换天气的倒计时**(重新抽一个"均值2天、标准差半天"的间隔)。写法`{"type":"change_weather","weather":"\"overcast\""}`。
由`Player::ApplyChange`执行(`Weather::SetByName`)，名字非法打警告忽略，不受冬季才下雪的限制。换成功后系统广播`weather_change`事件，
见`event.md`。

## 门禁

- `set_door`(name, access)：`name`是门的交互名(表达式)；`access`是布尔表达式，true=常开(任何人都能过)，false=常锁；JSON里可写布尔字面量
  （`AsString()`转成`"true"`/`"false"`）。
- `register_door`(name, citizen, pass)：按人登记例外，优先于门本身的常开/常锁/仅主人；`pass`布尔表达式，留空按true。
- 都在`Map::ApplyChange`执行。

## 计时器与电梯

- `create_timer`(name, time, category, label, mode)：`category`/`label`省略时计时器归创建它的脚本自己。`mode`（表达式，留空=`absolute`）：
  `absolute`=游戏内绝对时刻（带年月日，如`"2024-05-01 08:00:00"`，已过去则无效）；`relative`=游戏内相对时长（从0年1月1日0时起算，
  `"00:00:04"`=4个游戏秒后）；`real`=现实相对时长，写法同relative；`next`=游戏内未来某天的某个钟点（日期部分是相对天数，支持`"2月10日"`、
  `"1月40日"`写法，时间部分是绝对钟点，早于当前钟点则天数再加一）。见`Core/story/story.md`"计时器"。
- `remove_timer`(name)。
- `launch_elevator`(building, elevator, command)：`elevator`是电梯交互名(`$$self.name`)，`building`只用于日志，`command`解析成层号
  （`ParseElevatorCommand`）；`Map::ApplyChange`调`Cabin::Launch`，实际移动由UE层`ABuildingElement::Tick`推进。

## 其它

- `start_puzzle`(puzzle, param可选)：`param`是剧情脚本传给小游戏的启动参数(表达式)，空串=不传；和`config.json`里`puzzle_mods`的静态args不是一回事
  （那个在创建小游戏时传入）。
- `open_shop`：无字段。`pay_wage`(amount，分的表达式)：给被玩家操控的市民发工资，由执行它的Job脚本所属岗位的组织支付（`Flow::WAGE`，代扣所得税）。
- `global_setting`(setting, value)：运行时改全局设置，`value`是表达式。目前只认`time_flow_ratio`（`Player::SetTimeFlowRatio`，1.0=1真实秒1游戏分钟，
  默认10.0），由`AForeverFrameworkActor::ApplyChange`执行；未知设置打警告。主线脚本顶层的`global_settings.time_flow_ratio`仍管开局初值。
- `global_message`(message)：触发`global_message`事件广播，见`event.md`。
- `add_option`/`remove_option`(name, option)：给名字全局唯一的对象（门/市民/载具/资产/房间里的资产）增删交互选项，找不到静默忽略。
- `for_range`(var, from, to, step可选默认1, do)：含两端；循环变量写进当前脚本自己的变量池（`$$self.<var>`）；
  `AForeverFrameworkActor::ApplyChange`里有迭代次数上限（`kMaxForRangeIterations`），超出截断并打警告。

## 依赖关系

- 依赖：`common/utility.h`、`common/error.h`、`expression.h`（`EvaluateExpression`/`EvaluateExpressionBool`——字段本身是`std::string`，
  只有求值那一刻才用得到`Expression`）。
- 被谁依赖：`Core/story/milestone.h`（`Milestone::changes`）、`Core/story/script.h`（`Script::BuildChanges`）、各域`ApplyChange`、
  `dialog.h`（`Option::changes`）、`script_mod.h`（`FindLabel`按`dynamic_cast<const PlaceHolderChange*>`查找）、
  `Forever/Framework/ForeverFrameworkActor.cpp`及各Component（UE层执行）、`Basic/society/job_warehouse_keeper.cpp`（构造`NPCNavigateChange`）、
  `Forever_Mod/Empty/Cpp/Empty/empty_mods.h`（`EmptyScript`持有`ChangeControlChange`）。

## 待办/后续阶段

- 上面"没有JSON分支的20种"Change：被点名实现时需要同时补`Script::BuildChanges`分支和某个域/UE层的执行分支，两者要一起补。
- 嵌套`Option`里的Change本体没有回收机制，见`dialog.md`。

## JumpTimeChange(`jump_time`)
字段`time`、`mode`(均为表达式)，含义同`create_timer`的absolute/relative/next(不支持`real`)；目标不晚于现在静默不执行。
执行见`ForeverFrameworkActor.md`“跳时间”。JSON：`{ "type": "jump_time", "time": "08:00:00", "mode": "next" }`。
