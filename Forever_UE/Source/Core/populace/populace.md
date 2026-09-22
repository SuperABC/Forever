# populace.h / populace.cpp

## 进入populace域（本轮迁移概览）

这一轮迁移一共做了4件强相关的事，这份文档记录后两件（`Citizen`/`Populace`本身，见
`citizen.md`记`Citizen`）；前两件分别记在各自的文档里：

1. `Source/Basic/map/`下四个占位类改名：`ZoneBasic`→`ResidenceZone`、`BuildingBasic`→
   `ResidenceBuilding`、`RoomBasic`→`ResidenceRoom`、`ComponentBasic`→
   `ResidenceComponent`（纯改名，`ResidenceRoom`同时第一次填了`isResidential`/
   `residentialCapacity`真实数据）。见`Source/Basic/README.md`。
2. `Room`新增4类占位能力字段（residential/workspace/storage/manufacture）。见
   `Source/Core/map/room.md`。
3. `Citizen`类（本轮新建，见`citizen.md`）。
4. `Populace`类（本轮新建，本文档）+ `Map::ComputeAccommodationTarget()`/`Map::Checkin()`
   （见`Source/Core/map/map.md`"人口初始化对接"一节）+ Forever层的流式场景实例化（见
   `Source/Forever/Element/CitizenElement.md`/`Source/Forever/Framework/
   ForeverPopulaceFrameworkComponent.md`）。

## 职责

`Populace`：**和`Map`平级的顶层Core类，不是挂在`Map`底下的工具函数集合**——这是这次设计
唯一被用户明确纠正过的地方：`Populace`不知道`Map`的存在，自己管理`Citizen*`的生命周期，
`Map`只知道"有一个`Populace`可以读"，通过`Map::Checkin(const Populace&)`把外部传入的
citizens分配进自己的住宅room。两者都由`AForeverFrameworkActor`平级持有（`Map* map`旁边
`Populace* populace`），和老工程`GlobalBase`同时持有`map`/`populace`两个顶层对象、由它做
两者之间的编排是同一个分工：
```cpp
int accomodation = map->InitContents();      // 老工程
populace->Init(accomodation, player, implement);
map->Checkin(populace, player);
```
```cpp
populace = new Populace();                    // 这次
populace->Init(map->ComputeAccommodationTarget());
map->Checkin(*populace);
```

## 关键设计

### 年表模拟算法——完整照抄老工程

`Populace::Init(int accommodation)`先对`accommodation`做一次老工程同款的扰动
（`target = accommodation * exp(GetRandom(1000)/1000.0f - 0.5f)`，`expf`），再调私有的
`GenerateCitizens(target)`——完整照抄老工程`Populace::GenerateCitizens`
（`E:\Projects\Forever_UE\Source\Core\populace\populace.cpp:659-859`）的算法，逐行对照
过，包括几个容易出偏差的细节：

- **100男100女起始种子**：出生年`GetRandom(20)`（0~19），死亡年`birth+60+GetRandom(40)`
  （everyone都有），女性额外排一次婚姻年`birth+20+GetRandom(15)`。
- **`maleBirths`按出生年bucket-fill**：男性种子先按`birth`排序（`sort(males.begin()+1,
  males.end(), ...)`），再从`currentBirth+1`扫到`males[i].birth`逐年填
  `maleBirths[year]=i`——这样`maleBirths[year]`就是"出生年≤year的最后一个男性数组下标"，
  婚姻匹配靠这个数组按年龄窗口二分区间取随机候选。**这个排序只在初始100人这一步做一次**，
  后续`LIFE_BIRTH`新增的男性永远`push_back`在末尾，不会再触发重排——数组下标一旦分配就
  终身稳定，`chronology`里排队的死亡/生育事件引用的下标不会因为后续操作失效。
- **年表事件编码**：`chronology[year]`里每条`(eventIdx, LIFE_TYPE)`，`eventIdx>=0`表示
  女性数组下标，`eventIdx<0`表示`-男性数组下标`（真实下标从1开始，0留给哨兵，符号编码不
  会冲突）。
- **停止条件**：`year<100`无条件跑满，之后`year<4096`且"存活+死亡"总人数（`females.size()
  +males.size()`，不是只数活人）达到`target`前继续——**是"累计出生过的人数"而不是"当前
  活人数"**，这是容易读错的一点。
- **婚姻匹配**：`lowId=max(0,birth-10)`/`highId=max(0,birth+5)`是**出生年**，不是数组
  下标——`maleBirths[lowId]`/`maleBirths[highId]`才是数组下标区间，在这个区间里最多试10次
  随机候选，都是已婚就放弃（这次婚姻事件失败，这个女性永远不会再收到新的婚姻事件——和
  老工程一样，不重试）。
- **只把姓名/性别/生日+配偶/子女链接物化进`Citizen`**：父母/兄弟姐妹等其它血缘关系只在
  模拟用的内部临时结构（`populace.cpp`匿名namespace里的`Human`/`LIFE_TYPE`，不对外暴露）
  里用来推进人口数量，模拟结束后不保留（`Citizen`字段范围只到配偶/子女，见`citizen.md`）。
  配偶/子女之所以要保留，是因为`Map::Checkin()`需要靠它们判断"一家人要不要搬进同一间"——
  用户明确要求过这一点，见"不在这次范围内"一节曾经的错误简化历史。只有模拟结束时活着
  （`life != LIFE_DEAD`）且额外通过老工程同款95%存活过滤（`GetRandom(20) > 0`）的个体
  才会真正`new Citizen(...)`，物化时用`Human::idx`记下"这个人在`citizens`里的下标"
  （照抄老工程`Person::idx`的用法），再单独一趟"登记配偶/子女"的遍历把幸存双方的`idx`
  翻译成`Citizen*`互相关联——**只有配偶/子女双方都幸存物化才登记**，只有一方活下来的
  婚姻/亲子关系直接丢弃（本来也没有对应的`Citizen*`可指）。生日年份沿用老工程"2000+模拟
  内部年份"的换算习惯；模拟结束时的年份（`year+2000`）存进`currentYear`，通过
  `GetCurrentYear()`供`Map::Checkin()`给`Citizen::GetAge()`用，也供
  `AForeverFrameworkActor::EnsurePlayerGenerated()`把开局游戏时钟设成这一年的1月1日8点
  （`player->SetTime(Time(populace->GetCurrentYear(), 1, 1, 8))`），见
  `Core/player/player.md`"开局时间=人口模拟结束年份"一节。

### 姓名生成：真正的Name域（`NameMod`/`NameFactory`/`ChineseName`，第二版迁移）

第一版用了一个10姓+10男名+10女名的内置小词表占位，用户明确要求"不要这种方法，从老工程里
把name Concept迁移过来"——这次把`Source/Dependence/populace/name_mod.h`（`NameMod`接口）
和`Source/Basic/populace/name_chinese.h/.cpp`（具体实现，改名自阶段3占位骨架
`NameBasic`）从阶段3骨架换成老工程真正的取名算法/数据表。

`NameMod`接口照抄老工程`NameMod`（`E:\Projects\Forever_UE\Source\Dependence\populace\
name_mod.h`）的三个纯虚方法：`GetSurname(fullName)`/`GenerateName(male,female,neutral)`/
`GenerateName(surname,male,female,neutral)`。**这三个方法这次改回了老工程"传一个set
结果的lambda进去"的callback写法**——一度改成直接按值返回`std::string`更简单，但这正是
被明确指出禁止的跨DLL模式（mod侧构造的`std::string`临时对象按值返回穿过DLL边界）；
`std::function<void(const std::string&)>`按`const&`传入mod侧重写的虚方法，回调只读
`const string&`参数拷贝进调用方自己的`string`，不发生"一侧分配、另一侧释放"的情况，
和`RoadnetMod::DistributeRoadnet`/`TerrainMod::DistributeTerrain`已经在用的
`std::function`回调传参是同一类安全模式（这次不需要`PostHandle*`——那是给"向Core发起
查询"场景用的，`GenerateName`/`GetSurname`不需要查Core状态，纯粹是"把结果传出来"，两者
是不同的问题）。生成失败（候选词库为空等）时不调用`setResult`，`Name`（Core层包装类，
`Name::GenerateName`/`GetSurname`仍然按值返回`std::string`——`Name`不是mod、不跨DLL
边界，不受这条规则约束）读到的结果保持初始的空字符串，和老工程失败信号语义一致。

`ChineseName`（`Source/Basic/populace/name_chinese.cpp`）逐字段/逐行照抄老工程
`ChineseName`（`E:\Projects\Forever_UE\Source\Basic\populace\name_basic.cpp`）：
- 姓氏表（200个单字，按常见程度从高到低排列）+男/女/中性给定名表（分别90/130/370个
  单字）全部硬编码`std::vector<std::string>`字面量，不走config/数据文件。
- 姓氏采样用"sqrt反索引"实现非均匀分布：`N=surnames.size()`，从`[0,N²)`均匀取
  `randVal`，`idx=floor(sqrt(randVal))`本身偏向大值，`surnameIdx=N-1-idx`把这个偏向
  反过来，让数组靠前（常见）的姓氏被抽中概率远高于靠后（罕见）的，不需要显式权重表。
- 给定名长度：`GetRandom(9)==0`时1个字（概率1/9），否则2个字（概率8/9），比单字名
  常见得多。每个字独立按`allowMale`/`allowFemale`/`allowNeutral`哪些词库被打开，从
  合并后的候选池里均匀抽取（一个2字名可以混合不同词库的字）；三个词库都关闭时兜底成
  只用中性词库。
- `GetSurname(fullName)`：手动解析UTF-8前导字节，截取姓名的第一个UTF-8字符作为姓——
  这个方案假定姓氏永远是单字（这份姓氏表里成立），不做姓氏表查找匹配。

`Populace`新增`InitNames()`（`Init()`一开始调用一次）：**之前`Populace`自己独立持有一份
`ModLoader modLoader;`+`NameFactory nameFactory;`**（不和`Map`共用，两者互不知道对方
存在），每次`new Populace()`(每次开局)都会重新扫描/注册一遍name mod dll，和`Map`当初的
问题一样，被要求统一挪走：现在`nameFactory`是构造函数初始化列表里绑定的引用成员
（`Registry::Get().GetNameFactory()`），mod dll的发现/注册全部集中到`Registry`
（`Source/Core/common/registry.md`）里，只在整个UE进程生命周期里跑一次：
```cpp
Populace::Populace() : nameFactory(Registry::Get().GetNameFactory()) {}
// ...
void Populace::InitNames() {
	for (const auto& [id, args] : Config::GetConceptMods("name_mods")) {
		nameFactory.SetConfig(id, true);
	}
	string activeId = nameFactory.GetName();
	if (activeId.empty()) {
		auto ids = nameFactory.GetRegisteredIds();
		if (!ids.empty()) {
			nameFactory.SetConfig(ids[0], true);
			activeId = ids[0];
		}
	}
	if (activeId.empty()) {
		THROW_EXCEPTION(RuntimeException, "No name mod available.\n");
	}
	name = new Name(&nameFactory, activeId);
	// ...ReserveName，见下"主线剧情.script的name_reserve"一节
}
```
**`NameFactory`这次改成和`RoadnetFactory`同一个"单选"模式，不在C++里硬编码具体mod
名字**——Name这个concept只需要唯一一个"当前生效"的取名算法给`Populace`用（不是
Terrain/Zone/Building那种按`GetPriority()`/权重多mod叠加），最初的实现直接硬编码
`ChineseName::GetId()`（`"chinese"`）字面量，被要求改掉：`NameFactory`新增
`SetConfig(id, enabled)`/`GetName()`（分别对应`RoadnetFactory::SetConfig`/
`GetRoadnet()`），`CreateName`/`CheckRegistered`/`GetRegisteredIds`改回不看
`configuredArgs`的纯`registries`查询（和`RoadnetFactory`一致——"单选"这件事完全在
调用方这一层做，Factory自己不做启用过滤）。`InitNames()`现在和`Map::InitRoadnet()`
逐字同构：遍历`Config::GetConceptMods("name_mods")`挨个`SetConfig(id, true)`，
`GetName()`拿到胜出的id；`config.json`没配`name_mods`时退化选第一个注册到的id
（`GetRegisteredIds()[0]`），和`Map::InitRoadnet()`同一个容错风格；两条路径都拿不到
id才是真正的致命配置错误（没有取名算法整个游戏就没法生成任何市民），`THROW_EXCEPTION`
交给`AForeverFrameworkActor::BeginPlay()`的`try/catch`统一处理（打日志+退出游戏）。
`config.json`现在的`"name_mods"`数组只需要写`["chinese"]`，不会再有`"empty --test
true"`这种占位条目跟着凑数——`EmptyName`依然正常注册在`registries`里（`GetModNames`
返回的id列表不受影响），只是永远不会被`SetConfig`标记启用，`GetName()`不会选中它。
`Populace`的析构函数`delete name`（`Name`析构里调用`nameFactory.DestroyName(mod)`
释放真正的`NameMod*`）。

**架构修正：新增`Core/populace/name.h/.cpp`的`Name`类，`Populace`不再直接持有/调用
`NameMod*`**——最初这一版迁移图省事，让`Populace`直接持有`NameMod* nameMod`并调
`nameMod->GenerateName(...)`，被指出这和`Terrain`（`Core/map/terrain.h`）、`Script`
（`Core/story/script.h`）等其它concept"上层聚合类只操作Core层包装类，不直接碰
`<Concept>Mod*`"的架构不一致后改正：新增`Name`类（`factory`+`mod`+缓存的`type`/`name`，
构造/析构/转发方法的写法逐字照抄`Terrain`），`Populace`现在只持有`Name* name`，通过它转发
`GetSurname`/`GenerateName`两个重载，见`name.md`。

**一处顺手修正的疑似bug**：老工程`Populace::GenerateCitizens`给最初100男100女种子生成
姓名时，两个性别都传`(male=false, female=true, neutral=true)`——男性种子也只从女性+
中性词库取名，读起来像是老工程自己的疏漏（和几十行之后`LIFE_BIRTH`事件"按孩子实际性别
选对应词库、中性词库永远允许"的写法不一致）。这次迁移时改成和`LIFE_BIRTH`一致的
"按性别选对应词库+中性词库永远允许"，不逐字复刻这个疑似bug，`populace.cpp`里有对应
注释说明。

**`reserve`（避免和脚本占位姓名撞名）后来在Scheduler/主线剧情`.script`新增字段这轮
迁移时补上了，`roll`（同一轮生成内部去重）仍然没有迁移**：老工程`Name`（不是`NameMod`）
在具体算法之上加了`reserve`+`roll`两层`unordered_set`去重，这次最初以"Story/Script域
还没进这个项目，`reserve`完全用不上"为由都没迁移。后来Story/Script域进来了，用户要求
比照老工程给主线剧情`.script`加一个`name_reserve`顶层字段（见`Core/story/script.md`
"主线剧情.script新增三个顶层字段"一节），`Name::ReserveName`/`reserve`集合因此照抄
老工程补上——`GenerateName`两个重载内部改成"生成一个候选，撞上`reserve`就重试，最多
`kMaxReserveRetryAttempts`（1000）次，仍然撞上直接`THROW_EXCEPTION(DeadLoopException,
...)`"（不能静默返回一个撞名的结果，交给`AForeverFrameworkActor::BeginPlay()`的
`try/catch`统一处理），见`name.md`"`ReserveName`"一节。`roll`（同一轮生成内部互不
撞名）依然没有迁移——姓名生成算法从10x10的占位小词表换成了~200姓氏x最多590个给定名
字符的真实词库，组合数量级足够大，几百个citizen之间实际撞名的概率本来就很低，如果以后
确实需要严格去重可以再补。

## 房产归属（`Map::Checkin`，进入populace域第四轮迁移）

用户要求"migrate老工程的园区/建筑/房间随机分配归属逻辑，并保留公有资产的逻辑"——这不是
`Populace`自己的职责（`Populace`只管人口模拟，不知道`Map`/`Zone`/`Building`/`Room`的
存在），实际代码在`Map::Checkin()`里，完整算法见`Source/Core/map/map.md`"人口初始化
对接"一节、字段设计见`Source/Core/map/room.md`/`zone.md`。这里只记录几个需要显式决策
（不是照抄就能定下来）的点：

- **"归属"和"住处"是两个独立概念，之前混在一起过一次**：第一版实现把"抽到住处的那个
  citizen"直接设成`room->owner`，被用户指出不对——一个room的owner应该由独立的"房产
  归属"流程决定，可能是从没在这里住过的人（房东），和"谁实际住在这里"（`tenants`/
  `occupants`）没有必然关系。修正后`Map::Checkin()`先跑完整的zone→building→room归属
  级联（对应老工程的部分，见下），再跑完全独立的住处分配循环，两者互不干扰。
- **"公有"（`stated`）这个结果这次是真正随机可达的，老工程原版不是**：核对过老工程
  `Map::Checkin`全部代码，"stated"分支的触发条件永远是`zone->GetStated()`/
  `building->GetStated()`这种"查询一个预先已经被设置好的标记"，但搜了整个老工程也没有
  任何mod/代码路径会在`Checkin`跑之前主动`SetStated(true)`——也就是说"公有"这条分支在
  老工程实际发布的内容里从来不会被触发，是个只有骨架、没有内容能激活它的死分支。用户
  明确要求"保留公有资产的逻辑"，为了让"公有"真的能在这个项目里出现，这次自己加了两个
  小概率（`kZoneStatedChance=2`、`kBuildingStatedChance=3`，和老工程`ZONE_OWNERSHIP_
  CHANCE=2`/`BUILDING_OWNERSHIP_CHANCE=5`同一个量级，满分`kProbabilityScale=100`）——
  **这两个具体数值是这次新定的，不是老工程的原始数据**，只是结构上照抄"整体统一归属"的
  三选一模式（公有/私有/下探到下一级），按需可以调整。
- **不照抄老工程对zone内部building的重复roll**：老工程`Map::Checkin`处理完
  `zones`（含级联到zone内部building/room的归属）之后，会再无条件遍历一次
  `Map::buildings`（这个map本来就包含所有building，含zone内部的）给每个building重新
  roll一次归属——如果一个zone刚被判定"整体私有/公有"，它内部的building几行代码之后就会
  被这第二轮独立roll出来的结果覆盖掉，级联下来的归属形同虚设。核对多遍确认这是老工程的
  疏漏（不是有意为之的"精细化覆盖"设计——"stated"分支专门有`if (building->GetStated())
  continue`保护、不会被覆盖，但"私有"分支没有对应保护，一个显然是漏写了），这次移植时
  显式跳过`GetParentZone()`非空的building，不逐字复刻这个疏漏。

## 依赖关系

- 依赖：`Source/Core/populace/citizen.h`（`GENDER_TYPE`/`Citizen`）、
  `Source/Core/populace/name.h`（`Name`——`Populace`只通过这层转发访问取名算法，不直接持有
  `NameMod*`，见`name.md`）、`Source/Dependence/populace/name_factory.h`（`NameFactory`，
  引用成员`nameFactory`的类型，传给`Name`的构造函数）、`Source/Core/populace/scheduler.h`
  （`Scheduler`，`AssignSchedulers()`里`new`）、`Source/Dependence/populace/
  scheduler_factory.h`（`SchedulerFactory`，引用成员`schedulerFactory`的类型）、
  `Source/Core/story/script_factory.h`（`ScriptFactory`，引用成员`scriptFactory`的
  类型，传给每个`Scheduler`独占的`Script`——`Populace`这次第一次依赖Story域，和
  `Society`持有`ScriptFactory`引用成员是同一个先例）、`Source/Core/common/registry.h`
  （`Populace`构造函数绑定`nameFactory`/`schedulerFactory`/`scriptFactory`，见
  `registry.md`）、`Source/Dependence/common/utility.h`（`GetRandom`/`GetRandomNormal`/
  `Time::DaysInMonth`）、`Source/Core/populace/experience.h`（`KinshipExperience`/
  `EmotionExperience`/`EducationExperience`，见`experience.md`）、
  `Source/Core/populace/school.h`（`SchoolClass`，见`school.md`）。
- 被谁依赖：`Source/Core/map/map.h/.cpp`（`Map::Checkin(const Populace&)`读
  `GetCitizens()`）、`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（持有
  `Populace*`，`EnsurePopulaceGenerated()`里`new`+`Init`+`Checkin`）、`Source/Forever/
  Framework/ForeverPopulaceFrameworkComponent.h/.cpp`（`GenerateCitizens(Map*,
  Populace*)`缓存`GetCitizens()`列表）。

## Tick：驱动Job的调度（进入society域新增）

`Populace::Tick(currentTime, crossedDay, onActions, post)`——`Job`的timer放在这里
（不是老工程的`Society::timerSet`），因为`Job`由`Citizen`直接持有引用，`Populace`本来
就要遍历所有citizen，比按`Organization`→`Component`→`Job`反查"这个job的occupant是谁"
更直接。`post`（`PostHandle*`）原样透传给`job->DailyPlan`/`job->ExecNode`，`Populace`
自己不解读这个句柄，见`society/job.md`"按需查地址：PostHandle参数"一节。`crossedDay`
为true时遍历所有持有job的citizen，转调`job->DailyPlan(currentTime, post)`
生成今天的调度表，塞进`Populace`自己的`jobTimerSet`（`std::set<std::tuple<Time,
Citizen*, std::string>>`，按时间排序）；不论是否跨天，每帧从`jobTimerSet`弹出最多
`kMaxJobTimersPerTick`个到期节点执行——照抄老工程"一帧内允许处理的timer上限"这个
设计，避免单帧处理过多到期节点卡顿。**这次调成`1`**（原来是`4`）——每个Job类型这次
都用固定钟点（比如店员天天9点/12点），同一时刻到期的节点数很容易远超每帧上限，
PIE验证发现`kTimeFlowRatio`调快之后9点/12点这类"大量市民同一时刻上下班"场景会卡顿
好几帧，调到`1`让每帧的开销（尤其是`Map::FindPedestrianPath`这次Dijkstra寻路）更
均摊，代价是把同一批到期节点摊得更久才能处理完。Organization自己的调度是另一套完全
独立的timer，放在`Society::Tick`里，见`Source/Core/society/society.md`"Job的timer在
Populace，Organization的timer在Society"一节。

回调签名`(Citizen*, const vector<Change*>&)`——`Populace`自己不知道Actor层，调用方
（`AForeverFrameworkActor::Tick`）拿到这个`Citizen*`构造`ScriptContext`后，对每个
`Change*`统一转发给`AForeverFrameworkActor::ApplyChange`消费（不再像最初那样在回调里
自己手写`dynamic_cast` dispatch），见`Source/Forever/Framework/ForeverFrameworkActor.md`
"统一的Change消费入口：`ApplyChange`"一节。

**Scheduler concept迁移新增第三套独立timer**：`schedulerTimerSet`+
`kMaxSchedulerTimersPerTick`，和`jobTimerSet`那一段结构完全平行（同一个`crossedDay`
判断、同一个每帧上限式的弹出循环、同一个`onActions`回调），只是换成
`citizen->GetScheduler()`——`Scheduler`负责citizen下班之后的行为，见`scheduler.md`
"职责边界"一节。两套timer共用同一个回调是因为回调签名本来就是通用的`(Citizen*,
const vector<Change*>&)`，不关心Change是`Job`产的还是`Scheduler`产的。

## `AssignSchedulers`：加权随机给每个citizen分配一个Scheduler（Scheduler concept迁移新增）

`Populace::Init()`结尾（`GenerateCitizens(target)`跑完、`citizens`列表已经就绪之后）
调用一次。参考老工程`Populace::GenerateCitizens`结尾的算法（`E:\Projects\Forever_UE\
Source\Core\populace\populace.cpp:963-997`）：对所有已注册的Scheduler类型
（`schedulerFactory.GetRegisteredIds()`）累加权重（`schedulerFactory.GetPower(id)`）
建CDF，对每个citizen roll一个随机数（`GetRandom(10000)/10000.f * total`）选中一个
类型、`new Scheduler(&schedulerFactory, &scriptFactory, selected, citizen)`。**不照抄**
老工程`SchedulerFactory::RegisterScheduler`把`power`塞进注册表、`GetPowers()`一次性
返回全部`unordered_map<string,float>`这套形状——这个工程已经确立的是
`SchedulerFactory::GetPower(id)`单个查询（照抄`OrganizationFactory`，`Society::Init`
选Organization类型已经在用同一套CDF算法，见`society.md`），`SchedulerFactory`这次
补齐了`PowerFunc`/`GetPower`，详见`scheduler.md`"加权随机分配"一节。所有已注册类型
权重都是`0.f`（`total <= 0.f`）时退化成均匀随机，保证人人都有一个Scheduler，不整体
失败。

## 四类人际关系生成（个人属性 + 亲属/同学/情感三类，同事关系见society.md）

`Populace::Init()`结尾依次调用`GenerateRomanticRelations()`+`GenerateEducations()`
（`GenerateCitizens(target)`→`AssignSchedulers()`之后）。**亲属关系
（`GenerateKinshipRelations`，含配偶/子女/兄弟姐妹）不是独立的第三个`Init()`步骤**——
它折在`GenerateCitizens`函数体末尾直接调用，因为它需要读模拟阶段`Human`结构里的结婚
年份（`Human::marry`）/父母索引（`Human::father`/`mother`），这些数据物化成`Citizen`
后就丢弃了（见"年表模拟算法"一节），只能在`females`/`males`两个数组还在作用域内、也就
是`GenerateCitizens`函数体结束之前处理。

个人属性（`Personality`）不需要额外的生成pass——`Personality`是`Citizen`的成员，构造
`Citizen`时自动跑默认构造，`Personality::Personality()`已经在13个字段各自
`clamp(GetRandomNormal(0, 1/3), -1, 1)`，见`citizen.md`。

### 亲属关系（`GenerateKinshipRelations`，`populace.cpp`匿名namespace自由函数）

配偶/子女的信息`Populace::GenerateCitizens`早就有了（`Citizen::GetSpouse()`/
`GetChildren()`），亲属关系生成只是把这些已有链接记进`acquaintances`（随机赋
`Relation`数值）+`KinshipExperience`：
- **配偶**：`RELATIVE_SPOUSE`，`beginYear`是结婚年份——直接读`Human::marry`转成真实
  年份（`2000 + marry`）带出来，**不是重新计算**（一个人一生只有一个结婚年份，这次不
  模拟离婚/再婚）。
- **子女**：父母一侧`RELATIVE_CHILD`，子女一侧`RELATIVE_PARENT`，`beginYear`是子女的
  出生年份。
- **兄弟姐妹（这次新增，老工程没有对应的`RELATIVE_TYPE`）**：按`(father index, mother
  index)`对`females`/`males`两个模拟数组临时分组，组内两两互相标记
  `RELATIVE_SIBLING`，`beginYear`取两人中较晚的出生年份。这份"父母是谁"的分组数据
  只在`GenerateKinshipRelations`这一次调用里用一次，不持久化成`Citizen`的字段——和
  "不保留父母/兄弟姐妹等其它血缘关系"这条既有决策并不矛盾：**保留的是反推出来的
  兄弟姐妹`acquaintances`/`Experience`结果，不是"父母是谁"这条原始数据本身**。

### 情感关系（`GenerateRomanticRelations`）

参考老工程`GenerateEmotions`的年龄门槛/避雷思路，但方向反过来——老工程正向模拟一生，
这次从"当前状态"反推历史：

- **已婚**：结婚年份直接读`GenerateKinshipRelations()`已经写好的
  `KinshipExperience(RELATIVE_SPOUSE)::GetBeginYear()`，不重新计算。恋爱开始年份在
  `[max(双方满14岁年份), 结婚年份]`之间随机取，写一条`EmotionExperience`（`beginYear`
  是恋爱开始年份，`endYear=-1`表示至今仍在一起，这次不模拟离婚——恋爱到婚姻是同一段
  感情的连续记录，不会在结婚那年断开重写一条）。
- **更早的恋爱史（可以有多段）**：**按年份逐年推进的编年模拟**，不是"按总跨度套公式
  一次性算出总段数"——最初的实现用老工程`GenerateEmotions`的公式
  `maxRelationships = min(10, 跨度/3 + 1)`一次性决定总段数，审阅时被指出这样"单身时间
  越长（往往就是年纪越大的人）反而分配到越多段恋爱史"完全反了，改成`GeneratePastRelationships`
  逐年遍历`[满14岁年份, boundEnd)`，每一年是否开始一段新恋情按
  `DatingHazardForAge(当时年龄)`（见下）独立判定，年纪越大命中概率越低。时长仍然用
  `pow(r,4)`让持续时间偏短（照抄老工程取法）。前任保留在`acquaintances`里，不因为
  关系结束就删除。
- **`DatingHazardForAge(age)`——按年龄递减的年度恋爱概率**：14-19岁10%、20-29岁14%、
  30-39岁7%、40-49岁3%、50-59岁1.2%、60岁以上0.5%（具体数值实现时可调，"随年龄递减"
  是硬要求）——不管是"更早的恋爱史"还是下面的"当前情人/婚外情"，基础概率都来自这一个
  函数，不是各自维护互相独立的常数。
- **当前情人（可以同时有多个，`GetCurrentLovers()`返回`vector`）**：基础概率同样是
  `DatingHazardForAge(当前年龄)`，已婚在此基础上再乘一个大幅降低的系数
  （`kMarriedMultiplier`，0.15）——已婚只尝试一次，不连续叠加多个婚外情人；未婚最多
  连续尝试3次、每次不命中就停止（审阅时反馈"婚外情概率太高"，最初版本已婚固定5%、
  未婚固定35%，和年龄无关，一起改成了这个随年龄衰减的模型）。候选对象：异性、满14岁、
  不是近亲（查已经建好的`KinshipExperience`排除父母/子女/配偶/兄弟姐妹）、不是自己
  已有的情人。
- **候选查找**（`FindRomanticCandidate`）：最多尝试10次随机候选，找不到就放弃这一段/
  这次尝试，不强行凑数。

### 同学关系 + 虚拟学校/班级（`GenerateEducations`）

新建的虚拟`SchoolClass`实体（见`school.md`），`Populace::schoolClasses`持有。**按年份
正序推进的编年模拟**，不是一次性套公式分组——从每个citizen满6岁那年起、逐年判断谁该
入学/升学/是否摇号上大学：
- 小学(6-12岁)、中学(12-18岁)：义务教育，到年龄就100%入学。
- 大学(18-22岁)：到18岁时摇一次硬币（50%），只摇一次，不是每年摇。
- 分班：按`(EDUCATION_LEVEL, 入学年份)`分组，人数超过`kMaxClassSize`(35)就新开一个班。

由于是正序推进，"已经毕业的人"和"正在上学的人"不需要区分成两条路径——模拟推进到某人
毕业那年之后就不再变化（已毕业），推进到`currentYear`时人还卡在某个阶段中间就是"正在
上学"，一套年份循环天然同时覆盖两种情况。每个citizen自己的`EducationExperience`
（一对多里"一"的那一份）写完后，同步算出`GetLastGraduationYear()`（最后一次已完成
阶段的毕业年份，从未上过学/仍在读为-1，供`society.md`的入职历史反推用下限）。

**同学关系是全班互相认识**（班级人数几十人量级，全连接不算多）：对每个`SchoolClass`
遍历`GetStudents()`两两互相`AddAcquaintance`，不像同事关系那样只随机抽一部分（见
`society.md`）。

## `ApplyChange`/`FindCitizenByName`（这次重构`AForeverFrameworkActor::Tick`新增）

`Populace::ApplyChange(const Change*, const ScriptContext&)`——目前没有任何Change子类是
Populace域自己认识、需要处理的，空实现，也不打"未实现"警告（同一个Change会被转发给全部
六个Core域，只有`Story::ApplyChange`保留兜底警告）。

`Populace::FindCitizenByName(const std::string& name) const`——对`citizens`线性扫描比较
`GetName()`，找不到返回`nullptr`，不强制生成/持有任何Actor，纯Core层数据查询。存在的
原因：`AForeverFrameworkActor::ApplyChange`处理`NPCNavigateChange`时，不再像原来的
`Populace::Tick`回调那样直接拿到`Citizen*`形参（统一签名后只有`Change*`+`ScriptContext`），
只能反过来用change自带的occupant姓名（`NPCNavigateChange::GetName()`，构造时就是
occupantName）反查`Citizen*`——和`UForeverPopulaceFrameworkComponent::
FindOrSpawnCitizenByName`是同一个思路，一个在Core层纯查数据，一个在UE层顺带生成/复用
`ACitizenElement`。

## 不在这次范围内

- 老工程`Name`包装类的`reserve`/`roll`去重——见上"姓名生成"一节末尾。
- 除中文（`ChineseName`）外的其它取名算法（比如英文名）——`NameMod`接口已经是通用的，
  以后要加别的语言/风格直接新增一个具体实现+在`Populace::InitNames()`里按需切换
  `CreateName`的id即可。
- `Citizen`的父母/兄弟姐妹"是谁"这份原始数据本身不持久化（反推出来的兄弟姐妹关系
  结果已经保留，见上"四类人际关系生成"一节）、资产——见`citizen.md`（职业`job`/调度
  `scheduler`/性格`Personality`/人际关系`Relation`+`Experience`这几项均已落地）。
- `Citizen`通用的自由游走AI（没有工作驱动之外的自主移动）——有Job的市民已经能按调度
  走动，见上"Tick"一节、`Source/Forever/Element/CitizenElement.md`。
- 老工程`Map::Checkin`房产归属分配时顺带创建`Asset`对象登记进`adults[index]->AddAsset(
  asset)`——这次不迁移，`Asset`/`Player::assetFactory`所在的Player/Asset域还没进这个
  项目，`Citizen`这次也没有资产列表字段（见`citizen.md`）。归属本身（`owner`/`stated`）
  完整迁移了，只是不再额外生成一份"资产"记录。

### 两处曾经的简化，已按用户反馈依次修正

1. 第一版实现时`Citizen`只有姓名/性别/生日，没有配偶/子女字段，`Map::Checkin()`因此
   简化成"一人一间随机分配"——用户指出"所有人都是独自在一个房间里"不对，老工程Checkin
   本来就是"一个家庭消费一个room名额"（配偶+未成年子女一起搬进同一间）。修正方案：
   `Citizen`补上`spouse`/`children`两个字段（见`citizen.md`），
   `Populace::GenerateCitizens`模拟结束后额外做一趟"登记配偶/子女"的遍历（见上），
   `Map::Checkin()`照抄老工程算法（成年citizen各自抽房间，配偶90%概率+未成年子女无条件
   跟着搬进同一间）。
2. 紧接着又把"抽到住处的那个citizen"直接设成`room->owner`——用户指出"归属"和"住处"是
   两个独立概念，进一步要求把老工程完整的zone→building→room归属级联算法迁移过来，
   `owner`应该由这套独立算法决定，和"谁住在这里"没有必然关系。修正方案见上"房产归属"
   一节。
