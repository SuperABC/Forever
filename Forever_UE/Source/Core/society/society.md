# society.h / society.cpp

## 职责

`Society`：Job+Organization域的聚合入口（不再是空骨架）。这次迁移**不要Calendar**——
老工程Calendar只是"哪几天上班/几点上下班"，直接写进具体`JobMod`子类的`DailyPlan`里，
详见`job.md`。两件事：

1. **`Init(components)`**：按地图里所有`Component`加权随机分配`Organization`（每个
   `Organization`自己遍历claimed components设计Job），算法见`organization.md`"组织
   分配算法"一节。
2. **`RecruitCitizens(citizens, currentYear)`**：把成年市民随机匹配到还空缺的Job上，
   见`organization.md`"招聘"一节。

## 持有方式

和`Map`/`Populace`/`Story`一样，直接挂在`AForeverFrameworkActor`身上（`Society*
society`成员，`EnsureSocietyGenerated()`里`new`+`Init`+`RecruitCitizens`一次做完，
`EndPlay`/析构函数里`delete`）。`EnsureSocietyGenerated()`假定`map`/`populace`都已经
生成好（`map->GetAllComponents()`/`populace->GetCitizens()`/
`populace->GetCurrentYear()`），调用方（`AForeverFrameworkActor::BeginPlay()`）负责
保证`EnsureMapGenerated()`/`EnsurePopulaceGenerated()`已经先跑过。

`Society`不知道`Map`/`Populace`的存在——`Init`只接收一份`vector<Component*>`（由
`AForeverFrameworkActor::EnsureSocietyGenerated()`调`map->GetAllComponents()`拍平
得到），`RecruitCitizens`只接收`vector<Citizen*>`+年份，和`Populace::Init`只接收一个
`int accommodation`同一个解耦方向。

## Factory引用成员

不自己持有`ModLoader`/`OrganizationFactory`/`JobFactory`/`ScriptFactory`——绑定
`Registry::Get()`对应的三个Factory引用成员（`OrganizationFactory&`/`JobFactory&`/
`ScriptFactory&`），mod dll的发现/注册（`LoadLibrary`+`RegisterConcept`）只在整个UE
进程生命周期里发生一次，归`Registry`全局管，和`Map`/`Populace`/`Story`现在的写法完全
一致，见`Source/Core/common/registry.md`。

## Job的timer在Populace，Organization的timer在Society——两套独立timer

老工程把Job和Organization的到期节点混在同一个`Society::timerSet`里（用哪个指针非空
区分是Job还是Organization触发）。这次分开：**Job的调度由`Populace::Tick`驱动**（
`Populace`持有`jobTimerSet`，遍历`Citizen::GetJob()`，见`job.md`"驱动方式"一节）；
**Organization自己的调度由`Society::Tick`驱动**（`Society`持有
`organizationTimerSet`，遍历`organizations`，见`organization.md`"Organization自己的
DailyPlan/ExecNode"一节）。两者的每帧处理上限（`kMaxJobTimersPerTick`/
`kMaxOrganizationTimersPerTick`，都是4，照抄老工程默认值）也互不共享。

`AForeverFrameworkActor::Tick`每帧分别调用`populace->Tick(...)`和`society->Tick(...)`，
两个回调都是`(实体指针, const vector<Change*>&)`签名，产出的`Change*`所有权全部留在
产出它们的`JobMod`/`OrganizationMod`实例身上，回调只读值使用、不`delete`，见
`job.md`"跨DLL数据传递约束"一节。两个`Tick`都多了一个`PostHandle* post`末位参数，
由`AForeverFrameworkActor::Tick`现场构造一个`PostImplement`传入、原样透传到
`Organization::DailyPlan/ExecNode`再到`OrganizationMod`，供mod按需查citizen家/工位
地址，见`job.md`"按需查地址：PostHandle参数"一节（这次`ShopOrganization`不重写
`DailyPlan`/`ExecNode`，用不上这个参数，但接口和`JobMod`保持对称）。**这次重构
`AForeverFrameworkActor::Tick`后，回调体本身简化成只构造per-entity的`ScriptContext`
（`context.self = organization->GetScript()`），然后对每个`Change*`转发给
`AForeverFrameworkActor::ApplyChange`统一消费**，不再自己手写`dynamic_cast` dispatch，
见`Source/Forever/Framework/ForeverFrameworkActor.md`"统一的Change消费入口：
`ApplyChange`"一节。

## `ApplyChange`（这次重构`AForeverFrameworkActor::Tick`新增）

`Society::ApplyChange(const Change*, const ScriptContext&)`——目前没有任何Change子类是
Society域自己认识、需要处理的，空实现，也不打"未实现"警告（同一个Change会被
`AForeverFrameworkActor::ApplyChange`转发给全部六个Core域，只有`Story::ApplyChange`
保留兜底警告，避免六个域各打一遍重复日志）。等Society域真的长出需要处理的Change子类
时再补内容。

## 依赖关系

- 依赖：`Dependence/society/organization_mod.h`/`organization_factory.h`、
  `Dependence/society/job_mod.h`/`job_factory.h`、`Core/story/script_factory.h`、
  `Core/society/organization.h`/`job.h`、`Core/map/component.h`、
  `Core/populace/citizen.h`、`Core/common/registry.h`。
- 被谁依赖：`Core/common/implement.h`（`PostImplement`构造函数）、
  `Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（持有+`Tick`驱动，
  `ApplyChange`转发）。
