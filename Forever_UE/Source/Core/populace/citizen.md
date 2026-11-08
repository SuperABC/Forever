# citizen.h / citizen.cpp

## 职责

`Citizen`：市民数据类。存姓名/性别/生日、头像、所在lot/园区/建筑/房间（家）、当前所在房间、3D坐标、
配偶/子女链接、所属户、职业(`job`)、行为调度器(`scheduler`)、性格(`Personality`)、人际关系
(`acquaintances`+`experiences`)、生命值与死亡状态、饱食度、对话选项。不存资产。
同文件还有`Personality`（13项性格）和`Relation`（6项关系强度）两个小结构。

## 关键设计

- **纯Core类型，不`#include`任何UE头文件**：`Lot`/`Zone`/`Building`/`Room`只前向声明；3D坐标用裸`float`
  三元组，转UE坐标留给Forever层的`ACitizenElement`。
- **生命周期**：`Citizen`由`Populace`（`GenerateCitizens`/`SpawnCitizen`/读档）`new`并持有，`~Populace()`统一
  delete。`Citizen`自己独占`scheduler`和`experiences`，`~Citizen()`负责delete；`job`不持有（`Job`由`Organization`
  持有，见`society/job.md`）；`~Citizen()`不会把自己从房间`occupants`里摘掉，市民对象整局不销毁。
- **头像**：`avatar`是剧情脚本（`spawn_npc`的`avatar`）指定的头像名（`Resource/Avatars/`下不带扩展名的相对路径），
  空=没指定；`avatarIndex`是构造时`GetRandom(kAvatarIndexRange)`随机出的编号，UE侧对该性别图片张数取模得到
  具体哪一张（张数是磁盘文件数，Core不知道，所以存大范围随机数而不是下标）。Core只存数据、不碰图片，两个字段
  随人口段存档。见`Source/Forever/Player/ForeverAvatarSubsystem.md`。
- **`spouse`/`children`**：只保留模拟结束后双方都幸存的婚姻/亲子关系，供`Map::Checkin()`决定配偶/未成年子女
  是否搬进同一间。**不保留**父母/兄弟姐妹链接（没有消费方）；兄弟姐妹关系在生成时就写进了`acquaintances`/
  `KinshipExperience`。
- **`GetAge(currentYear)`是粗略年龄**：按年份差，不精确到月/日；`currentYear`来自`Populace::GetCurrentYear()`。
- **家和当前位置是两个独立字段**：`GetRoom()`（家，`Map::Checkin`一次性设好，不搬家）与`GetCurrentRoom()`
  （当前物理位置）不能合并。`SetCurrentRoom`同步维护房间`occupants`（旧房间移出、新房间加入；死人不加入）；
  读档直接赋值字段，不经过它。Forever层渲染位置读`GetCurrentRoom()`。所属户`GetHousehold()`由
  `Room::AddResident`维护，见`household.md`。
- **3D坐标**：`hasPosition`为false表示尚未算出位置；真正的读写逻辑在Forever层`ACitizenElement::Init()`
  （首次实例化按房间中心+随机偏移算出并写回，此后复用）。`ClearPosition()`只把`hasPosition`置回false，
  不改`posX/Y/Z`——用于“没有Actor的市民瞬移到新房间”。
- **行进模拟（无Actor的市民在路上，随人口段存档）**：`BeginWalk(path, destination, now, speed)`记录路径
  （地图单位）/出发时间/速度，并把位置设为路径首点；`AdvanceWalk(now)`按“已走时间×速度”沿路径推算位置写回
  `SetPosition`，走完`SetCurrentRoom(destination)`+`ClearPosition`并返回true；`GetRemainingWalk`取剩余路径
  （首点=当前推算位置）供生成Actor后接着走；`EndWalk`/`Die`清状态。`BeginWalk`本身不碰`currentRoom`，由UE侧
  （`UForeverPopulaceFrameworkComponent`）在开始走路时置nullptr（人在路上不属于任何房间）。时间由调用方给
  （UE侧传世界秒），Core不关心时钟来源。
  存档保存行进的即时状态：目的地、速度、路径（首点=当前位置）。存档前UE侧调`RebaseWalk(now)`把路径改写成
  "从当前推算位置起的剩余路径"，读档用`BeginWalk(path, dest, 0, speed)`恢复，再由UE侧`RestartWalk(now)`把起算时间
  对齐到新关卡的游戏时钟。`wasSpawned`（`WasSpawned`/`SetWasSpawned`）记录存档时这个市民是否有Actor，
  同样随人口段存档，读档后UE侧据此先把这些市民全部生成出来，生成完清掉，不参与游戏逻辑。
- **动态属性（目前只有饱食度）**：只有`statsActive`的市民参与`Populace::Tick`的每日衰减，避免全城实时模拟。
  `ActivateStats()`在被玩家`change_control`操控时调用（饱食度随机0.5~1），放手后保持实例化。`DecaySatiety(days)`
  乘`0.5^days`（一天减半），`AddSatiety`截断到[0,1]；`GetSatiety()`对外四舍五入到两位小数，内部存精确值（否则
  每帧衰减量小于0.005时永远降不下去）。目前没有任何后续影响（不饿死）。随人口段存档。
- **`IsInEconomy`**：false的市民不分配住处、不产生每日需求、不参与招聘/交税/补助（钱包照开）。开局市民默认true，
  `spawn_npc`的`economy`字段可置false。**不存档**：读档时开局市民都是true，spawn出来的false市民存档里本来
  就没有住处/岗位。
- **死亡与复活**：`isDead`是独立字段（不由`health<=0`推出）。`TakeDamage`把血量扣到0时调`Die()`；`Heal`/
  `TakeDamage`对死人无效。`Die()`：置`isDead`、血量归零、`EndWalk`、`SetCurrentRoom(nullptr)`、`ClearPosition`，
  **保留“家”**。`Revive(health=-1)`：清`isDead`，血量恢复（<=0或超过`maxHealth`取满血），**不设房间/位置**，
  需后续`npc_teleport`（destination可写`"home"`）才会在场景里出现；脚本入口是`npc_revive` Change。
  Forever层流式生成会跳过`IsDead()`的市民。默认100点血是占位值，待PIE调。
- **离城**：`departed`是另一个独立标记（不可逆）。`Depart()`：置`departed`、断开岗位（`Job::SetOccupant(nullptr)`）、`EndWalk`、`SetCurrentRoom(nullptr)`、`ClearPosition`，**保留“家”和户**，账户和房产由`Emigration`结算。`IsActive()`=!isDead&&!departed，经济结算和UE流式生成判断“人在不在场”都用它。`departed`随人口段存档。
- **`Personality`/`Relation`**：`Personality`13项各自`clamp(GetRandomNormal(0, 1/3), -1, 1)`（±1大致对应3σ）；
  `Relation`6项全部初始化为0。两者都支持按枚举和按脚本变量名（`operator()(name)`，未知名抛
  `InvalidArgumentException`）访问。`Personality`值域统一为[-1,1]（`AdjustPersonalityValue`同样clamp到[-1,1]）；
  `Relation`强度值域为[0,1]（`AdjustAcquaintanceValue`clamp到[0,1]）。
- **`Relation::category`**：必须创建时指定（无默认构造），`AddAcquaintance(name, category)`用`insert_or_assign`
  （`Relation`不可默认构造，不能用`operator[]`）。**配偶标记`RELATIONSHIP_KINSHIP`，不是`RELATIONSHIP_ROMANTIC`**：
  情感类别专指恋人/情人，婚姻归亲属（恋爱阶段的`EmotionExperience`是历史记录，不影响这里）。没做多类别优先级：
  同时符合多种关系时，后生成的pass覆盖前者，是已知简化而非bug。
- **`experiences`与`acquaintances`**：`GetExperiences/AddExperience`持有可追溯的历史记录，与`acquaintances`
  （关系强度）对应但不是一对一，见`experience.md`。`GetCurrentLovers()`是对`experiences`的派生查询（进行中的
  `RELATIONSHIP_ROMANTIC`经历中对方不是配偶的）；`GetLastGraduationYear()`是独立存储字段（citizen自己的求学
  事实，`Society::GenerateEmploymentHistory()`用它给入职年份定下限）。`GetAcquaintances()`返回整表，供调试日志
  遍历，正常逻辑按姓名查询。
- **`options`**：对话选项文本列表，`Populace::ApplyChange`处理`add_option`/`remove_option`，`MeetOption` UI读取。

## 依赖关系

- 依赖：`populace/experience.h`（`RELATIONSHIP_CATEGORY`）、`populace/scheduler.h`（`~Citizen()`需要完整类型）、
  `map/room.h`（`SetCurrentRoom`维护`occupants`）、`common/utility.h`（随机数）；`Lot`/`Zone`/`Building`/`Room`/
  `Job`/`Household`经`class.h`前向声明。
- 被谁依赖：`Populace`（创建、`SetScheduler`）、`Map::Checkin`/`SettleCitizen`（设置住处/当前房间）、
  `PopulaceSaveCodec`（友元，直接读写私有字段）、`Society`（`SetJob`）、Forever层的`ACitizenElement`/
  `UForeverPopulaceFrameworkComponent`。

## 待办/后续阶段

- 资产（`Citizen`上没有资产列表）。
- 没有Job或Job没有产生调度的市民仍是“站在分配到的房间里的固定点”，没有工作驱动之外的自主移动。
