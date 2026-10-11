# society.h / society.cpp

## 职责

`Society`：Job+Organization域的聚合入口。不引入Calendar（"哪几天上班/几点
上下班"）——直接写进具体`JobMod`子类的`DailyPlan`里，详见`job.md`。三件事：

1. **`Init(components)`**：按地图里所有`Component`加权随机分配`Organization`
   （每个`Organization`自己遍历claimed components设计Job），算法见
   `organization.md`"组织分配算法"一节。
2. **`RecruitCitizens(citizens, currentYear)`**：把成年市民随机匹配到还空缺的
   Job上，见`organization.md`"招聘"一节。
3. **`GenerateEmploymentHistory(citizens, currentYear)`**：反推入职历史+生成
   同事关系，见下"四类人际关系生成：同事关系"一节。

## 关键设计

### 持有方式

和`Map`/`Populace`/`Story`一样，直接挂在`AForeverFrameworkActor`身上
（`Society* society`成员，`EnsureSocietyGenerated()`里`new`+`Init`+
`RecruitCitizens`一次做完，`EndPlay`/析构函数里`delete`）。
`EnsureSocietyGenerated()`假定`map`/`populace`都已经生成好
（`map->GetAllComponents()`/`populace->GetCitizens()`/
`populace->GetCurrentYear()`），调用方（`AForeverFrameworkActor::
BeginPlay()`）负责保证`EnsureMapGenerated()`/`EnsurePopulaceGenerated()`已经
先跑过。

`Society`不知道`Map`/`Populace`的存在——`Init`只接收一份
`vector<Component*>`（由`AForeverFrameworkActor::EnsureSocietyGenerated()`调
`map->GetAllComponents()`拍平得到），`RecruitCitizens`只接收
`vector<Citizen*>`+年份，和`Populace::Init`只接收一个`int accommodation`同一个
解耦方向。

### Factory引用成员

不自己持有`ModLoader`/`OrganizationFactory`/`JobFactory`/`ScriptFactory`——
绑定`Registry::Get()`对应的三个Factory引用成员（`OrganizationFactory&`/
`JobFactory&`/`ScriptFactory&`），mod dll的发现/注册（`LoadLibrary`+
`RegisterConcept`）只在整个UE进程生命周期里发生一次，归`Registry`全局管，和
`Map`/`Populace`/`Story`的写法一致，见`Source/Core/common/registry.md`。

### Job的timer在Populace，Organization的timer在Society——两套独立timer

**Job的调度由`Populace::Tick`驱动**（`Populace`持有`jobTimerSet`，遍历
`Citizen::GetJob()`，见`job.md`"驱动方式"一节）；**Organization自己的调度由
`Society::Tick`驱动**（`Society`持有`organizationTimerSet`，遍历
`organizations`，见`organization.md`"Organization自己的DailyPlan/ExecNode"
一节）。两者的每帧处理上限（`kMaxJobTimersPerTick`/
`kMaxOrganizationTimersPerTick`，都是4）互不共享。

`AForeverFrameworkActor::Tick`每帧分别调用`populace->Tick(...)`和
`society->Tick(...)`，两个回调都是`(实体指针, const vector<Change*>&)`签名，
产出的`Change*`所有权全部留在产出它们的`JobMod`/`OrganizationMod`实例身上，
回调只读值使用、不`delete`，见`job.md`"跨DLL数据传递约束"一节。两个`Tick`都有
一个`PostHandle* post`末位参数，由`AForeverFrameworkActor::Tick`现场构造一个
`PostImplement`传入、原样透传到`Organization::DailyPlan/ExecNode`再到
`OrganizationMod`，供mod按需查citizen家/工位地址，见`job.md`"按需查地址：
PostHandle参数"一节（`ShopOrganization`不重写`DailyPlan`/`ExecNode`，用不上
这个参数，但接口和`JobMod`保持对称）。回调体本身只构造per-entity的
`ScriptContext`（`context.self = organization->GetScript()`），然后对每个
`Change*`转发给`AForeverFrameworkActor::ApplyChange`统一消费，不自己手写
`dynamic_cast` dispatch，见`Source/Forever/Framework/
ForeverFrameworkActor.md`"统一的Change消费入口：`ApplyChange`"一节。

### 四类人际关系生成：同事关系（`GenerateEmploymentHistory`）

`AForeverFrameworkActor::EnsureSocietyGenerated()`里`RecruitCitizens(...)`
之后立刻调用一次——依赖`RecruitCitizens`已经把当前在职citizen分配进`Job`，也
依赖`Citizen::GetLastGraduationYear()`（`Populace::Init()`里
`GenerateEducations()`已经算好，见`populace.md`"四类人际关系生成"一节）。

**入职历史反推——逐年往回推、递归到底，不是只反推一层前任**：每个当前有人在
职的`Job`维护一条"待定链条"：链条头是"目前认为占着这个职位的citizen"+"这个人
干到哪一年"（初始是"当前在职者，`-1`表示至今仍在干"）。从`currentYear`起逐年
往回推，每年对每条链条做一次概率判定（`kHireEventChancePercent`，15%，对应
平均约6-7年一任）——"这位是不是恰好这一年入职的"：
- 命中就写一条`JobExperience`（`beginYear`=判定到的年份，`endYear`=链条已知
  的"干到哪一年"），然后从空闲成年人池（没有当前工作、年满18岁的citizen）里
  随机挑一个满足"在接任前一年就已年满18岁"的前任接上链条，继续往更早的年份
  判定；挑不到前任就中断链条（这个人就是这个职位链条能追溯到的最早一任）。
- 没命中就把年份继续往前推一年，下次再判定同一个人。
- 链条头被推到自己"满18岁/最后一次毕业年份"（`GetLastGraduationYear()`，没
  上过学的成年人退化成18岁）这个硬下限时也直接终止，不再继续找前任。

循环的外层年份下限（`floorYear`）取所有初始在职者硬下限里最早的一个——持续到
"空闲成年人池被榨干找不到下一个前任"或者"链条头推到硬下限"为止，一个职位背后
可能挂出好几任前任。前任和继任者的任期没有重叠，不算同事，不生成
`JobExperience`/`acquaintances`，只在推导过程中占用/腾出空闲成年人池的名额，
让"职位数量长期不变"这个假设看起来合理。

**同事关系——从上面"一"的JobExperience派生出"多"**：所有`JobExperience`写完
后，对每个`Organization`收集当前在职者列表，两两组合按固定概率
（`kColleagueChancePercent`，40%）互相`AddAcquaintance`+随机`Relation`数值
（familiarity/trust中等，competing比同学/亲属更高一些，体现职场竞争）——不是
全组织互相认识，同一个组织里的人不一定都互相认识，和同学关系"全班互相认识"
（见`populace.md`）形成密度差异。两个citizen互为同事的"证据"是各自都有一条
`JobExperience`指向同一个`Organization`且任期重叠（现在都是ongoing，天然
重叠），不靠额外`new`的`Experience`表达这层"多"的关系，见`experience.md`
"关键设计"一节。

### `ApplyChange`

`Society::ApplyChange(const Change*, const ScriptContext&)`——目前没有任何
Change子类是Society域自己认识、需要处理的，空实现，也不打"未实现"警告（同一个
Change会被`AForeverFrameworkActor::ApplyChange`转发给全部六个Core域，只有
`Story::ApplyChange`保留兜底警告，避免六个域各打一遍重复日志）。等Society域
真的长出需要处理的Change子类时再补内容。

## 依赖关系

- 依赖：`Dependence/society/organization_mod.h`/`organization_factory.h`、
  `Dependence/society/job_mod.h`/`job_factory.h`、`Core/story/script_factory.h`、
  `Core/society/organization.h`/`job.h`、`Core/map/component.h`、
  `Core/populace/citizen.h`、`Core/populace/experience.h`（`JobExperience`，
  `GenerateEmploymentHistory`用）、`Core/common/registry.h`。
- 被谁依赖：`Core/common/implement.h`（`PostImplement`构造函数）、
  `Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（持有+`Tick`驱动，
  `ApplyChange`转发）。
