# citizen.h / citizen.cpp

## 职责

`Citizen`：市民数据类。存姓名/性别/生日、所在lot/园区/建筑/房间、3D坐标、配偶/子女链接、
职业(`job`)、行为调度器(`scheduler`)、性格(`Personality`)、人际关系(`acquaintances`+
`experiences`)。不存资产(assets)——依赖还没迁移的Industry域。

## 关键设计

- **头像(`avatar`/`avatarIndex`)**：`avatar`是剧情脚本(`spawn_npc`的`avatar`)指定的头像名，空=没指定；`avatarIndex`是构造时`GetRandom(kAvatarIndexRange)`随机出的自动头像编号，UE侧对该性别图片张数取模得到具体哪一张(张数是磁盘上的文件数，Core不知道，所以存大范围随机数而不是下标)。Core只存数据、不碰图片文件，两个字段随人口段存档(没升段版本)；见`Source/Forever/Player/ForeverAvatarSubsystem.md`。
- **`spouse`/`children`**：`Map::Checkin()`按"一个家庭消费一个room名额"分配住处
  （配偶/未成年子女跟着一起搬进同一间），需要这两个字段。**不保留**父母/兄弟姐妹等
  其它血缘关系——`Checkin`只消费配偶+子女，其它关系没有消费方。
- **`GetAge(currentYear)`是粗略年龄**：按年份差，不精确到月/日——没有真正的日历/游戏
  时钟系统，`currentYear`来自`Populace::GetCurrentYear()`，够用来给`Map::Checkin()`
  做成年（≥18）/未成年判断。
- **纯Core类型，不`#include`任何UE头文件**——和`Building`/`Room`同一个约定。`Lot`/
  `Zone`/`Building`/`Room`在`citizen.h`里只做前向声明（只存指针，不解引用），3D坐标用
  裸`float`三元组而不是`FVector`，转UE坐标的工作留给Forever层的`ACitizenElement`。
- **所在lot/园区/建筑/房间只是简单的get/set，没有级联清空**——`Citizen`分配住处后不会
  再变（没有"搬家"逻辑），由`Map::Checkin()`一次性设好。
- **`GetRoom()`（家）和`GetCurrentRoom()`（当前物理位置）是两个独立字段**：房间有且
  只有一个`owner`，0个或多个`tenants`（住在这里），0个或多个`occupants`（当前人在这里），
  三者不能混为一谈（见`Source/Core/map/room.md`）。`Map::Checkin()`分配住处时两者会
  同时设成同一个room，这是初始状态恰好重合，不代表两个概念可以合并——以后有真正的移动
  AI之后，`currentRoom`会随着citizen走动而变化，`room`（家）不会跟着变。对称地，
  `Map::Checkin()`同时把citizen加进`room->AddTenant()`和`room->AddOccupant()`两个
  列表；Forever层渲染citizen位置时读的是`GetCurrentRoom()`，不是`GetRoom()`——语义上
  "该把citizen摆在哪"永远该问"它现在在哪"，不是"它家在哪"。
- **3D坐标"留空/首次随机/此后复用"**：`hasPosition`为false表示"换了新房间之后从未在
  场景里实例化过"。真正的判断和写入逻辑不在这个类里——Forever层的`ACitizenElement::
  Init()`才是真正读/写这几个字段的地方（见`Source/Forever/Element/CitizenElement.md`），
  这样Core端保持零UE依赖。
- **`job`字段**：`Job*`，不持有所有权（`Job`由`Organization`持有，见
  `Source/Core/society/job.md`）——`Society::RecruitCitizens`把成年市民随机匹配到
  空缺Job时调`SetJob`。`GetJob()==nullptr`表示没有工作。
- **`scheduler`字段（`Citizen`自己持有所有权）**：`Scheduler*`——`Scheduler`没有类似
  "组织"这样的自然归属者，最自然的归属就是它服务的那个市民自己，
  `Populace::AssignSchedulers()`负责`new`+`SetScheduler`。`~Citizen() { delete
  scheduler; }`是`Citizen`唯一需要主动释放的成员，详见`scheduler.md`。
- **`ClearPosition()`**：把`hasPosition`重置回`false`，不改`posX/Y/Z`本身
  （`hasPosition==false`时反正不会被读取）。给`Job`调度触发市民"瞬移"到新room（当前
  没有对应`ACitizenElement`、不需要算精确3D坐标）这个场景用，见
  `Source/Forever/Framework/ForeverPopulaceFrameworkComponent.md`"市民走路"一节——
  效果和"换房间后从未在场景里实例化过"是同一个状态，下次真正生成时会按新房间重新算
  一次位置+随机偏移。
- **生命周期由`Populace`持有**：`Citizen`对象由`Populace::GenerateCitizens()`
  `new`出来，存进`Populace::citizens`，`~Populace()`统一`delete`——`Citizen`自己没有
  factory/工厂查表机制，不参与任何"地块竞争"式的顶层concept注册。
- **`Personality`/`Relation`**：`Personality`13个字段各自`clamp(GetRandomNormal(0,
  1/3), -1, 1)`（正态分布，标准差1/3让±1边界大致对应3σ）；`Relation`6个字段全部初始化
  为0。`GetRandomNormal`是`Source/Dependence/common/utility.h/.cpp`的正态分布随机数
  helper，和`GetRandom(int)`同一个"每次调用现场构造引擎"风格。
- **`Relation::category`（`RELATIONSHIP_CATEGORY`，标记两人是四类关系里的哪一种）**：
  必须在创建时就指定类别（`Relation(RELATIONSHIP_CATEGORY)`，不可默认构造），
  `Citizen::AddAcquaintance(name, category)`内部用`acquaintances.insert_or_assign`
  而不是`operator[]`。**配偶标记`RELATIONSHIP_KINSHIP`（亲属），不是
  `RELATIONSHIP_ROMANTIC`**——情感类别专指恋人/情人，婚姻关系本身归亲属（对应的恋爱
  阶段仍然在`experiences`里有一条`EmotionExperience`，那是历史记录，不影响
  `acquaintances`这条的`category`）。`category`没有做"多类别取交集/优先级"处理——如果
  两个citizen同时符合多种关系（比如正好是兄弟姐妹又是同班同学），谁的生成pass后调用
  `AddAcquaintance`就以谁的类别为准（`insert_or_assign`直接覆盖），这是当前的已知
  简化，不是bug。
- **四类人际关系（亲属/同学/同事/情感）+ `Experience`体系**：`Citizen`持有
  `experiences`（`vector<Experience*>`，取得所有权，`~Citizen()`统一delete）+
  `GetExperiences`/`AddExperience`，和`acquaintances`（关系强度dial）建立对应关系——
  但不是"一对一"：亲属(`KinshipExperience`)/情感(`EmotionExperience`)各自指向一个
  具体`other`，一条对应一条`acquaintances`；同学(`EducationExperience`)/同事
  (`JobExperience`)是"一对多"，一条`Experience`表达"我自己"参与的一段经历（在某个班/
  组织待过），不指向具体某个人，班上/组织里其他人的`acquaintances`由
  `Populace::GenerateEducations()`/`Society::GenerateEmploymentHistory()`另外派生。
  详见`experience.md`。`GetAcquaintances()`返回整张`unordered_map<string,
  Relation>`，供`Source/Forever/Element/CitizenElement.md`"T键：输出附近市民的人际
  关系数据"一节的调试日志用，正常游戏逻辑按姓名查询即可。另有两个查询：
  `GetCurrentLovers()`（当前情人，可以同时有多个——对`experiences`的派生查询，不是
  独立字段：找所有`RELATIONSHIP_ROMANTIC`类别、`IsOngoing()`为true、且`GetOther()`
  不是当前配偶的`EmotionExperience`）；`GetLastGraduationYear()`/
  `SetLastGraduationYear()`（独立存储字段，不是派生查询——这是citizen自己的求学阶段
  事实，不是和某个具体他人的关系；`Society::GenerateEmploymentHistory()`用它给入职
  年份定下限）。四类关系的具体生成算法见`populace.md`"四类人际关系生成"一节和
  `Source/Core/society/society.md`。

## 依赖关系

- 依赖：`Source/Core/populace/scheduler.h`（`citizen.cpp`里`~Citizen()`需要
  `Scheduler`完整类型才能`delete`，`citizen.h`只前置声明）外加`Lot`/`Zone`/
  `Building`/`Room`/`Job`的前向声明。
- 被谁依赖：`Source/Core/populace/populace.h/.cpp`（`Populace::GenerateCitizens()`
  `new Citizen(...)`，`Populace::AssignSchedulers()`调`SetScheduler`，
  `~Populace()`析构）、`Source/Core/map/map.h/.cpp`（`Map::Checkin(const
  Populace&)`调用`SetLot`/`SetZone`/`SetBuilding`/`SetRoom`/`SetCurrentRoom`，
  以及`room->SetOwner`/`AddTenant`/`AddOccupant`）、`Source/Forever/Element/
  CitizenElement.h/.cpp`（`Init()`读`GetCurrentRoom()`+读/写位置字段）、
  `Source/Forever/Framework/ForeverPopulaceFrameworkComponent.h/.cpp`
  （流式生成/销毁时读`GetCurrentRoom()`估算距离）。

## 待办/后续阶段

- 父母/兄弟姐妹链接不持久化在`Citizen`上（只在`Populace::GenerateCitizens`模拟过程中
  用来反推兄弟姐妹`acquaintances`/`KinshipExperience`，之后即丢弃）——需要查"某人父母
  是谁"仍然没有消费方。
- 资产——依赖还没迁移的Industry域，等对应域迁移到了再回来加。
- 通用的自由游走AI——有Job的市民已经能按调度（上下班）走动/寻路（见`job.md`/
  `Source/Forever/Element/CitizenElement.md`"WalkTo"一节），但没有Job、或者Job没有
  产生调度的市民仍然是"站在分配到的房间里的固定点"，没有工作驱动之外的自主移动。

## 死亡与复活
- `isDead`是独立字段(不再由`health<=0`推出)，存档在`maxHealth`之后多存一个U8。`TakeDamage`把血量扣到0时调用`Die()`；`Heal`对死人无效。
- `Die()`：`isDead`置位、血量归零，从当前房间`occupants`移除并清空`currentRoom`/位置；**保留“家”**(`room`/tenant/building/zone/lot)。
- `Revive(health=-1)`：清除`isDead`，血量恢复(<=0或超过`maxHealth`取满血)；**不设房间/位置**，要后续`npc_teleport`(destination可写`"home"`)才会在场景里出现。脚本入口是`npc_revive` Change。
- `SetCurrentRoom`现在同步维护房间`occupants`(旧房间移出、新房间加入；死人不加入)，`Map::SettleCitizen`不再单独`AddOccupant`。读档直接赋值字段，不经过它。
