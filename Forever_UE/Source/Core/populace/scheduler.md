# scheduler.h / scheduler.cpp

## 职责

`Scheduler`：每个市民独占持有一个的行为调度器（`Citizen::GetScheduler()`），负责
citizen**下班之后**的行为。市民上班的行为由`Citizen::GetJob()`负责
（`Populace::Tick`驱动`Job::DailyPlan`/`ExecNode`，见`job.md`）。结构上是
`Core/society/job.h`的照抄，只是所有权归属和分配时机不同（见下）。`SchedulerBasic::
DailyPlan`留空，不产出任何调度节点，和`EmptyJob::DailyPlan`同一个"占位，不产生
任何调度"做法。

## 关键设计

### 和`Job`的差异：所有权归属+分配时机

`Job`由`Organization`持有所有权，构造时不一定已经绑定occupant。`Scheduler`没有
类似"组织"这样的自然归属者——最自然的归属就是它服务的那个市民自己：

- **`Citizen`自己独占持有`Scheduler*`**（不是`Populace`/某个"SchedulerOrganization"），
  `Citizen`因此有一个真正的析构函数`~Citizen() { delete scheduler; }`，见
  `citizen.md`。
- **`Scheduler`从`new`出来那一刻就唯一绑定一个`Citizen`**，构造函数直接收
  `Citizen* citizen`参数，不需要`Job`那样的`SetOccupant`事后绑定——`citizen`只是
  一个不持有所有权的裸指针，供`ExecNode`前同步`occupantName`用（和`Job::
  occupant`用途一样，只是`Job`是先有Job后有occupant，`Scheduler`是先有citizen后
  有Scheduler）。
- **分配时机**：`Populace::AssignSchedulers()`在`GenerateCitizens(target)`跑完、
  `citizens`列表已经就绪之后调用一次，`Populace::Init()`结尾统一编排，见
  `populace.md`"AssignSchedulers"一节。

### 加权随机分配

对所有已注册的Scheduler类型累加权重建CDF，对每个citizen roll一个随机数选中一个
类型、`new`一个`Scheduler`实例。权重接口是`OrganizationFactory`同款的
`RegisterOrganization(id, creator, deleter, PowerFunc power)`+`GetPower(id)`
单个查询（`Source/Dependence/society/organization_factory.h`），`Society::Init`
已经在用这个接口跑同一套"建CDF、roll随机数选中"逻辑。`SchedulerFactory`照这个
写法补齐`PowerFunc`/`GetPower`，`Populace::AssignSchedulers()`的CDF算法和
`Society::Init`是同一套写法，见`populace.md`。

`SchedulerBasic::GetPower()`返回`1.f`，`EmptyScheduler::GetPower()`返回`0.f`
（和`EmptyOrganization::GetPower()`同一个"占位类型权重为0，永远不会被随机选中，
除非它是唯一注册的类型"约定）——当前`config.json`的`"scheduler_mods"`只启用了
这两个类型时，`AssignSchedulers()`实际上总是选中`scheduler_basic`，这是预期
行为，不是bug。

### Script配置：和`JobMod`/`OrganizationMod`同一个模式

`SchedulerMod`新增`scriptModName`/`milestoneNames`两个普通成员字段（默认
`"script_empty"`/空），具体子类在自己的构造函数里直接赋值，`Scheduler`的构造函数据此建
`Script`+`ReadMilestones`，和`Job::Job`/`Organization::Organization`逐字照抄，
见`job.md`"Script配置"一节。`SchedulerBasic`配的是`scriptModName = "script_empty";
milestoneNames = { "schedule_empty" };`——`Resource/Story/schedule_empty.script`
是一个空的`{"milestones": []}`，纯粹是为了让"Mod指定Script类和script脚本文件"
这条约定在Scheduler这个concept上也有一个可以指向的真实文件，不代表要跑通任何
milestone内容。`script->SetValue("name", ...)`填的是`citizen->GetName()`（不是
`mod->GetName()`）——`Scheduler`从构造起就唯一绑定一个citizen，直接用citizen自己
的名字给`$self.name`更贴切，和`Job`用`mod->GetName()`（因为一个Job类型可能有多个
实例，需要mod自己的计数器区分）语义不同。

### `Populace::Tick`里独立的第三套timer

和`Job`（`jobTimerSet`）、`Organization`（`organizationTimerSet`，在
`Society::Tick`里）完全平行的第三套独立timer——`schedulerTimerSet`，放在
`Populace::Tick`里（`Scheduler`和`Job`一样挂在`Citizen`身上，`Populace`本来就要
遍历所有citizen），结构、每帧处理上限（`kMaxSchedulerTimersPerTick`）、驱动方式
（`crossedDay`时生成今天的调度表、每帧弹出到期节点执行）都和`jobTimerSet`那一段
逐字照抄，只是换成`citizen->GetScheduler()`，共用同一个`onActions`回调——回调
签名本来就是通用的`(Citizen*, const vector<Change*>&)`，不关心Change是Job产的
还是Scheduler产的，最终都统一走`AForeverFrameworkActor::ApplyChange`消费，见
`ForeverFrameworkActor.md`。

## 依赖关系

- 依赖：`Dependence/populace/scheduler_mod.h`/`scheduler_factory.h`
  （`SchedulerMod`/`SchedulerFactory`）、`Core/story/script.h`/
  `story/script_factory.h`（`Script`）、`common/config.h`
  （`Config::GetScriptPath()`）、`Core/populace/citizen.h`
  （`Citizen::GetName()`）。
- 被谁依赖：`Citizen`（持有所有权，`~Citizen()`里delete）、
  `Populace::AssignSchedulers()`（构造时`new Scheduler(...)`）、
  `Populace::Tick`（驱动调度）。
