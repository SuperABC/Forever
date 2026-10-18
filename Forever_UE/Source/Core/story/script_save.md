# script_save.h / script_save.cpp

## 职责

全部 Script 的运行时状态的存档读写，类名 `ScriptSaveCodec`，写成二进制段 `world.scripts`（段名定义在 `common/save.h`）。每个 Script 存：
- 每个里程碑的 `premise`；
- 活动里程碑列表 `actives`，按名字记，保持原顺序；
- 变量 `variables`，`ValueType` 按类型下标加值写出；
- ScriptMod 自己的状态。

## 关键设计

- **不在这里创建 Script**。读档时，各个持有者已经按原来的方式建好了 Script：
  - 门：Door 构造函数；
  - 组织、岗位：Organization/Job 构造函数；
  - Scheduler、车辆：各自的构造函数；
  - 主线：`Story::Init`。

  它们读的是同样的 `.script` 文件。这里按持有者的稳定键找到对应的 Script，把状态覆盖回去：

  | 持有者 | 键 |
  |---|---|
  | 主线、全局变量池 | `story.main`、`story.system` |
  | 门 | `door:<门id>` |
  | 组织 | `org:<组织下标>` |
  | 岗位 | `job:<组织下标>:<岗位下标>` |
  | Scheduler | `scheduler:<市民下标>` |
  | 车辆 | `vehicle:<车名>` |

  按键排序写出。找不到持有者、或者持有者现在用的 ScriptMod 类型和存档时不同的条目跳过并打警告。
- **读档跳过 game_start**。这一段存在时，`AForeverFrameworkActor::EnsureStoryGenerated` 不再广播 game_start：脚本已经在存档时的进度上，game_start 里做过的事在存档里都已经是结果，包括：
  - 给市民、车辆、门加交互选项；
  - 随机交给玩家一个市民；
  - 各个 milestone 的推进。

  对应地：
  - 门、市民、车辆的交互选项改为由地图、人口、交通段按存档恢复；
  - 玩家操控谁由 UE 段恢复（`AForeverFrameworkActor::RestoreControlledPawn`）。

  没有这一段的旧存档照新开局广播，那时其它段也不恢复交互选项，避免重复。
- **里程碑结构不存**。里程碑本身（触发、对话、后续）来自 `.script` 文件缓存。这里只存每个节点的 `premise`，`subsequents` 指针由 `ReadMilestones` 重新建好。

## 依赖关系

- 依赖：`story/script.h`（friend）、各个持有者的公开接口、`common/save_refs.h`。
- 被谁依赖：
  - `common/save.cpp`（`BuildArchive`，传入完整的 `SaveWorld`）；
  - `Forever/Framework/ForeverFrameworkActor.cpp`；
  - `Forever/Mod/ForeverSaveSubsystem.cpp`（`Forever.SaveRoundTrip`）。

## 待办 / 后续

- 对话队列进行中、谜题进行中时拒绝保存（P6），因为这些中间状态不存。
