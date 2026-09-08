# change.h / change.cpp

原样移植自旧工程`E:\Projects\Forever_UE\Source\Dependence\story\change.h/.cpp`，未做任何
修改（无windows/UE类型依赖）。和`condition.md`/`event.md`同属阶段4-0共享脚本引擎三件套，是
其中"脚本要做什么"的一半——`condition.h`负责求值表达式，这里定义脚本能触发的**具体动作词汇
表**。

## 职责

`Change`是所有"动作"的抽象基类，只有一个`GetType()`（返回固定字符串标识，如`"spawn_npc"`/
`"give_estate"`）和一个可选的`Condition`（`SetCondition`/`GetCondition`——这个动作是否执行，
还可以再受一层条件控制，和`condition.h`共用同一套表达式语法）。文件里定义了近40个`Change`
具体子类，每个对应旧工程story脚本JSON里的一种动作，例如：

- 变量/状态：`SetValueChange`（赋值）、`RemoveValueChange`、`GlobalSettingChange`（改全局
  设置）、`DeactivateMilestoneChange`（停用某个里程碑）。
- 叙事流程：`GlobalMessageChange`（全局广播消息）、`AddOptionChange`/`RemoveOptionChange`
  （对话选项增删）、`AddGlobalChange`/`RemoveGlobalChange`（全局选项增删）、`GameEndChange`。
- NPC/玩家：`SpawnNpcChange`/`RemoveNpcChange`（生成/移除NPC，带一整套形象/姓名/职业/调度器
  字段）、`TeleportCitizenChange`/`TeleportPlayerChange`（瞬移）、`NPCNavigateChange`（自动
  导航）、`PlayerInjuredChange`/`PlayerCuredChange`/`PlayerIllChange`/`PlayerRecoverChange`/
  `PlayerSleepChange`（健康与作息状态变化）、`ChangeCultivationChange`/`ChangeWantedChange`
  （修炼/通缉等数值变化，具体玩法待阶段4确认）。
- 物品/资产/财务：`GiveObjectChange`/`RemoveObjectChange`、`GiveEstateChange`/
  `RemoveEstateChange`（房产）、`GiveVehicleChange`/`RemoveVehicleChange`、
  `BankTransactionChange`（存取款，`name`为空表示对象是玩家本人，是多处"接收者/所有者"字段
  共用的约定）。
- 场景/演出：`EnterVehicleChange`/`LeaveVehicleChange`、`OpenShopChange`、
  `StartPuzzleChange`（启动小游戏）、`LaunchElevatorChange`、`PlayVideoChange`/
  `PlayBgmChange`/`StopBgmChange`、`ChangeWeatherChange`/`ChangePolicyChange`。
- 控制结构：`ForRangeChange`（for循环，持有子`Change*`列表，析构时递归`delete`，是唯一会
  管理子对象生命周期的`Change`）、`PlaceHolderChange`（占位符，仅带一个`label`标签，用途待
  阶段4读Script业务代码时确认）。

## 关键设计

- **每个具体`Change`都是纯数据+`GetType()`字符串标签，没有`Execute()`之类的执行方法**——
  这是有意的：`change.h`本身只是"动作的数据描述"（比如"给谁多少钱"），真正**怎么执行**
  （改哪个domain的哪个数据结构）要等阶段4-6迁移`Script`/`Milestone`时才实现，那里大概率会按
  `GetType()`字符串switch/查表分发到各个domain的实际接口。**不要在这里找Execute方法，
  它属于Core层的Script系统，不属于这个纯数据词汇表。**
- **`ChangeValue`是文件末尾的`std::variant`别名，收纳了除`Change`本身、`ForRangeChange`、
  `PlaceHolderChange`外的全部具体子类**——`ForRangeChange`被排除是因为它持有嵌套的
  `Change*`列表（variant存值语义不适合），`PlaceHolderChange`被排除的原因不明显，阶段4接触
  到`ChangeValue`的实际用途（大概率是脚本JSON反序列化时的目标类型）时需要留意这两个例外是
  故意的还是遗留的不一致。
- **`GetType()`统一实现成`static const string type = "..."; return type;`**——每个子类返回
  的字符串就是脚本JSON里这个动作的`type`字段值（如`"spawn_npc"`），是字符串→具体`Change`
  子类反序列化的关键字，但反序列化逻辑本身不在这个文件里（不在Dependence层）。
- **绝大多数字段类型是`std::string`，即使语义上是数值**（如`SetValueChange::value`、
  `ChangeTimeChange::delta`）——这是因为这些字段的值本身也可能是一段要通过`Condition`求值的
  表达式（如`"$$level + 1"`），不是写死的字面量，交给消费方在真正执行时调用
  `Condition::EvaluateValue`求值，而不是在这一层就固定成某个具体类型。

## 依赖关系

- 依赖：`common/utility.h`、`common/error.h`（间接，通过`condition.h`）、`condition.h`
  （`Change::condition`成员）。文件顶部有`#undef GetMessage`/`#undef GetObject`——这是旧工程
  为了避免和Windows SDK的宏（`GetMessage`是`user32.h`的消息循环API宏、`GetObject`是GDI
  宏）撞名而做的防御性`#undef`，因为`GlobalMessageChange::GetMessage()`和
  `GiveObjectChange::GetObject()`两个方法名恰好和这两个Windows宏同名；这两行原样保留（新
  工程`utility.h`虽然已经不在头文件里直接include `<windows.h>`了，但不能保证所有引入本文件
  的编译单元都没有间接拉到`<windows.h>`，保留`#undef`是零成本的防御）。
- 被谁依赖：`story/script.h`（Core层，阶段4-6迁移）大概率会用这套词汇表描述"某个剧本节点要
  执行的变化列表"；`event.h`不依赖`change.h`（两者是并列关系，都依赖`condition.h`）。

## 待办/后续阶段

- 阶段4-6（Story域业务内容）：给每个`Change`子类接上真正的执行逻辑（分发到Map/Populace/
  Society/Traffic/Player等domain的具体接口），并确认`PlaceHolderChange`的实际用途、
  `ChangeValue`排除`PlaceHolderChange`是否有意为之。
