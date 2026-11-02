# scheduler_basic.h / scheduler_basic.cpp

## 职责

`SchedulerBasic`(`scheduler_basic`)：默认的下班调度器实现，**占位**：`DailyPlan`留空、不产出任何调度节点。

## 关键设计

- 不写实际测试逻辑——`DailyPlan`/`ExecNode`这套机制已经在`Job`/`Organization`验证过，和`EmptyJob::DailyPlan`同一个"占位，不产生任何调度"的做法。
- `SetProperty()`的Script配置指向一个空的`schedule_empty.script`(`Resource/Story/schedule_empty.script`)。
- **`static float GetPower()`返回1.f**：`RegisterModSchedulers`注册时作为`PowerFunc`传给`SchedulerFactory::RegisterScheduler`，`Populace::AssignSchedulers()`据此建CDF做加权随机分配，见`Core/populace/populace.md`。

## 依赖关系

- 依赖：`populace/scheduler_mod.h`。
- 被谁依赖：`basic.cpp`注册；`Core/populace/scheduler.h`消费。

## 待办/后续阶段

- 没有真正的下班行为，等有具体设计时从占位升级。
