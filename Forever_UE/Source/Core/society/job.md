# job.h / job.cpp

## 职责

`Job`：一份具体工作岗位的实体（比如某个商店里的一个店员岗位）。独占持有一个
`JobMod*`（`Dependence/society/job_mod.h`定义，具体类型如`ShopSalerJob`）+一份
自己独占的`Script*`（`self.`前缀路由目标）。**`Job`由`Organization`持有所有权**
（见`organization.md`），`Citizen`只存一个不持有所有权的`Job*`指针
（`Citizen::GetJob()`/`SetJob()`）。

不引入Calendar（"哪几天上班/几点上下班"）——直接写进具体`JobMod`子类的
`DailyPlan`里，`Job`不持有`Calendar*`。

## 关键设计

### DailyPlan/ExecNode：纯C++通道，和milestone/JSON完全独立

`Job::DailyPlan(currentTime, post)`每天（`Populace::Tick`在`Player::CrossDay()`
为true时调用，见下"驱动方式"）转调`mod->DailyPlan(currentTime, post)`，
`DailyPlan`直接写`mod`自己的`plans`成员字段（node名->今天触发的具体时间）。

`Job::ExecNode(node, post)`到期时（同样由`Populace::Tick`驱动）转调
`mod->ExecNode(node, post)`，`ExecNode`直接`new Change*`写进`mod`自己的
`changes`成员字段。`post`（`PostHandle*`，见下"按需查地址：PostHandle参数"
一节）两个方法都会透传，`DailyPlan`这次的`ShopSalerJob`实现用不上（生成调度表
不需要知道地址），只有`ExecNode`真正用它查地址。**这套调度机制完全不查、不碰、
不关心这个Job的Script里是否有同名milestone**——milestone只能通过它自己原本的
`Script::MatchEvent`事件匹配流程被触发，两者运行时完全独立，不做任何"按节点名找
milestone"的合并逻辑（milestone和C++调度刻意保持互不相干）。`scriptModName`/
`milestoneNames`这两个字段（见下"Script配置"一节）因此纯粹是给`Job`的`Script`
提供milestone内容用的，和`DailyPlan`/`ExecNode`无关；`ShopSalerJob`给
`milestoneNames`塞了`{"job_shop_saler"}`，用来验证Job的game_start广播链路。

### Script配置：`scriptModName`/`milestoneNames`

`JobMod`不能有按值返回容器的接口（Mod绝不能把STL容器按值返回给Core，这是跨DLL
安全的硬性规则）、也不能由Core硬编码"用哪个ScriptMod"/"文件放在哪"这类决策——
这两个决策都该由Mod自己声明。设计是"用`pair<ScriptModName, vector<MilestoneName
>>`在Mod侧声明意图，Core只负责按ScriptModName创建Script、按MilestoneName列表
挨个加载"：`JobMod`有两个普通成员字段（和`plans`/`changes`同一个"mod自己的
字段，Core只读"安全模式）：

```cpp
std::string scriptModName = "script_empty"; // 默认"script_empty"，纯粹要个能创建出来的ScriptMod壳
std::vector<std::string> milestoneNames; // 默认为空，每项是不含路径/扩展名的bare文件名
```

`ShopSalerJob`构造函数里设`scriptModName = "script_empty"; milestoneNames =
{"job_shop_saler"};`。`Job::Job()`：

`Job::Job()`先`mod->SetProperty()`，再用`MakeScript(scriptFactory, mod->scriptModName, mod->milestoneNames)`
创建`Script`并加载milestone（两者都空则没有脚本，`script`为nullptr），然后`script->SetValue("name", mod->GetName())`，
供milestone脚本里`$$self.name`引用这个Job的唯一名字（JobMod的`GetName()`）。

`Config::GetScriptPath(name)`（`Source/Core/common/config.h/.cpp`）按不含路径/
扩展名的bare文件名反查`.script`文件的实际绝对路径——文件实际存放在哪由
`config.json`的`resource_paths`数组决定（`["../Story"]`），`JobMod`不知道也不
需要知道用户把脚本文件放在哪，只管声明"我要哪个bare名字"。

### 跨DLL数据传递约束——谁分配的对象谁负责释放

`JobMod::plans`/`changes`是mod自己的成员字段：`DailyPlan`/`ExecNode`是mod自己
重写的虚方法，直接写自己的成员字段（分配、写入、随mod实例一起析构全程都在这个
mod实例所在的同一侧），和`BuildingMod::floors/singles/rows`已经在用的跨DLL
安全模式一致。`Job::ExecNode(node)`调用完`mod->ExecNode(node)`后只**读**
`mod->changes`把里面的指针**值**复制进这个函数自己新建的`vector<Change*>`返回
——不`delete`、不接管这些`Change*`的所有权，它们留给mod自己在下一次`ExecNode`
调用开头（每个具体`JobMod::ExecNode`实现开头都会先`for (auto* c : changes)
delete c; changes.clear();`清掉上一次的残留）或mod实例最终析构时清理。`Job`
自己新建的返回值vector本身是Core分配的，安全。

`occupantName`：`JobMod`的一个纯字符串成员字段（不是`Citizen*`指针——
`Dependence`层看不到`Citizen`这个Core类型），`Job::ExecNode`调用`mod->
ExecNode`之前会先同步`mod->occupantName = occupant ? occupant->GetName() :
""`，供`ExecNode`构造`NPCNavigateChange`这类需要知道"是谁"的Change使用。

### C++调度的例子：WarehouseKeeperJob（`Source/Basic/society/job_warehouse_keeper.h/.cpp`）

`ShopSalerJob`/`FactoryWorkerJob`的上下班已改为脚本驱动（见下"脚本驱动通勤"），`DailyPlan`为空；
仓库管理员`WarehouseKeeperJob`仍走C++的`DailyPlan`/`ExecNode`。下面的写法示意这条通道（9点出门、18点回家）：

```cpp
void ShopSalerJob::DailyPlan(const Time& currentTime, PostHandle*) {
	plans.clear();
	plans["leave_home"] = Time(currentTime.GetYear(), currentTime.GetMonth(), currentTime.GetDay(), 9, 0);
	plans["leave_work"] = Time(currentTime.GetYear(), currentTime.GetMonth(), currentTime.GetDay(), 18, 0);
}
```

`ExecNode`两个节点都是`new NPCNavigateChange(occupantName, <具体房间地址>)`。
`name`/`destination`字段是纯`std::string`——不走`Change`家族"存DSL源码字符串、
求值前现场Parse"的通用约定，因为这两个字段本来就是已经算好的具体值，从不需要
`$$`动态求值（**不要改用`Expression`+`MakeLiteralExpression`构造这两个字段**：
`Expression`树跨DLL构造/求值会导致堆字符串在错误的模块侧释放，引发堆损坏，完整
分析见`Dependence/story/change.md`"字段类型是`std::string`"一节）。
**`destination`必须是具体房间地址（`Room::GetAddress()`格式，`"<building地址>
门牌号>"`），不能写`"home"`/`"workplace"`这种描述性文本**——`JobMod`所在的
Dependence层只能用字符串/int/Time/Change*表达"要什么"，不能反过来依赖Core语义，
`"home"`/`"workplace"`本质上是在Dependence层编码`Citizen::GetRoom()`/
`GetJob()->GetPosition()`这两个Core概念。`ExecNode`通过`post`（`PostHandle*`）
向Core查occupant家/工位的具体房间地址（见下"按需查地址：PostHandle参数"一节），
把查到的地址字符串塞进`NPCNavigateChange`，由`AForeverFrameworkActor::Tick`的
回调用`Map::LocateRoom(address)`直接解析回`Room*`后转发给
`UForeverPopulaceFrameworkComponent::RequestWalk`，见
`ForeverPopulaceFrameworkComponent.md`"市民走路"一节。

### 按需查地址：PostHandle参数

`DailyPlan`/`ExecNode`都有`PostHandle* post`参数（`Dependence/common/
handle.h`，"向Core发起查询的句柄"，另一个用途见`Dependence/story/script_mod.h`
的`ScriptMod::WrapScript`/`Core/common/implement.md`）——`ExecNode`需要知道
occupant的家/工位地址时，构造一个`JsonValue`请求：

```cpp
JsonValue request(DATA_OBJECT);
request["post"] = "citizen home address"; // 或"citizen workplace address"
request["name"] = occupantName;
post->Post(request);
const JsonValue& result = post->GetResult();
if (result["result"].AsString() == "success") {
	std::string address = result["address"].AsString();
}
```

`Core::PostImplement::Post()`（`Core/common/implement.cpp`）实现这两个post
类型：按`name`在`populace->GetCitizens()`里线性查到`Citizen*`，`"citizen home
address"`取`citizen->GetRoom()->GetAddress()`，`"citizen workplace
address"`取`citizen->GetJob()->GetPosition()->GetAddress()`
（`Job::GetPosition()`就是这份工作的工位room），查不到citizen/citizen没有家/
没有job都返回`{"result":"fail","msg":"..."}`。`WarehouseKeeperJob::ExecNode`把这段
查询逻辑提炼成`job_warehouse_keeper.cpp`匿名namespace里的`QueryAddress`辅助函数，查询失败（理论上
不会发生）直接放弃这次调度、不产出任何`Change`。

`post`这个具体实例由调用方（`AForeverFrameworkActor::Tick`）在调用
`populace->Tick(...)`/`society->Tick(...)`前现场构造一个`PostImplement`并
传入，和`UForeverStoryFrameworkComponent::BroadcastGameStart`构造
`PostImplement`供`ScriptMod::WrapScript`查询同一个用法，生命周期只覆盖这一帧的
调用，不长期持有，见`ForeverFrameworkActor.md`"Tick"一节。

### 驱动方式：Populace::Tick

触发点是`Populace::Tick`（不是`Society::Tick`）——`Job`由`Citizen`直接持有引用
（`Citizen::GetJob()`），`Populace`本来就要遍历所有citizen，比按`Organization`
→`Component`→`Job`这条链路反过来找"这个job的occupant是谁"更直接。
`Player::CrossDay()`为true时遍历所有持有job的citizen生成今天的调度、塞进
`Populace`自己的`jobTimerSet`（`std::set<std::tuple<Time, Citizen*,
std::string>>`，按时间排序）；不论是否跨天，每帧从`jobTimerSet`弹出最多
`kMaxJobTimersPerTick`(16)个到期节点执行，避免单帧处理过多到期节点卡顿。**Job的
timer和Organization的timer分开放**：Job的timer在`Populace`
（`jobTimerSet`），Organization的timer在`Society`
（`organizationTimerSet`，见`organization.md`），两套完全独立，各自的每帧处理
上限也互不共享。

`Populace::Tick`的回调签名是`(Citizen*, const vector<Change*>&)`——调用方
（`AForeverFrameworkActor::Tick`）要按这个`Citizen*`决定`NPCNavigateChange`
具体怎么生效（查它当前是否有已生成的`ACitizenElement`），`Populace`自己不知道
Actor层。

## 依赖关系

- 依赖：`Dependence/society/job_mod.h`/`job_factory.h`（`JobMod`/`JobFactory`）、
  `Core/story/script.h`/`story/script_factory.h`（`Script`）、`common/config.h`
  （`Config::GetScriptPath()`按bare文件名反查milestone文件实际路径，若
  `mod->milestoneNames`非空）。
- 被谁依赖：`Organization`（持有所有权，构造时`new Job(...)`）、`Citizen`（不持有
  所有权的引用）、`Populace::Tick`（驱动调度）。

## 脚本驱动通勤
上下班也可以不走C++的`DailyPlan`/`ExecNode`，写在Job自己的脚本里。`Job::SetOccupant`会给Job脚本写三个变量：`$$self.occupant`(上班的人的姓名，空串=空岗)、`$$self.home`(家的房间地址)、`$$self.workplace`(本岗位所在房间的地址)。脚本在`game_start`里`create_timer`(`mode: "next"`)，`time_up`里`npc_navigate`(name=`$$self.occupant`，destination=`$$self.workplace`/`$$self.home`，condition=`$$self.occupant != ""`)再重建次日计时器。`ShopSalerJob`和`FactoryWorkerJob`用这种方式(`DailyPlan`为空)；仓库管理员仍在C++里写。剧情计时器每帧最多处理16个(见story.md)。

## 到岗选项与玩家交互
工厂工人(`job_factory_worker.script`)和商店店员(`job_shop_saler.script`)在`npc_arrive`(到自己的工作房间)时给员工`add_option`("制作汉堡"/"购买汉堡")，下班`remove_option`。点选项是`option_dialog`，也会发给Job脚本(不止Scheduler脚本)：工人→`start_puzzle("puzzle_burger")`(汉堡店小游戏)，结束的`puzzle_result`→`pay_wage "local.score * 200"`(得分在`local.score`，每分2元，工资单位是分)；店员→`open_shop`。

## 开放/关闭

`Job::IsOpen()`/`SetClosed()`：岗位关闭后不算空缺、不招人、不发工资，占用者必须已被清掉。月结裁员和企业倒闭走关闭，扩招和重启走重开（先重开已有的再新建），Job对象只增不删——存档按`Organization::jobs`的顺序写、脚本键`job:<组织下标>:<岗位下标>`依赖这个顺序。存档里每个Job记录末尾追加一个`U8 closed`。
