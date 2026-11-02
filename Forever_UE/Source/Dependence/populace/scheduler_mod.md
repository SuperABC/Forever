# scheduler_mod.h / scheduler_mod.cpp

## 职责

`SchedulerMod`：每个市民挂一个的行为调度器定义，负责**下班之后**的行为。上班行为完全由`Citizen::GetJob()`负责(`Populace::Tick`驱动`Job::DailyPlan`/`ExecNode`)。

## 关键设计

- 结构和`JobMod`一致：`DailyPlan(currentTime, post)`生成今天的调度表，直接写进`this->plans`(节点名->今天触发的具体时间)；`ExecNode(node, post)`执行一个到期节点，直接`new Change*`写进`this->changes`(所有权留在这个mod实例身上，不转移给调用方)。这套调度机制和milestone/JSON完全独立，见`Core/society/job.md`"DailyPlan/ExecNode"一节。
- **`plans`/`changes`是mod实例自己的成员字段**：分配/写入/析构全程在同一侧，Core侧只读、不`delete`里面的`Change*`，和`BuildingMod::floors/singles/rows`同一个跨DLL安全模式。
- **`occupantName`**：当前绑定的市民姓名，由`Scheduler`在调用`ExecNode`前同步进来，供构造需要知道"是谁"的`Change`使用，和`JobMod::occupantName`同一个用途。
- **`SetProperty()`两段式**：具体子类在这里填`scriptModName`/`milestoneNames`，不在构造函数里赋值。Script配置和`JobMod`/`OrganizationMod`同一套"mod自己声明意图，Core只读"的字段模式。
- `post`(`PostHandle`)：向Core发起查询的句柄，同`JobMod::DailyPlan`，不需要这个能力可以忽略。

## 依赖关系

- 依赖：`common/mod_state.h`、`common/utility.h`(`Time`)、`common/handle.h`、`Change`(前向声明)。
- 被谁依赖：`scheduler_factory.h`创建它；`Core/populace/scheduler.h`消费它；具体实现在`Source/Basic/populace/scheduler_basic.*`。

## 待办/后续阶段

- 无。
