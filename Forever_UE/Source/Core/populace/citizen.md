# citizen.h / citizen.cpp

## 职责

`Citizen`：老工程里叫`Person`，这次进入populace域第一次迁移的市民数据类。只搬"姓名/性别/
生日"这三项 + 用户明确要求的"所在lot/园区/建筑/房间"位置字段 + "3D坐标"，不搬老工程`Person`
身上的relatives/personality/acquaintances/assets/educationExperiences等字段——依赖还没
迁移的Industry域，等对应域迁移到了再回来加，完整对照见`Source/Core/populace/populace.md`。
`jobs`/`scheduler`这两项后来分别在society域、Scheduler concept迁移时补上了，见下"`job`
字段"/"`scheduler`字段"两节。

## 关键设计

- **`spouse`/`children`（配偶/子女，第二版补入）**：第一版`Citizen`只有姓名/性别/生日，
  `Map::Checkin()`因此只能"一人一间随机分配"——被用户指出"所有人都独自一间"不对，老工程
  Checkin本来是"一个家庭消费一个room名额"。修正后`Populace::GenerateCitizens`模拟结束时
  额外做一趟"登记配偶/子女"的遍历（只有双方都幸存物化才登记），`Map::Checkin()`靠这两个
  字段决定"配偶/未成年子女要不要跟着一起搬进同一间"，见`populace.md`。**不保留**父母/
  兄弟姐妹等其它血缘关系——`Checkin`只需要配偶+子女这两种，其它关系没有消费方。
- **`GetAge(currentYear)`是粗略年龄**：按年份差，不精确到月/日——这次没有真正的日历/
  游戏时钟系统，`currentYear`来自`Populace::GetCurrentYear()`（模拟结束时的内部年份），
  够用来给`Map::Checkin()`做成年（≥18）/未成年判断。
- **纯Core类型，不`#include`任何UE头文件**——和`Building`/`Room`同一个约定。`Lot`/`Zone`/
  `Building`/`Room`在`citizen.h`里只做前向声明（只存指针，不解引用），3D坐标用裸`float`
  三元组而不是`FVector`，转UE坐标（`*SCALE`）的工作留给Forever层的`ACitizenElement`，和
  `ComputeWorldPosition`一类函数把map单位转UE单位的既有分工一致。
- **所在lot/园区/建筑/房间只是简单的get/set，没有级联清空**：老工程`Person::SetStatus`
  系列是给"人可能在zone/building/room间移动"场景用的（进城/离开某栋楼时把更粗粒度的字段
  级联清空）——这次`Citizen`分配住处后不会再变（没有"搬家"逻辑），保留最简单的setter就够，
  由`Map::Checkin()`一次性设好。
- **`GetRoom()`（家）和`GetCurrentRoom()`（当前物理位置）是两个独立字段，第三版补入**：
  用户指出人和房间实际上有3个概念——房间有且只有一个`owner`，0个或多个`tenants`（住在
  这里），0个或多个`occupants`（当前人在这里），三者不能混为一谈（见
  `Source/Core/map/room.md`）。`Citizen`这边对应补一个`currentRoom`字段，和`GetRoom()`
  （tenancy/家）分开存：`Map::Checkin()`分配住处时两者会同时设成同一个room（人刚分配到
  房间，此刻当然也在那），这是"初始状态恰好重合"，不代表两个概念可以合并成一个字段——
  以后有真正的移动AI之后，`currentRoom`会随着citizen走动而变化，`room`（家）不会跟着变。
  对称地，`Map::Checkin()`同时把citizen加进`room->AddTenant()`和`room->AddOccupant()`
  两个列表，Forever层渲染citizen位置时（`ACitizenElement::Init()`/
  `UForeverPopulaceFrameworkComponent`估算距离）读的是`GetCurrentRoom()`，不是
  `GetRoom()`——语义上"该把citizen摆在哪"永远该问"它现在在哪"，不是"它家在哪"。
- **3D坐标"留空/首次随机/此后复用"**：`hasPosition`为false表示"换了新房间之后从未在场景里
  实例化过"。真正的"首次随机、此后复用"判断和写入逻辑不在这个类里——`Citizen`只负责存储，
  Forever层的`ACitizenElement::Init()`才是真正读/写这几个字段的地方（见
  `Source/Forever/Element/CitizenElement.md`），这样Core端保持零UE依赖，判断逻辑和"要不要
  转UE坐标系"这类表现层关切放在一起。
- **`job`字段（进入society域新增）**：`Job*`，不持有所有权（`Job`由`Organization`持有，
  见`Source/Core/society/job.md`）——`Society::RecruitCitizens`把成年市民随机匹配到
  空缺Job时调`SetJob`。`GetJob()==nullptr`表示没有工作。
- **`scheduler`字段（Scheduler concept迁移新增，`Citizen`自己持有所有权）**：`Scheduler*`
  ——和`job`不同，`Scheduler`没有类似"组织"这样的自然归属者，最自然的归属就是它服务的
  那个市民自己，`Populace::AssignSchedulers()`（生成citizen之后加权随机分配，见
  `populace.md`）负责`new`+`SetScheduler`。因为这个字段的引入，`Citizen`**这次新增了
  一个真正的析构函数`~Citizen() { delete scheduler; }`**——在此之前`Citizen`是纯POD式
  生命周期，没有任何需要主动释放的成员，`Populace::~Populace()`原有的
  `for (Citizen* citizen : citizens) delete citizen;`不用改，析构链自动级联，详见
  `scheduler.md`。
- **`ClearPosition()`（进入society域新增）**：把`hasPosition`重置回`false`，不改
  `posX/Y/Z`本身（`hasPosition==false`时反正不会被读取）。给`Job`调度触发市民"瞬移"到
  新room（当前没有对应`ACitizenElement`、不需要算精确3D坐标）这个场景用，见
  `Source/Forever/Framework/ForeverPopulaceFrameworkComponent.md`"市民走路"一节——效果
  和"换房间后从未在场景里实例化过"是同一个状态，比直接塞一个"猜"出来的坐标更准确（下次
  真正生成时会按新房间重新算一次位置+随机偏移）。
- **生命周期由`Populace`持有**：`Citizen`对象本身由`Populace::GenerateCitizens()`
  `new`出来，存进`Populace::citizens`，`~Populace()`统一`delete`——`Citizen`自己没有
  factory/工厂查表机制（和`Room`不需要独立"factory查表创建"入口是同一个理由：不参与任何
  "地块竞争"式的顶层concept注册）。

## 依赖关系

- 依赖：`Source/Core/populace/scheduler.h`（`citizen.cpp`里`~Citizen()`需要`Scheduler`
  完整类型才能`delete`，`citizen.h`只前置声明）外加`Lot`/`Zone`/`Building`/`Room`/`Job`
  的前向声明。
- 被谁依赖：`Source/Core/populace/populace.h/.cpp`（`Populace::GenerateCitizens()`
  `new Citizen(...)`，`Populace::AssignSchedulers()`调`SetScheduler`，`~Populace()`
  析构）、`Source/Core/map/map.h/.cpp`
  （`Map::Checkin(const Populace&)`调用`SetLot`/`SetZone`/`SetBuilding`/`SetRoom`/
  `SetCurrentRoom`，以及`room->SetOwner`/`AddTenant`/`AddOccupant`）、
  `Source/Forever/Element/CitizenElement.h/.cpp`（`Init()`读`GetCurrentRoom()`+读/写
  位置字段）、`Source/Forever/Framework/ForeverPopulaceFrameworkComponent.h/.cpp`
  （流式生成/销毁时读`GetCurrentRoom()`估算距离）。

## 待办/后续阶段

- 父母/兄弟姐妹等其它血缘关系（配偶/子女已迁移）、性格/交情、资产——依赖还没迁移的
  Industry域（资产）或者本来就没有消费方（父母/兄弟姐妹/性格/交情），等对应域迁移到了
  再回来加，见`populace.md`"不在这次范围内"一节。职业（`job`）/调度（`scheduler`）
  这两项已经分别在society域、Scheduler concept迁移时补上。
- 通用的自由游走AI——进入society域后，有Job的市民已经能按调度（上下班）走动/寻路（见
  `job.md`/`Source/Forever/Element/CitizenElement.md`"WalkTo"一节），但没有Job、或者
  Job没有产生调度的市民仍然是"站在分配到的房间里的固定点"，没有工作驱动之外的自主移动。
