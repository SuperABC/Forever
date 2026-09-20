# job.h / job.cpp

## 职责

`Job`：一份具体工作岗位的实体（比如某个商店里的一个店员岗位）。独占持有一个`JobMod*`
（`Dependence/society/job_mod.h`定义，具体类型如`ShopSalerJob`）+一份自己独占的`Script*`
（`self.`前缀路由目标）。**`Job`由`Organization`持有所有权**（见`organization.md`），
`Citizen`只存一个不持有所有权的`Job*`指针（`Citizen::GetJob()`/`SetJob()`）。

这次迁移**不要Calendar**——老工程Calendar只是"哪几天上班/几点上下班"，这次直接写进具体
`JobMod`子类的`DailyPlan`里，`Job`不持有`Calendar*`。

## DailyPlan/ExecNode：纯C++通道，和milestone/JSON完全独立

`Job::DailyPlan(currentTime)`每天（`Populace::Tick`在`Player::CrossDay()`为true时调用，
见下"驱动方式"）转调`mod->DailyPlan(currentTime)`，`DailyPlan`直接写`mod`自己的
`plans`成员字段（node名->今天触发的具体时间）。

`Job::ExecNode(node)`到期时（同样由`Populace::Tick`驱动）转调`mod->ExecNode(node)`，
`ExecNode`直接`new Change*`写进`mod`自己的`changes`成员字段。**这套调度机制完全不查、
不碰、不关心这个Job的Script里是否有同名milestone**——milestone只能通过它自己原本的
`Script::MatchEvent`事件匹配流程被触发（比如将来有代码广播一个Event给这个Job的
Script），两者运行时完全独立，不做任何"按节点名找milestone"的合并逻辑（这是设计过程中
明确否决掉的一个方案，milestone和C++调度这次刻意保持互不相干）。`GetMilestoneFiles()`
这个可选钩子因此纯粹是给`Job`的`Script`提供milestone内容用的，和`DailyPlan`/`ExecNode`
无关；这次`ShopSalerJob`用不上这个能力，不重写（用基类默认空实现），构造出来的`Script`
就是一个纯变量容器。

## 跨DLL数据传递约束——谁分配的对象谁负责释放

`JobMod::plans`/`changes`是mod自己的成员字段：`DailyPlan`/`ExecNode`是mod自己重写的
虚方法，直接写自己的成员字段（分配、写入、随mod实例一起析构全程都在这个mod实例所在的
同一侧），和`BuildingMod::floors/singles/rows`已经在用的跨DLL安全模式一致。`Job::
ExecNode(node)`调用完`mod->ExecNode(node)`后只**读**`mod->changes`把里面的指针**值**
复制进这个函数自己新建的`vector<Change*>`返回——不`delete`、不接管这些`Change*`的所有权，
它们留给mod自己在下一次`ExecNode`调用开头（每个具体`JobMod::ExecNode`实现开头都会先
`for (auto* c : changes) delete c; changes.clear();`清掉上一次的残留）或mod实例最终
析构时清理。`Job`自己新建的返回值vector本身是Core分配的，安全。

`occupantName`：`JobMod`的一个纯字符串成员字段（不是`Citizen*`指针——`Dependence`层
看不到`Citizen`这个Core类型），`Job::ExecNode`调用`mod->ExecNode`之前会先同步
`mod->occupantName = occupant ? occupant->GetName() : ""`，供`ExecNode`构造
`NPCNavigateChange`这类需要知道"是谁"的Change使用。

## ShopSalerJob（`Source/Basic/society/job_basic.h/.cpp`）

天天上班，早上9点从家出门去商店，中午12点从商店下班回家：

```cpp
void ShopSalerJob::DailyPlan(const Time& currentTime) {
	plans.clear();
	plans["leave_home"] = Time(currentTime.GetYear(), currentTime.GetMonth(), currentTime.GetDay(), 9, 0);
	plans["leave_work"] = Time(currentTime.GetYear(), currentTime.GetMonth(), currentTime.GetDay(), 12, 0);
}
```

`ExecNode`两个节点都是`new NPCNavigateChange(occupantName, "workplace"/"home")`
（`NPCNavigateChange`是老工程已经实现过、这次连同`change.h`一起搬过来的Change类型，
字段是`Expression`不是纯字符串——用`Parse("\"字面量\"")`构造只求值出常量的表达式，见
`job_basic.cpp`的`MakeLiteralExpression`）。`destination`这次约定只用`"home"`/
`"workplace"`两个字面值，由`AForeverFrameworkActor::Tick`的回调解析成实际的`Room*`
（`"home"`→`citizen->GetRoom()`，`"workplace"`→`citizen->GetJob()->GetPosition()`）
后转发给`UForeverPopulaceFrameworkComponent::RequestWalk`，见
`ForeverPopulaceFrameworkComponent.md`"市民走路"一节。

## 驱动方式：Populace::Tick

和老工程一样，Script驱动市民的方式是`DailyPlan`+`ExecNode`，但这次触发点是
`Populace::Tick`（不是老工程的`Society::Tick`）——`Job`由`Citizen`直接持有引用
（`Citizen::GetJob()`），`Populace`本来就要遍历所有citizen，比按`Organization`→
`Component`→`Job`这条链路反过来找"这个job的occupant是谁"更直接。`Player::CrossDay()`
为true时遍历所有持有job的citizen生成今天的调度、塞进`Populace`自己的`jobTimerSet`
（`std::set<std::tuple<Time, Citizen*, std::string>>`，按时间排序）；不论是否跨天，
每帧从`jobTimerSet`弹出最多`kMaxJobTimersPerTick`(4)个到期节点执行——照抄老工程
"一帧内允许处理的timer上限"的设计，避免单帧处理过多到期节点卡顿。**Job的timer和
Organization的timer这次分开放**（老工程混在一个`Society::timerSet`里）：Job的timer
在`Populace`（`jobTimerSet`），Organization的timer在`Society`
（`organizationTimerSet`，见`organization.md`），两套完全独立，各自的每帧处理上限也
互不共享。

`Populace::Tick`的回调签名是`(Citizen*, const vector<Change*>&)`——调用方
（`AForeverFrameworkActor::Tick`）要按这个`Citizen*`决定`NPCNavigateChange`具体怎么
生效（查它当前是否有已生成的`ACitizenElement`），`Populace`自己不知道Actor层。

## 依赖关系

- 依赖：`Dependence/society/job_mod.h`/`job_factory.h`（`JobMod`/`JobFactory`）、
  `Core/story/script.h`/`story/script_factory.h`（`Script`）、`common/config.h`
  （milestone文件路径，若`GetMilestoneFiles()`非空）。
- 被谁依赖：`Organization`（持有所有权，构造时`new Job(...)`）、`Citizen`（不持有所有权
  的引用）、`Populace::Tick`（驱动调度）。
