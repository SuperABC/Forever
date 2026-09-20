# organization.h / organization.cpp

## 职责

`Organization`：一份具体组织的实体（比如一家商店）。独占持有一个`OrganizationMod*`
（`Dependence/society/organization_mod.h`定义，具体类型如`ShopOrganization`）+一份
自己的`Script*`（这次一起补上——之前`Organization`完全没有Script，只有`Job`有，构造
方式和`Job::Job()`同一套机制，见下"Script配置"一节），可以
占多个`Component`（也可以要多种不同类型的`Component`，具体由`mod->requirements`决定）。
构造时**自己**遍历claimed components的每个workspace room、调
`mod->DesignJobsForRoom(...)`拿到要配的job类型字符串列表，逐条`new Job(...)`——
`Job`的分配/释放全程在Core侧（`OrganizationMod`所在的`Dependence`层看不到`Job`这个
Core类型，见下"分层约束"）。

## 分层约束：Dependence不能反过来依赖Core

`OrganizationMod`定义在`Dependence/society/`，`Job`/`Component`/`Room`是`Core`层的类
——`Dependence`绝对不能出现这些Core类型的引用（`Core`依赖`Dependence`，反过来不行）。
照抄`BuildingMod::Layout`/`Assign`只出现`Quad`/`Road`/`Lot`这几个`Dependence`层类型
的既有模式，`OrganizationMod`的接口只用字符串/int/`Time`/`Change*`表达"要什么""配
什么"：

```cpp
virtual void ComponentRequirements() = 0; // 写进this->requirements
virtual void DesignJobsForRoom(const std::string& componentType, const std::string& componentName,
	const std::string& roomType, const std::string& roomName, int workspaceCapacity) = 0; // 写进this->vacancies
```

真正的`Component`/`Room`遍历和`new Job(...)`全部在`Organization`（Core侧代码）里做：
构造函数遍历`components`里每个`Component::GetRooms()`，对`IsWorkspace()`的room先
`mod->vacancies.clear()`，调`mod->DesignJobsForRoom(component->GetType(),
component->GetName(), room->GetType(), room->GetName(),
room->WorkspaceCapacity())`，再读`mod->vacancies`里的每一条字符串，各
`new Job(jobFactory, scriptFactory, 那个字符串, room)`，push进自己的`jobs`。

## 跨DLL数据传递约束

同`job.md`的原则：`OrganizationMod::requirements`/`vacancies`/`plans`/`changes`都是
mod自己的成员字段，`ComponentRequirements`/`DesignJobsForRoom`/`DailyPlan`/`ExecNode`
是mod自己重写的虚方法直接写自己的成员字段；`Organization`只读，不`delete`
`mod->changes`里的指针（见`ExecNode`）。`Job*`对象则完全不存在这个问题——它们从头到尾
就是`Organization`（Core）自己`new`/`delete`的，`OrganizationMod`所在的`Dependence`
层根本不知道`Job`这个类型的存在。

## 组织分配算法（`Society::Init`，替代老工程`Society::Init`前半段+固定16次重试）

```
// 第一步：对每个已注册组织类型各构造一个临时mod实例查requirements，读完立刻销毁——
// 这里直接操作organizationFactory，不经过Organization这层Core包装(包装类构造函数会
// 立刻尝试从components设计Job，这一步还没有分配到任何component，不适用)。
requirementsByType: map<组织类型id, unordered_map<Component类型, pair<min,max>>>
for id in organizationFactory.GetRegisteredIds():
    temp = organizationFactory.CreateOrganization(id)
    temp->ComponentRequirements()
    requirementsByType[id] = temp->requirements
    organizationFactory.DestroyOrganization(temp)

componentsByType = 按GetType()把所有传入的Component*分组

loop:
    筛出"每种要求类型在componentsByType里剩余数量都>=min"且requirements非空的候选类型
    候选为空 -> 结束循环
    按organizationFactory.GetPower(id)加权CDF随机选一个候选类型
    对它的每种需求(type,[min,max])：实际数量=min+GetRandom(max-min+1)，clamp到剩余
    数量；从池子里摘取这些Component(移除)
    organizations.push_back(new Organization(...)) // 构造函数内部自己完成Job设计
```

不需要老工程`MAX_ALLOCATION_ATTEMPTS`固定次数重试——候选类型集合空了自然停止，每次
成功分配都会让至少一个类型的剩余数量减少，不会死循环。**`requirements`为空的组织类型
（比如`Forever_Mod/Empty`里纯占位的`EmptyOrganization`）会被跳过、不当候选**——这种
类型永远"满足"、永远不消耗任何Component，会让循环死循环。

## ShopOrganization（`Source/Basic/society/organization_basic.h/.cpp`）

用来测试"一个组织随机占1~2个`component_shop`"：

```cpp
void ShopOrganization::ComponentRequirements() {
	requirements.clear();
	requirements["component_shop"] = { 1, 2 };
}

void ShopOrganization::DesignJobsForRoom(const std::string& componentType, const std::string&,
	const std::string& roomType, const std::string&, int workspaceCapacity) {
	if (componentType == "component_shop" && roomType == "room_shop" && workspaceCapacity > 0) {
		vacancies.push_back("job_shop_saler");
	}
}
```

**每个`room_shop`类型的workspace room恰好配一个店员**（不管这个房间
`WorkspaceCapacity()`具体是多少，`workspaceCapacity==0`就不配）——不是"按容量数量重复
配"，接口本身支持"一个room配多个/多种不同类型Job"，只是这次用不上。`GetPower()`固定
返回`1.f`（目前只有这一种组织类型，权重值本身没有意义）。

## Script配置：和Job同一套`scriptModName`/`milestoneNames`机制

`OrganizationMod`这次一起加上了`scriptModName`/`milestoneNames`两个字段（默认
`"empty"`+空，和`JobMod`完全一样的安全模式，见`job.md`"Script配置"一节）。
`ShopOrganization`构造函数设`scriptModName = "empty"; milestoneNames =
{"organization_shop"};`，用来验证Organization的game_start广播链路。`Organization`
构造函数在`mod = factory->CreateOrganization(id);`成功之后（`mod`为空直接`return`，
`script`保持`nullptr`——不像`Job`那样即使mod为空也兜底建一个"empty"壳，因为
`Organization`构造函数本来就在mod为空时整个提前返回，不需要额外处理）：

```cpp
script = new Script(scriptFactory, mod->scriptModName);
for (const string& name : mod->milestoneNames) {
	script->ReadMilestones(Config::GetScriptPath(name));
}
script->SetValue("name", ValueType(string(mod->GetName()))); // 供milestone脚本里
	// $$self.name引用这个Organization的唯一名字，见job.cpp同款注释
```

`~Organization()`里`delete script;`。`GetScript() const`供`Organization`所在的
`Script`被外部（`UForeverStoryFrameworkComponent::BroadcastGameStart`）拿到并广播一次
`game_start`，见`ForeverStoryFrameworkComponent.md`——**这个广播编排逻辑完全在
Forever层，`Society`/`Organization`本身不提供任何"广播给所有下属Script"的聚合方法**，
`Society`只需要已有的`GetOrganizations()`/`Organization::GetJobs()`/`GetScript()`这几个
纯访问器就够了。

## 招聘（`Society::RecruitCitizens`）

收集所有`organizations`里`GetOccupant()==nullptr`的`Job*`到`vacancies`，收集所有
满足`GetAge(currentYear)>=18`且`GetJob()==nullptr`的`Citizen*`到`adults`，两边各自用
项目统一的`GetRandom(n)`做Fisher-Yates洗牌，按`min(size)`一一配对
（`vacancies[i]->SetOccupant(adults[i])`+`adults[i]->SetJob(vacancies[i])`）。比老
工程"每个成年人独立随机选组织、组织满了就从候选池踢出重试"简单很多，效果等价（随机
配对）。

## Organization自己的DailyPlan/ExecNode + timer（`Society::Tick`）

和`Job`结构对称，但驱动方是`Society`而不是`Populace`——两套完全独立的timer，见
`job.md`"驱动方式"一节。`Society`持有`organizationTimerSet`
（`std::set<std::tuple<Time, Organization*, std::string>>`），
`kMaxOrganizationTimersPerTick`(4)是这一份的独立上限。`DailyPlan`/`ExecNode`这次也和
`JobMod`同步加上了`PostHandle* post`参数（见`job.md`"按需查地址：PostHandle参数"
一节）——保持接口对称，即使目前没有具体组织类型真的用到。这次`ShopOrganization`不重写
`DailyPlan`/`ExecNode`（用基类默认空实现），这套设施目前没有真正产出任何Change，是为
将来"组织级调度"（比如发工资）预留的接口，不是死代码。

## 依赖关系

- 依赖：`Dependence/society/organization_mod.h`/`organization_factory.h`、
  `Core/society/job.h`（构造函数`new Job(...)`）、`Core/map/component.h`/`room.h`
  （遍历`GetRooms()`/`IsWorkspace()`/`WorkspaceCapacity()`）、`Core/story/script.h`/
  `story/script_factory.h`（`Script`）、`common/config.h`（`Config::GetScriptPath()`）。
- 被谁依赖：`Society`（持有所有权，`Society::Init`构造）、
  `UForeverStoryFrameworkComponent::BroadcastGameStart`（`GetScript()`+
  `Organization::GetJobs()`遍历广播game_start，见.md"Script配置"一节）。
