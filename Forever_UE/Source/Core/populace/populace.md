# populace.h / populace.cpp

## 职责

`Populace`：和`Map`平级的顶层Core类，不是挂在`Map`底下的工具函数集合——`Populace`
不知道`Map`的存在，自己管理`Citizen*`的生命周期，`Map`只知道"有一个`Populace`可以
读"，通过`Map::Checkin(const Populace&)`把外部传入的citizens分配进自己的住宅room。
两者都由`AForeverFrameworkActor`平级持有（`Map* map`旁边`Populace* populace`）：

```cpp
populace = new Populace();
populace->Init(map->ComputeAccommodationTarget());
map->Checkin(*populace);
```

负责的事：年表模拟生成citizen人口、姓名生成（`Name`/`NameFactory`）、住处+房产归属
对接（`Map::Checkin`的数据来源）、Job调度驱动（`Populace::Tick`）、Scheduler加权
分配、四类人际关系生成（亲属/同学/情感三类，同事关系见`society.md`）。

## 关键设计

### 年表模拟算法

`Populace::Init(int accommodation)`先对`accommodation`做一次扰动（`target =
accommodation * exp(GetRandom(1000)/1000.0f - 0.5f)`），再调私有的
`GenerateCitizens(target)`跑年表模拟算法，几个容易出偏差的细节：

- **100男100女起始种子**：出生年`GetRandom(20)`（0~19），死亡年`birth+60+
  GetRandom(40)`（everyone都有），女性额外排一次婚姻年`birth+20+GetRandom(15)`。
- **`maleBirths`按出生年bucket-fill**：男性种子先按`birth`排序（初始100人这一步
  只排序一次，后续`LIFE_BIRTH`新增的男性永远`push_back`在末尾，不会再触发重排——
  数组下标一旦分配就终身稳定，`chronology`里排队的死亡/生育事件引用的下标不会因为
  后续操作失效），再从`currentBirth+1`扫到`males[i].birth`逐年填
  `maleBirths[year]=i`——这样`maleBirths[year]`就是"出生年≤year的最后一个男性数组
  下标"，婚姻匹配靠这个数组按年龄窗口二分区间取随机候选。
- **年表事件编码**：`chronology[year]`里每条`(eventIdx, LIFE_TYPE)`，
  `eventIdx>=0`表示女性数组下标，`eventIdx<0`表示`-男性数组下标`（真实下标从1开始，
  0留给哨兵，符号编码不会冲突）。
- **停止条件**：`year<100`无条件跑满，之后`year<4096`且**当前活人数**`aliveCount`
  达到`target`前继续（初始200人计入；出生`+1`，死亡事件把活着的人标成`LIFE_DEAD`时`-1`，
  不是累计出生过的人数）。注意最后物化成`Citizen`时还有约5%的随机丢弃，实际市民数
  略低于`target`。
- **婚姻匹配**：`lowId=max(0,birth-10)`/`highId=max(0,birth+5)`是出生年，
  `maleBirths[lowId]`/`maleBirths[highId]`才是数组下标区间，在这个区间里最多试10次
  随机候选，都是已婚就放弃（这个女性永远不会再收到新的婚姻事件，不重试）。
- **只把姓名/性别/生日+配偶/子女链接物化进`Citizen`**：父母/兄弟姐妹等其它血缘关系
  只在模拟用的内部临时结构（`populace.cpp`匿名namespace里的`Human`/`LIFE_TYPE`，
  不对外暴露）里用来推进人口数量，模拟结束后不保留（见`citizen.md`）。配偶/子女之所以
  要保留，是因为`Map::Checkin()`需要靠它们判断"一家人要不要搬进同一间"。只有模拟
  结束时活着（`life != LIFE_DEAD`）且额外通过95%存活过滤（`GetRandom(20) > 0`）的
  个体才会真正`new Citizen(...)`，物化时用`Human::idx`记下"这个人在`citizens`里的
  下标"，再单独一趟"登记配偶/子女"的遍历把幸存双方的`idx`翻译成`Citizen*`互相关联
  ——只有配偶/子女双方都幸存物化才登记，只有一方活下来的婚姻/亲子关系直接丢弃。生日
  年份用"2000+模拟内部年份"的换算；模拟结束时的年份（`year+2000`）存进
  `currentYear`，通过`GetCurrentYear()`供`Map::Checkin()`给`Citizen::GetAge()`用，
  也供`AForeverFrameworkActor::EnsurePlayerGenerated()`把开局游戏时钟设成这一年的
  1月1日8点（`player->SetTime(Time(populace->GetCurrentYear(), 1, 1, 8))`），见
  `Core/player/player.md`"开局时间=人口模拟结束年份"一节。

### 姓名生成：`NameMod`/`NameFactory`/`ChineseName`

`NameMod`接口三个纯虚方法：`GetSurname(fullName)`/`GenerateName(male,female,
neutral)`/`GenerateName(surname,male,female,neutral)`，都是"传一个set结果的
lambda进去"的callback写法——**不能改成直接按值返回`std::string`**：这是跨DLL
禁止模式（mod侧构造的`std::string`临时对象按值返回穿过DLL边界，分配/释放发生在
不同模块）；`std::function<void(const std::string&)>`按`const&`传入mod侧重写的
虚方法，回调只读`const string&`参数拷贝进调用方自己的`string`，和
`RoadnetMod::DistributeRoadnet`/`TerrainMod::DistributeTerrain`已经在用的
`std::function`回调传参是同一类安全模式。生成失败（候选词库为空等）时不调用
`setResult`，`Name`（Core层包装类，不跨DLL边界，不受这条规则约束）读到的结果保持
初始的空字符串。

`ChineseName`（`Source/Basic/populace/name_chinese.cpp`）：
- 姓氏表（200个单字，按常见程度从高到低排列）+男/女/中性给定名表（分别90/130/370
  个单字）全部硬编码`std::vector<std::string>`字面量，不走config/数据文件。
- 姓氏采样用"sqrt反索引"实现非均匀分布：`N=surnames.size()`，从`[0,N²)`均匀取
  `randVal`，`idx=floor(sqrt(randVal))`本身偏向大值，`surnameIdx=N-1-idx`把这个
  偏向反过来，让数组靠前（常见）的姓氏被抽中概率远高于靠后（罕见）的，不需要显式
  权重表。
- 给定名长度：`GetRandom(9)==0`时1个字（概率1/9），否则2个字（概率8/9）。每个字
  独立按`allowMale`/`allowFemale`/`allowNeutral`哪些词库被打开，从合并后的候选池里
  均匀抽取（一个2字名可以混合不同词库的字）；三个词库都关闭时兜底成只用中性词库。
- `GetSurname(fullName)`：手动解析UTF-8前导字节，截取姓名的第一个UTF-8字符作为姓
  ——这个方案假定姓氏永远是单字（这份姓氏表里成立），不做姓氏表查找匹配。

`Populace::InitNames()`（`Init()`一开始调用一次）：`nameFactory`是构造函数初始化
列表里绑定的引用成员（`Registry::Get().GetNameFactory()`），mod dll的发现/注册
全部集中到`Registry`（`Source/Core/common/registry.md`），只在整个UE进程生命周期
里跑一次：

```cpp
Populace::Populace() : nameFactory(Registry::Get().GetNameFactory()) {}
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
	// ...ReserveName，见name.md"ReserveName"一节
}
```

和`Map::InitRoadnet()`逐字同构：遍历`Config::GetConceptMods("name_mods")`挨个
`SetConfig(id, true)`，`GetName()`拿到胜出的id；`config.json`没配`name_mods`时
退化选第一个注册到的id（`GetRegisteredIds()[0]`）；两条路径都拿不到id才是真正的
致命配置错误（没有取名算法整个游戏就没法生成任何市民），`THROW_EXCEPTION`交给
`AForeverFrameworkActor::BeginPlay()`的`try/catch`统一处理。`EmptyName`正常注册在
`registries`里（`GetModNames`返回的id列表不受影响），只是永远不会被`SetConfig`
标记启用，`GetName()`不会选中它。`Populace`的析构函数`delete name`（`Name`析构里
调`nameFactory.DestroyName(mod)`释放真正的`NameMod*`）。

`Populace`通过`Core/populace/name.h/.cpp`的`Name`类转发`GetSurname`/
`GenerateName`，不直接持有/调用`NameMod*`，和`Terrain`/`Script`等其它concept
"上层聚合类只操作Core层包装类"的架构一致，见`name.md`。

**一处和老工程行为不同的点**：给最初100男100女种子生成姓名时，统一按"按性别选
对应词库+中性词库永远允许"（和`LIFE_BIRTH`事件一致），不是两个性别都固定传
`(male=false, female=true, neutral=true)`。

## 房产归属（`Map::Checkin`）

实际代码在`Map::Checkin()`里（`Populace`只管人口模拟，不知道`Map`/`Zone`/
`Building`/`Room`的存在），完整算法见`Source/Core/map/map.md`"人口初始化对接"
一节、字段设计见`Source/Core/map/room.md`/`zone.md`。几个关键决策：

- **"归属"和"住处"是两个独立概念**：一个room的owner由独立的"房产归属"流程决定
  （可能是从没在这里住过的人，即房东），和"谁实际住在这里"（`tenants`/
  `occupants`）没有必然关系。`Map::Checkin()`先跑完整的zone→building→room归属
  级联，再跑完全独立的住处分配循环，两者互不干扰。
- **"公有"（`stated`）结果真正随机可达**：触发条件是`zone->GetStated()`/
  `building->GetStated()`，用两个小概率（`kZoneStatedChance=2`、
  `kBuildingStatedChance=3`，满分`kProbabilityScale=100`）让"公有"分支真的能被
  随机命中（整体统一归属按公有/私有/下探到下一级三选一）——这两个具体数值是这个
  工程自己定的，按需可以调整。
- **zone内部building不重复roll**：显式跳过`GetParentZone()`非空的building，
  一个zone刚被判定"整体私有/公有"之后，它内部的building不会再被单独重新roll一次
  覆盖掉这个结果（会让级联归属形同虚设）。

## 依赖关系

- 依赖：`Source/Core/populace/citizen.h`（`GENDER_TYPE`/`Citizen`）、
  `Source/Core/populace/name.h`（`Name`）、`Source/Dependence/populace/
  name_factory.h`（`NameFactory`，引用成员`nameFactory`的类型）、
  `Source/Core/populace/scheduler.h`（`Scheduler`，`AssignSchedulers()`里
  `new`）、`Source/Dependence/populace/scheduler_factory.h`（`SchedulerFactory`，
  引用成员`schedulerFactory`的类型）、`Source/Core/story/script_factory.h`
  （`ScriptFactory`，引用成员`scriptFactory`的类型，传给每个`Scheduler`独占的
  `Script`）、`Source/Core/common/registry.h`（`Populace`构造函数绑定
  `nameFactory`/`schedulerFactory`/`scriptFactory`，见`registry.md`）、
  `Source/Dependence/common/utility.h`（`GetRandom`/`GetRandomNormal`/
  `Time::DaysInMonth`）、`Source/Core/populace/experience.h`
  （`KinshipExperience`/`EmotionExperience`/`EducationExperience`，见
  `experience.md`）、`Source/Core/populace/school.h`（`SchoolClass`，见
  `school.md`）。
- 被谁依赖：`Source/Core/map/map.h/.cpp`（`Map::Checkin(const Populace&)`读
  `GetCitizens()`）、`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`
  （持有`Populace*`，`EnsurePopulaceGenerated()`里`new`+`Init`+`Checkin`）、
  `Source/Forever/Framework/ForeverPopulaceFrameworkComponent.h/.cpp`
  （`GenerateCitizens(Map*, Populace*)`缓存`GetCitizens()`列表）。

## Tick：驱动Job的调度

`Populace::Tick(currentTime, crossedDay, onActions, post)`——`Job`的timer放在
这里（不是`Society`），因为`Job`由`Citizen`直接持有引用，`Populace`本来就要遍历
所有citizen，比按`Organization`→`Component`→`Job`反查"这个job的occupant是谁"更
直接。`post`（`PostHandle*`）原样透传给`job->DailyPlan`/`job->ExecNode`，
`Populace`自己不解读这个句柄，见`society/job.md`"按需查地址：PostHandle参数"一节。
`crossedDay`为true时遍历所有持有job的citizen，转调`job->DailyPlan(currentTime,
post)`生成今天的调度表，塞进`Populace`自己的`jobTimerSet`（`std::set<std::tuple
<Time, Citizen*, std::string>>`，按时间排序）；不论是否跨天，每帧从`jobTimerSet`
弹出最多`kMaxJobTimersPerTick`个到期节点执行，避免单帧处理过多到期节点卡顿。
**这个上限是`1`**——每个Job类型都用固定钟点（比如店员天天9点/12点），同一时刻到期
的节点数很容易远超每帧上限，PIE验证发现`kTimeFlowRatio`调快之后9点/12点这类"大量
市民同一时刻上下班"场景会卡顿好几帧，调到`1`让每帧的开销（尤其是
`Map::FindPedestrianPath`的Dijkstra寻路）更均摊,代价是把同一批到期节点摊得更久
才能处理完。Organization自己的调度是另一套完全独立的timer，放在`Society::Tick`
里，见`Source/Core/society/society.md`"Job的timer在Populace，Organization的
timer在Society"一节。

回调签名`(Citizen*, const vector<Change*>&)`——`Populace`自己不知道Actor层，调用方
（`AForeverFrameworkActor::Tick`）拿到这个`Citizen*`构造`ScriptContext`后，对每个
`Change*`统一转发给`AForeverFrameworkActor::ApplyChange`消费，见
`Source/Forever/Framework/ForeverFrameworkActor.md`"统一的Change消费入口：
`ApplyChange`"一节。

**第三套独立timer**：`schedulerTimerSet`+`kMaxSchedulerTimersPerTick`，和
`jobTimerSet`那一段结构完全平行（同一个`crossedDay`判断、同一个每帧上限式的弹出
循环、同一个`onActions`回调），只是换成`citizen->GetScheduler()`——`Scheduler`
负责citizen下班之后的行为，见`scheduler.md`"职责边界"一节。两套timer共用同一个
回调是因为回调签名本来就是通用的`(Citizen*, const vector<Change*>&)`，不关心
Change是`Job`产的还是`Scheduler`产的。

## `AssignSchedulers`：加权随机给每个citizen分配一个Scheduler

`Populace::Init()`结尾（`GenerateCitizens(target)`跑完、`citizens`列表已经就绪
之后）调用一次：对所有已注册的Scheduler类型（`schedulerFactory.GetRegisteredIds()
`）累加权重（`schedulerFactory.GetPower(id)`）建CDF，对每个citizen roll一个随机数
（`GetRandom(10000)/10000.f * total`）选中一个类型、`new Scheduler(&
schedulerFactory, &scriptFactory, selected, citizen)`。`SchedulerFactory`的权重
查询接口是`GetPower(id)`单个查询（照抄`OrganizationFactory`，`Society::Init`选
Organization类型已经在用同一套CDF算法，见`society.md`），详见`scheduler.md`"加权
随机分配"一节。所有已注册类型权重都是`0.f`（`total <= 0.f`）时退化成均匀随机，
保证人人都有一个Scheduler，不整体失败。

## 四类人际关系生成（个人属性 + 亲属/同学/情感三类，同事关系见society.md）

`Populace::Init()`结尾依次调用`GenerateRomanticRelations()`+
`GenerateEducations()`（`GenerateCitizens(target)`→`AssignSchedulers()`之后）。
**亲属关系（`GenerateKinshipRelations`，含配偶/子女/兄弟姐妹）不是独立的第三个
`Init()`步骤**——它折在`GenerateCitizens`函数体末尾直接调用，因为它需要读模拟阶段
`Human`结构里的结婚年份（`Human::marry`）/父母索引（`Human::father`/`mother`），
这些数据物化成`Citizen`后就丢弃了，只能在`females`/`males`两个数组还在作用域内、
也就是`GenerateCitizens`函数体结束之前处理。

个人属性（`Personality`）不需要额外的生成pass——`Personality`是`Citizen`的成员，
构造`Citizen`时自动跑默认构造，见`citizen.md`。

### 亲属关系（`GenerateKinshipRelations`，`populace.cpp`匿名namespace自由函数）

配偶/子女的信息`Populace::GenerateCitizens`早就有了（`Citizen::GetSpouse()`/
`GetChildren()`），亲属关系生成只是把这些已有链接记进`acquaintances`（随机赋
`Relation`数值）+`KinshipExperience`：
- **配偶**：`RELATIVE_SPOUSE`，`beginYear`是结婚年份——直接读`Human::marry`转成
  真实年份（`2000 + marry`）带出来，不重新计算（一个人一生只有一个结婚年份，不
  模拟离婚/再婚）。
- **子女**：父母一侧`RELATIVE_CHILD`，子女一侧`RELATIVE_PARENT`，`beginYear`是
  子女的出生年份。
- **兄弟姐妹**：按`(father index, mother index)`对`females`/`males`两个模拟
  数组临时分组，组内两两互相标记`RELATIVE_SIBLING`，`beginYear`取两人中较晚的
  出生年份。这份"父母是谁"的分组数据只在`GenerateKinshipRelations`这一次调用里
  用一次，不持久化成`Citizen`的字段——保留的是反推出来的兄弟姐妹
  `acquaintances`/`Experience`结果，不是"父母是谁"这条原始数据本身。

### 情感关系（`GenerateRomanticRelations`）

从"当前状态"反推历史：

- **已婚**：结婚年份直接读`GenerateKinshipRelations()`已经写好的
  `KinshipExperience(RELATIVE_SPOUSE)::GetBeginYear()`，不重新计算。恋爱开始
  年份在`[max(双方满14岁年份), 结婚年份]`之间随机取，写一条`EmotionExperience`
  （`beginYear`是恋爱开始年份，`endYear=-1`表示至今仍在一起，不模拟离婚——恋爱到
  婚姻是同一段感情的连续记录，不会在结婚那年断开重写一条）。
- **更早的恋爱史（可以有多段）**：按年份逐年推进的编年模拟，不是"按总跨度套公式
  一次性算出总段数"——`GeneratePastRelationships`逐年遍历`[满14岁年份,
  boundEnd)`，每一年是否开始一段新恋情按`DatingHazardForAge(当时年龄)`（见下）
  独立判定，年纪越大命中概率越低。时长用`pow(r,4)`让持续时间偏短。前任保留在
  `acquaintances`里，不因为关系结束就删除。
- **`DatingHazardForAge(age)`——按年龄递减的年度恋爱概率**：14-19岁10%、20-29岁
  14%、30-39岁7%、40-49岁3%、50-59岁1.2%、60岁以上0.5%（具体数值实现时可调，
  "随年龄递减"是硬要求）——不管是"更早的恋爱史"还是下面的"当前情人/婚外情"，基础
  概率都来自这一个函数，不是各自维护互相独立的常数。
- **当前情人（可以同时有多个，`GetCurrentLovers()`返回`vector`）**：基础概率同样
  是`DatingHazardForAge(当前年龄)`，已婚在此基础上再乘一个大幅降低的系数
  （`kMarriedMultiplier`，0.15）——已婚只尝试一次，不连续叠加多个婚外情人；未婚
  最多连续尝试3次、每次不命中就停止。候选对象：异性、满14岁、不是近亲（查已经
  建好的`KinshipExperience`排除父母/子女/配偶/兄弟姐妹）、不是自己已有的情人。
- **候选查找**（`FindRomanticCandidate`）：最多尝试10次随机候选，找不到就放弃
  这一段/这次尝试，不强行凑数。

### 同学关系 + 虚拟学校/班级（`GenerateEducations`）

新建的虚拟`SchoolClass`实体（见`school.md`），`Populace::schoolClasses`持有。按
年份正序推进的编年模拟，不是一次性套公式分组——从每个citizen满6岁那年起、逐年
判断谁该入学/升学/是否摇号上大学：
- 小学(6-12岁)、中学(12-18岁)：义务教育，到年龄就100%入学。
- 大学(18-22岁)：到18岁时摇一次硬币（50%），只摇一次，不是每年摇。
- 分班：按`(EDUCATION_LEVEL, 入学年份)`分组，人数超过`kMaxClassSize`(35)就新开
  一个班。

由于是正序推进，"已经毕业的人"和"正在上学的人"不需要区分成两条路径——模拟推进到
某人毕业那年之后就不再变化（已毕业），推进到`currentYear`时人还卡在某个阶段中间
就是"正在上学"，一套年份循环天然同时覆盖两种情况。每个citizen自己的
`EducationExperience`（一对多里"一"的那一份）写完后，同步算出
`GetLastGraduationYear()`（最后一次已完成阶段的毕业年份，从未上过学/仍在读为-1，
供`society.md`的入职历史反推用下限）。

**同学关系是全班互相认识**（班级人数几十人量级，全连接不算多）：对每个
`SchoolClass`遍历`GetStudents()`两两互相`AddAcquaintance`，不像同事关系那样只
随机抽一部分（见`society.md`）。

## `ApplyChange`/`FindCitizenByName`

`Populace::ApplyChange(const Change*, const ScriptContext&)`——处理市民的`add_option`/`remove_option`，
返回是否认识这个类型(七个域都不认识时由`AForeverFrameworkActor::ApplyChange`统一打"未实现"警告)。

`Populace::FindCitizenByName(const std::string& name) const`——对`citizens`
线性扫描比较`GetName()`，找不到返回`nullptr`，不强制生成/持有任何Actor，纯Core层
数据查询。`AForeverFrameworkActor::ApplyChange`处理`NPCNavigateChange`时，只能
反过来用change自带的occupant姓名（`NPCNavigateChange::GetName()`）反查
`Citizen*`——和`UForeverPopulaceFrameworkComponent::FindOrSpawnCitizenByName`
是同一个思路，一个在Core层纯查数据，一个在UE层顺带生成/复用`ACitizenElement`。

## 待办/后续阶段

- 老工程`Name`包装类的`roll`去重——见`name.md`"待办"一节。
- 除中文（`ChineseName`）外的其它取名算法（比如英文名）——`NameMod`接口已经是
  通用的，以后要加别的语言/风格直接新增一个具体实现+在`Populace::InitNames()`里
  按需切换`CreateName`的id即可。
- `Citizen`的父母/兄弟姐妹"是谁"这份原始数据本身不持久化（反推出来的兄弟姐妹
  关系结果已经保留）、资产——见`citizen.md`。
- `Citizen`通用的自由游走AI（没有工作驱动之外的自主移动）——有Job的市民已经能按
  调度走动，见上"Tick"一节、`Source/Forever/Element/CitizenElement.md`。
- 老工程`Map::Checkin`房产归属分配时顺带创建`Asset`对象登记进
  `adults[index]->AddAsset(asset)`——不迁移，`Asset`/`Player::assetFactory`所在
  的Player/Asset域还没进这个项目，`Citizen`也没有资产列表字段。归属本身
  （`owner`/`stated`）完整迁移了，只是不再额外生成一份"资产"记录。
