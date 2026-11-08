# event.h / event.cpp

## 职责

`Event`基类 + 34个具体事件子类。事件有两种身份：里程碑`triggers`里的**触发条件**（字段是表达式，留空=不限），和运行时由
C++广播出来的**事件实例**（字段是字面量）。`Script::MatchEvent`用触发条件的`Match(运行时事件, context)`判断是否命中。

## 关键设计

- `GetType()`纯虚：每个子类的静态类型标识，snake_case字符串（如`"game_start"`），也是JSON里的`type`。
- `Match(Event* e, const ScriptContext& context)`是有默认实现的虚函数：默认只比较`GetType()`。需要字段过滤的子类override，
  先调`Event::Match`（类型相同才`static_cast`成自己），再逐个字段比较：触发条件侧的字段用`EvaluateExpression(expr, context)`
  求值后和运行时事件的字面量比，空字段不比较。`MatchLocationField`是这类"表达式 vs 字面量"比较的公共函数（位置/站点/载具/资产事件共用）。
- `GetLocalValue(name)`：`local.`前缀变量的求值入口（见`expression.md`）。基类默认返回`{false, {}}`，需要的子类override，把自己的字段暴露出来。
  事件实例上的字段读的就是运行时字面量。
- 字段统一是`std::string`（DSL源码，不预先解析），原因同`change.md`"字段类型是`std::string`"（`Expression`树不能跨模块存活）。
  `CitizenDeceaseEvent`的`damage`/`visible`是例外，只存在于运行时事件，不做过滤。
- `condition`（`SetCondition`/`GetCondition`）：事件级控制条件，`BuildEvent`统一从JSON的`condition`读入。
- 类型字符串和成员命名的坑：`GetAssetEvent`/`LoseAssetEvent`的资产类型getter叫`GetAssetType()`，因为`GetType()`已是事件类型。

## 没有字段逻辑的13种

`transaction_result`/`npc_meet`/`citizen_born`/`player_injured`/`player_cured`/`player_ill`/`player_recover`/`player_rest`/`player_sleep`/
`wanted_change`/`player_arrested`/`player_released`/`use_asset`只搬运了字段和getter/setter，继承默认`Match`，没有`GetLocalValue`，
`Script::BuildEvent`也没有分支（遇到会`THROW_EXCEPTION`，不是静默跳过）。等被点名实现时三者一起补。

## 脚本里能用的触发条件（`Script::BuildEvent`识别的21种）

- `game_start`：无字段。
- `option_dialog`(name/option)：玩家点了某个对象的某个选项。脚本里`local.name`/`local.option`读被点对象名和选项文字。
- `global_message`(message，留空=任何全局消息都触发)：只由`global_message` Change发出——执行时求出消息文本，
  `UForeverStoryFrameworkComponent::BroadcastGlobalMessage`广播给所有挂了Script的东西(主线/组织/岗位/市民Scheduler/车辆/门，和`game_start`同范围)。
  脚本里用`local.message`。收到消息的里程碑可以再发全局消息，嵌套深度超过8层丢弃并打警告，防无限递归。
- `speaking_finish`(label，留空=任何带label的台词播完都触发)：对话里某句台词可写`"label"`(表达式)，玩家推进离开这一句时
  `AdvanceDialog`调`BroadcastSpeakingFinish`，广播给所有挂了Script的东西(含主线)。脚本里`local.label`。剧情对话最后一句写label，
  下一个里程碑用`speaking_finish`接上。
- `time_up`(name，留空=这个脚本的任何计时器)：`create_timer`建的计时器到时，只发给计时器的归属脚本(默认是创建它的脚本)。`local.name`。
  详见`Source/Core/story/story.md`"计时器"。
- `weather_change`(weather，留空=任何天气变化都触发)：天气变了(系统自动随机换，或`change_weather`强制换)时由
  `UForeverStoryFrameworkComponent::BroadcastWeatherChange`广播给所有挂了Script的东西(含主线，和`global_message`同范围)。
  脚本里`local.weather`读变化后的天气名。读档不广播。天气名见`Core/player/weather.md`。
- `enter_zone`/`leave_zone`(zone)、`enter_building`/`leave_building`(zone/building)、`enter_room`/`leave_room`(zone/building/room)：
  只认玩家本人(当前操控的Pawn，开车时就是车)进出园区/建筑/房间的Trigger碰撞盒，字段都是表达式、留空不限。字段值是
  `Zone/Building/Room::GetName()`，不在园区里的建筑zone为空。由`UForeverStoryFrameworkComponent::BroadcastPlayerLocation`广播给所有挂了Script
  的东西(含主线)，同时更新系统变量`system.player.zone`/`building`/`room`(离开时只有当前值还是这个名字才清空)。脚本里`local.zone`/
  `local.building`/`local.room`。园区碰撞盒只有带内部建筑的园区才有。
- `enter_stop`/`leave_stop`(stop，留空=不限站点)：公交(公共交通载具)停稳进站/启动离站。Core的`Route::Update`检测停靠边沿产出队列，
  `AForeverFrameworkActor::Tick`在`traffic->Tick`之后取走(`Traffic::ConsumeStopEvents`)，**只发给这辆载具自己的Script**
  (`UForeverStoryFrameworkComponent::BroadcastToVehicle`)。读档后第一次Update不补发(`Traffic::SilenceInitialStopEvents`)。
  `local.stop`读站点名。见`Resource/Story/vehicle_bus.script`。
- `enter_vehicle`/`leave_vehicle`(vehicle/mode，留空=不限)：玩家进入/离开载具，mode是`"drive"`驾驶/`"ride"`搭乘（作为表达式要写成
  `"\"ride\""`）。只发给被进出的那辆载具自己的Script；读档恢复不发。`local.vehicle`/`local.mode`。
  **和同名的`enter_vehicle`/`leave_vehicle` Change是两套**：事件是"发生了"的通知，Change是"去做"的指令。
- `get_asset`/`lose_asset`(JSON里字段叫`asset_type`、`result`，都是表达式、留空不限)：脚本里读`local.type`(资产mod id)和`local.result`
  (`"success"`/`"fail"`)。
  - **get_asset**：`pick_asset`/`grant_asset`之后(成功和失败都发)，以及背包界面把东西从地面区(房间/附近)拖进身上区(肩/手/背/容器)。
  - **lose_asset**：`drop_asset`/`deduct_asset`(成功对每件被扣/丢的资产发一次；失败=身上没有指定类型或数量不足，发一次，type是Change里要求的类型)，
    以及背包界面把东西从身上拖到地面区(只会成功)。
  - **发给谁**：Change触发的发给**执行这个Change的脚本**(`context.self`)；背包界面里的移动发给**这件资产自己的脚本**(没有脚本则不发)。
    根资产是容器时，容器和递归进去的每件资产各发一次。`UForeverStoryFrameworkComponent::NotifyAssetEvent`分发，同时在屏幕左上角显示
    “获取/失去 xxx”(测试用)；事件处理里再触发资产事件的嵌套深度上限8。
- `npc_arrive`(name/address，留空不比较)：`address`是房间地址(`Room::GetAddress()`)。`local.name`/`local.address`。
- `vehicle_arrive`(vehicle/address，留空不比较)：车辆Controller导航到房间后由`UForeverTrafficFrameworkComponent`只发给这辆车自己的script；`address`是房间地址。`local.vehicle`/`local.address`。
- `puzzle_result`：没有过滤字段，`Match`只比类型。`SetFromJson(root)`把小游戏`Loop`返回的json展平进事件(root必须是object，否则返回false、
  不写任何字段)：object成员用点号连接(`a.b`)，数组项用`[idx]`(`items[0]`、`items[1].x`)，int/double/bool/string直接写，null忽略。
  `GetLocalValue(展平名)`读出，脚本里写`$$local.a.b`、`$$local.items[0]`(分词器把紧跟变量名的`[数字]`并进变量token)。
- `citizen_decease`(name/reason/killer/weapon/part，留空=不限)：市民死亡时由`UForeverStoryFrameworkComponent::BroadcastCitizenDecease`广播给主线脚本和
  死者自己的Scheduler脚本(按name找，和`option_dialog`同一个模式)。`local.name`/`local.reason`读死者姓名和死因。

### citizen_decease的击杀信息(武器击杀)

- `local.killer`：凶手姓名(开枪的市民；不是市民则为空串)
- `local.weapon`：武器种类(`Weapon::GetType()`)
- `local.part`：命中部位`head`/`body`/`legs`——射线只打得到胶囊体，按命中点在胶囊体上的相对高度粗分(>=80%头，40%~80%躯干，其余腿)
- `local.damage`：致死那一发的伤害值(数值)
- `local.visible`：凶手能否看到死者(布尔)——连接死者和凶手中心点的线段用`ECC_Visibility`检测，两人自己忽略，被墙等挡住为假
- `local.location`：死亡地址，按房间地址 > 建筑地址 > 园区地址 > 地块(block)地址 > `"x,y,z"`(地图单位，两位小数)的优先级。房间/建筑/园区用它们已有的
  触发盒做小球重叠反查(`AForeverFrameworkActor::DescribeLocation`)，地块用顶层Lot的旋转矩形做几何包含。

触发条件里killer/weapon/part也是表达式、留空不限；damage/visible/location只能在里程碑的changes/dialogs/condition里通过`$$local.xxx`读
（`BuildEvent`构造触发条件时这三个固定填`0.0`/`false`/空串）。目前只有武器击杀带这些信息(此时`reason`固定为`"player"`，武器种类看`weapon`)，
其它死因这几个字段为空/0/假。

## 依赖关系

- 依赖：`common/utility.h`（`ValueType`）、`common/error.h`、`common/json.h`（`PuzzleResultEvent::SetFromJson`）、`expression.h`
  （`ScriptContext`、`EvaluateExpression`）。
- 被谁依赖：`Core/story/milestone.h`（`Milestone::triggers`）、`Core/story/script.h`（`Script::BuildEvent`/`MatchEvent`）、
  `Core/story/story.cpp`及`Forever/Framework/ForeverStoryFrameworkComponent`（构造并广播事件）、`expression.cpp`
  （`VariableExpression::Evaluate`调`Event::GetLocalValue`）。

## 待办/后续阶段

- 上面"没有字段逻辑的13种"事件：`Match`字段级逻辑、`GetLocalValue`、`BuildEvent`的JSON分支，被点名实现时一起补。
