# job.h / job.cpp

## 职责

`Job`：一份具体工作岗位的实体（比如某个商店里的一个店员岗位）。独占持有一个`JobMod*`
（`Dependence/society/job_mod.h`定义，具体类型如`ShopSalerJob`）+一份自己独占的`Script*`
（`self.`前缀路由目标）。**`Job`由`Organization`持有所有权**（见`organization.md`），
`Citizen`只存一个不持有所有权的`Job*`指针（`Citizen::GetJob()`/`SetJob()`）。

这次迁移**不要Calendar**——老工程Calendar只是"哪几天上班/几点上下班"，这次直接写进具体
`JobMod`子类的`DailyPlan`里，`Job`不持有`Calendar*`。

## DailyPlan/ExecNode：纯C++通道，和milestone/JSON完全独立

`Job::DailyPlan(currentTime, post)`每天（`Populace::Tick`在`Player::CrossDay()`为true时
调用，见下"驱动方式"）转调`mod->DailyPlan(currentTime, post)`，`DailyPlan`直接写`mod`
自己的`plans`成员字段（node名->今天触发的具体时间）。

`Job::ExecNode(node, post)`到期时（同样由`Populace::Tick`驱动）转调
`mod->ExecNode(node, post)`，`ExecNode`直接`new Change*`写进`mod`自己的`changes`
成员字段。`post`（`PostHandle*`，见下"按需查地址：PostHandle参数"一节）两个方法都会
透传，`DailyPlan`这次的`ShopSalerJob`实现用不上（生成调度表不需要知道地址），只有
`ExecNode`真正用它查地址。**这套调度机制完全不查、
不碰、不关心这个Job的Script里是否有同名milestone**——milestone只能通过它自己原本的
`Script::MatchEvent`事件匹配流程被触发（比如将来有代码广播一个Event给这个Job的
Script），两者运行时完全独立，不做任何"按节点名找milestone"的合并逻辑（这是设计过程中
明确否决掉的一个方案，milestone和C++调度这次刻意保持互不相干）。`scriptModName`/
`milestoneNames`这两个字段（见下"Script配置"一节）因此纯粹是给`Job`的`Script`提供
milestone内容用的，和`DailyPlan`/`ExecNode`无关；`ShopSalerJob`这次给
`milestoneNames`塞了`{"job_shop_saler"}`，用来验证Job的game_start广播链路，见下。

## Script配置：`scriptModName`/`milestoneNames`——不按值返回容器，Core不替mod做选择

**这里曾经有一个被明确指出的违规实现**：`JobMod`一度有一个
`virtual std::vector<std::string> GetMilestoneFiles() const`方法，按值把mod分配的
`vector<string>`返回给Core遍历——这正是这个项目最核心的"Mod绝不能把STL容器按值返回给
Core"规则要禁止的跨DLL模式。同时`Job::Job()`当时还硬编码了`kEmptyScriptId="empty"`
（决定"用哪个ScriptMod"）和`configDir/"../Story"/(name+".json")`路径拼接（决定
"文件放在哪"），这两个决策本来都应该由Mod自己声明，不该被Core替它做主。

修复后的设计——照抄老工程"用`pair<ScriptModName, vector<MilestoneName>>`在Mod侧声明
意图，Core只负责按ScriptModName创建Script、按MilestoneName列表挨个加载"的方式，
`JobMod`新增两个普通成员字段（和`plans`/`changes`同一个"mod自己的字段，Core只读"安全
模式）：

```cpp
std::string scriptModName = "empty"; // 默认"empty"，纯粹要个能创建出来的ScriptMod壳
std::vector<std::string> milestoneNames; // 默认为空，每项是不含路径/扩展名的bare文件名
```

`ShopSalerJob`构造函数里设`scriptModName = "empty"; milestoneNames =
{"job_shop_saler"};`。`Job::Job()`不再硬编码，改成：

```cpp
script = new Script(scriptFactory, mod ? mod->scriptModName : "empty"); // mod为空是
	// CreateJob本身失败的防御性兜底，不是"决定用哪个ScriptMod"这个设计选择
if (mod) {
	for (const string& name : mod->milestoneNames) {
		script->ReadMilestones(Config::GetScriptPath(name)); // 按bare文件名反查实际
			// 路径——Config::resource_path/.script扫描机制，见Config.md
	}
	script->SetValue("name", ValueType(string(mod->GetName()))); // 供milestone脚本里
		// $$self.name引用这个Job的唯一名字（是JobMod自己的GetName()，不是Script的
		// mod->GetName()），见job_basic.cpp的GetName()唯一性修复
}
```

`Config::GetScriptPath(name)`（`Source/Core/common/config.h/.cpp`）按不含路径/扩展名的
bare文件名反查`.script`文件的实际绝对路径——文件实际存放在哪由`config.json`的
`resource_path`数组决定（`["../Story"]`），`JobMod`不知道也不需要知道用户把脚本文件
放在哪，只管声明"我要哪个bare名字"。

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
void ShopSalerJob::DailyPlan(const Time& currentTime, PostHandle*) {
	plans.clear();
	plans["leave_home"] = Time(currentTime.GetYear(), currentTime.GetMonth(), currentTime.GetDay(), 9, 0);
	plans["leave_work"] = Time(currentTime.GetYear(), currentTime.GetMonth(), currentTime.GetDay(), 12, 0);
}
```

`ExecNode`两个节点都是`new NPCNavigateChange(occupantName, <具体房间地址>)`
（`NPCNavigateChange`是老工程已经实现过、这次连同`change.h`一起搬过来的Change类型，
`name`/`destination`字段是纯`std::string`——不走`Change`家族"存DSL源码字符串、求值前
现场Parse"的通用约定，因为这两个字段本来就是已经算好的具体值，从不需要`$$`动态求值。
这里最初误写成`Expression`类型、靠`MakeLiteralExpression`+`Expression::Parse`构造，
结果引出一次真实的跨模块堆损坏崩溃——`Basic.dll`构造的`Expression`树被`Forever.dll`
求值，虚函数调用实际执行的是`Basic.dll`编译的机器码，返回的堆字符串却被UE的`FMemory`
释放，完整分析见`Dependence/story/change.md`"字段类型是`std::string`"一节；修复后
直接改成纯`std::string`，彻底跳出`Expression`体系，`MakeLiteralExpression`这个辅助
函数也一并删除了）。**`destination`不能写`"home"`/
`"workplace"`这种描述性文本**——`ExecNode`通过`post`（`PostHandle*`）向Core查
occupant家/工位的具体房间地址（`Room::GetAddress()`格式，`"<building地址> <门牌号>"`，
见下"按需查地址：PostHandle参数"一节），把查到的地址字符串塞进`NPCNavigateChange`，由
`AForeverFrameworkActor::Tick`的回调用`Map::LocateRoom(address)`直接解析回`Room*`
后转发给`UForeverPopulaceFrameworkComponent::RequestWalk`，见
`ForeverPopulaceFrameworkComponent.md`"市民走路"一节。**这次改这个的原因**：早期实现
图省事直接用`"home"`/`"workplace"`两个描述性字面值，靠`AForeverFrameworkActor::Tick`
的回调按字符串相等判断走哪个分支——这违反了`JobMod`所在Dependence层"只用字符串/int/
Time/Change\*表达要什么，不能反过来依赖Core语义"的分层原则（`"home"`/`"workplace"`
本质上是在Dependence层编码"Citizen::GetRoom()/GetJob()->GetPosition()"这两个Core概念，
只是用字符串包了一层皮），用户明确要求改成"具体房间地址"，即`Job`所在层不需要知道
"家"/"工位"这两个概念是什么，只需要知道地址是什么、按地址走过去。

## 按需查地址：PostHandle参数

`DailyPlan`/`ExecNode`这次都新增了`PostHandle* post`参数（`Dependence/common/handle.h`，
"向Core发起查询的句柄"，已有的第一个用途见`Dependence/story/script_mod.h`的
`ScriptMod::WrapScript`/`Core/common/implement.md`）——`ExecNode`需要知道occupant的
家/工位地址时，构造一个`JsonValue`请求：

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

`Core::PostImplement::Post()`（`Core/common/implement.cpp`）实现这两个post类型：按
`name`在`populace->GetCitizens()`里线性查到`Citizen*`，`"citizen home address"`取
`citizen->GetRoom()->GetAddress()`，`"citizen workplace address"`取
`citizen->GetJob()->GetPosition()->GetAddress()`（`Job::GetPosition()`就是这份工作的
工位room），查不到citizen/citizen没有家/没有job都返回`{"result":"fail","msg":"..."}`。
`ShopSalerJob::ExecNode`把这段查询逻辑提炼成本文件匿名namespace里的`QueryAddress`
辅助函数，查询失败（理论上不会发生）直接放弃这次调度、不产出任何`Change`。

`post`这个具体实例由调用方（`AForeverFrameworkActor::Tick`）在调用
`populace->Tick(...)`/`society->Tick(...)`前现场构造一个`PostImplement`并传入，和
`UForeverStoryFrameworkComponent::BroadcastGameStart`构造`PostImplement`供
`ScriptMod::WrapScript`查询同一个用法，生命周期只覆盖这一帧的调用，不长期持有，见
`ForeverFrameworkActor.md`"Tick"一节。

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
  （`Config::GetScriptPath()`按bare文件名反查milestone文件实际路径，若
  `mod->milestoneNames`非空）。
- 被谁依赖：`Organization`（持有所有权，构造时`new Job(...)`）、`Citizen`（不持有所有权
  的引用）、`Populace::Tick`（驱动调度）。
