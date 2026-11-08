# populace_save.h / populace_save.cpp

## 职责

人口域的存档读写，类名 `PopulaceSaveCodec`，写成一个二进制段 `world.populace`。内容：
- 市民：姓名、性别、生日、性格、人际关系、经历、住处、当前所在房间、位置、生命值、Scheduler；
- 班级；
- Job 和 Scheduler 两套定时器；
- 取名器的占位名；
- 市民和地图之间的归属关系：Zone/Building/Room 的 owner，Room 的 households（每户的成员）/occupants。

存档系统总说明见 `Source/Core/common/save.md`，地图部分见 `Source/Core/map/map_save.md`。

## 段格式（v1）

按顺序写出：
1. `currentYear`。
2. 取名器：mod 注册 id、mod 状态、`reserve`（排序后写出）。
3. 第一遍：每个市民自身的字段，不含指向别的市民、班级、组织的引用。包括：
   - 13 项性格；
   - 人际关系（按对方姓名排序）；
   - 毕业年份；
   - 住处房间、当前所在房间；
   - 位置、生命值；
   - options；
   - Scheduler（mod 注册 id + mod 状态）；
   - 头像（`avatar` 字符串 + `avatarIndex`，追加在每个市民记录的最后；按项目约定没升段版本）。
4. 班级：学校名、学段、入学年份、学生下标。
5. 第二遍：配偶、子女、经历。经历分 5 类，用 `dynamic_cast` 区分：普通、亲属、感情、教育、工作。
6. Job 定时器、Scheduler 定时器。按 (时间, 市民下标, 节点名) 排序后写出：`std::set` 遇到同一时刻的条目时按指针地址排序，地址每次运行都不同。
7. 归属关系：
   - 每个 Zone（按名字排序）的 owner；
   - 每个 Building（按名字排序）的 owner；
   - 该 Building 每个房间的 owner、households（每户成员列表）、occupants。

## 关键设计

- **存结果**：和地图段一样，读档时不跑 `Populace::Init`，也不跑 `Map::Checkin`。
- **引用编码**：
  - 市民按 `Populace::citizens` 的下标引用；
  - 房间用 `SaveRefs::WriteRoom`/`ReadRoom`，即（所属 Building 名字, 房间下标），见 `common/save_refs.h`；
  - 地图必须已经按同一份存档恢复。
- **从房间推出其余住处字段**：市民的 lot/zone/building 不单独存。按 `Map::Checkin` 同一套规则从住处房间推出：`room->GetParentBuilding()`，再取它的 parentZone 和 parentLot。
- **工作经历里的组织等社会段来连**：`JobExperience` 里的 `Organization*` 要等社会段读完才能连上。所以先留空，把 (经历, 组织下标) 交给 `SocietySaveCodec::Load` 去连。
- **Scheduler 重建**：用存档里的注册 id 调用原来的构造函数，Script 的创建方式和新开局一样，再调用 `LoadState` 恢复 mod 状态。mod 的 `plans` 不用恢复：`Populace::Tick` 只在 `DailyPlan` 之后读一次 `plans` 来生成定时器，而定时器本身已经恢复了。
- **options**：读档会跳过 game_start 广播，所以按存档恢复。
- **不存档的字段**：`inEconomy`（见 `citizen.md`）、`lastTickTime`。行进模拟状态不再丢弃，见末尾"行进与实例化"。
- **段必须存在**：没有人口段或段损坏时 `Load` 返回 nullptr 并给出 error。
- `Load` 用 `unique_ptr<Populace>` 暂存新建的 `Populace`，失败路径（取名 mod 不可用、段截断、异常）自动析构并清空 `outPendingOrganizations`（其中的 `JobExperience*` 随之失效），成功才 `release`。

## 依赖关系

- 依赖：`populace/*`、`society/society.h`（只用来给组织编号）、`common/save_refs.h`、`common/save.h`。
- 被谁依赖：
  - `common/save.cpp`（`BuildArchive`）；
  - `Forever/Framework/ForeverFrameworkActor.cpp`（`EnsurePopulaceGenerated`）；
  - `Forever/Mod/ForeverSaveSubsystem.cpp`（`Forever.SaveRoundTrip`）。

## 待办 / 后续

- 房间里的物品（`Room::assets`）存在玩家段，见 `Source/Core/player/player_save.md`。

## 动态属性
每个市民记录末尾(avatarIndex之后)追加`U8 statsActive`+`F32 satiety`+`U8 departed`，读写严格对称。
人口段头部`currentYear`之后是`I32 initialPopulation`（开局市民总数，迁入机制的基准）。

## 行进与实例化
每个市民记录末尾(`departed`之后)追加，读写严格对称，按项目约定没升段版本：
- `U8 wasSpawned`：存档时这个市民是否有 Actor（UE 侧写入）。
- `U8 walking`；为 1 时再写：目的地房间（`WriteRoom`）、`F32 speed`、`U32 n`、n 个 `(F32 x, y, z)`。路径首点是存档时的当前位置，已走弧长为 0。

读档后：有路径的市民用 `BeginWalk(path, destination, 0, speed)` 恢复，起算时间由 UE 侧在 `GenerateCitizens` 里用 `RestartWalk` 对齐；
`wasSpawned` 的市民由 UE 侧先强制生成（不看距离和数量上限），保证看到的人不比存档前少。
