# event.h / event.cpp

原样移植自旧工程`E:\Projects\Forever_UE\Source\Dependence\story\event.h/.cpp`，未做任何修改
（无windows/UE类型依赖）。和`change.md`/`condition.md`同属阶段4-0共享脚本引擎三件套——
`change.h`是脚本"能做什么"的词汇表，这里的`event.h`是脚本"响应什么"的词汇表：游戏运行时发生
的具体事情（进了哪个房间、和谁对话选了哪个选项……）都对应一种`Event`子类，Story系统按
`Match()`拿运行时发生的事件去比对脚本里声明的触发条件。

## 职责

`Event`基类除了`GetType()`/`Condition`（和`Change`一致），多一个纯虚方法
`Match(Event* e, getValues)`——**判断"我"（脚本里声明的、可能带通配字段的模板事件）是否匹配
"e"（运行时真实发生的具体事件）**。文件里定义了32个具体`Event`子类，按用途分组：

- 系统/剧情：`GameStartEvent`（游戏开始）、`GlobalMessageEvent`（收到全局广播）、
  `OptionDialogEvent`/`GlobalDialogEvent`（对话选项被选中）、`SpeakingFinishEvent`（一段对话
  播放完毕）。
- 场景进出：`EnterZoneEvent`/`LeaveZoneEvent`、`EnterBuildingEvent`/`LeaveBuildingEvent`、
  `EnterRoomEvent`/`LeaveRoomEvent`——三组"区域/建筑/房间"分别成对，粒度从粗到细。
- 玩法结果：`PuzzleResultEvent`（小游戏结算）、`TransactionResultEvent`（交易结算）、
  `ObjectResultEvent`（物品操作结算）、`TimeUpEvent`（`change.h`里`CreateTimerChange`创建的
  计时器到时）、`NpcArriveEvent`（NPC抵达目的地，对应`NPCNavigateChange`导航完成）、
  `UseAssetEvent`（使用了某个资产/道具）。
- 人物状态：`NPCMeetEvent`（与NPC相遇）、`CitizenBornEvent`/`CitizenDeceaseEvent`（市民出生/
  死亡）、`PlayerInjuredEvent`/`PlayerCuredEvent`/`PlayerIllEvent`/`PlayerRecoverEvent`/
  `PlayerRestEvent`/`PlayerSleepEvent`（健康与作息，和`change.h`同名的`Change`一一对应，一个
  是"让它发生"的指令，一个是"它发生了"的通知）、`CultivationChangeEvent`/
  `WantedChangeEvent`（修炼/通缉变化）、`PlayerArrestedEvent`/`PlayerReleasedEvent`（被捕/
  释放）、`WeatherChangeEvent`/`PolicyChangeEvent`（天气/政策变化）。

## 关键设计

- **`Match`的标准实现模式**：先比较`GetType()`是否相同（类型不同直接不匹配），
  `dynamic_cast`成具体子类失败也不匹配，然后逐字段比较——**如果模板事件的某个字符串字段非
  空**，就把它当一段`Condition`表达式**现场解析求值**，转成字符串后和运行时事件对应字段的
  **原始值**比较是否相等；**字段为空**则视为通配（不比较，任意值都算匹配）。例如
  `GlobalMessageEvent::Match`：`message`字段非空时，`Condition().ParseCondition(message)`
  求值后和`other->message`比较；这样脚本里的事件模板可以写`"$$level > 3"`这种带表达式的字段
  去匹配运行时的具体值，而不必是字面量精确匹配。这个"模板字段是表达式、运行时字段是字面量"
  的不对称设计是整个匹配机制的核心，阶段4-6实现Story系统时要复用同一模式，不要误以为两边
  都该走`Condition`解析。
- **部分事件用"空即通配"以外的特判**，如`OptionDialogEvent::Match`：当模板的`id`和`name`
  都是"未设置"状态（`id == -1 && name == ""`）时，直接退化成只比较`option`文本，这类特判
  分散在各个`Match`实现里，阶段4接入具体某个事件时要单独确认，不能假设全部字段都遵循"空则
  通配"这一条规则。
- **没有类似`change.h`的`EventValue`variant别名**——`event.h`定义了32个子类但不像`change.h`
  那样在文件末尾打包成一个`std::variant`，阶段4-6如果需要按类型枚举/分发所有Event子类，要
  自己决定用什么机制（`GetType()`字符串表 + 工厂，或者手写variant），这不是遗漏，是旧工程
  本来就没有这个别名。
- **文件顶部只有`#undef GetMessage`（没有`#undef GetObject`）**——因为`event.h`里只有
  `GlobalMessageEvent::GetMessage()`会和Windows `user32.h`的`GetMessage`宏撞名，没有
  `GetObject`同名方法，和`change.h`的两个`#undef`是各自按实际用到的方法名精确添加，不是
  无脑复制一套。

## 依赖关系

- 依赖：`common/utility.h`、`common/error.h`（间接，通过`condition.h`）、`condition.h`
  （`Event::condition`成员，以及各`Match`实现内部现场构造`Condition`求值）。
- 被谁依赖：`story/script.h`（Core层，阶段4-6迁移）大概率会持有一份"当前脚本关心哪些
  `Event`模板"的列表，每当UE侧真实发生某种事情就构造一个对应的具体`Event`实例广播过去，逐个
  调用`Match`寻找命中的脚本分支。

## 待办/后续阶段

- 阶段4-6（Story域业务内容）：确定运行时"谁负责在什么时机构造具体`Event`实例并广播"（大概率
  是各domain的Core类在状态变化时主动通知Story系统），以及`Match`失败/命中后分别触发哪些
  `change.h`动作的完整链路。
