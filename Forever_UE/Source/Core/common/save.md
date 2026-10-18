# save.h / save.cpp

## 职责

存档/读档的 Core 侧编排：manifest、段版本表、mod 状态的 JSON 读写实现、加载前校验、把存档的 mod 集合应用到当前 Config、恢复 mod 命名计数器。UE 侧的文件、压缩、对话框在 `Source/Forever/Mod/ForeverSaveSubsystem`。

这份文档同时是存档系统的总说明。各个域怎么存，见下面"段"一节列出的各自的 `*_save.md`。

## 需求

1. Tab 暂停界面里有"保存""加载"两个按钮，各自弹出文件选择窗口（`UI/ForeverPausePanelWidget`）。开始菜单也能加载存档（`UI/ForeverConfigMenuWidget` 的 `LoadGameButton`）。
2. 存档是单个文件（`.fsav`）。
3. 存档里不出现任何文件路径，换电脑、换文件夹都能用。
4. 加载时检查存档用到的 mod 是否都在，缺了就拒绝并报错。
5. 每个 mod 可以有自己的序列化函数（`SaveState`/`LoadState`，见 `Dependence/common/mod_state.md`）。

## 文件格式

- **外层**（UE 写）：`"FEVR"` + `SAVE_FILE_VERSION`（u32）+ 未压缩长度（u64）+ zlib 压缩体，见 `Dependence/common/save_archive.md`。
- **压缩体** = `SaveArchive`：若干个段，每段有名字、段版本、类型（JSON 或二进制）、数据。

## 段

| 段 | 内容 | 说明文档 |
|---|---|---|
| `manifest` | 用到的 mod、脚本名、布局名、计数器、版本 | 本文 |
| `world.map.terrain/roadnet/structures/vegetation` | 地图与建筑 | `Source/Core/map/map_save.md` |
| `world.populace` | 市民、班级、定时器、房产和住处归属 | `Source/Core/populace/populace_save.md` |
| `world.society` | 组织、岗位、组织定时器 | `Source/Core/society/society_save.md` |
| `world.traffic` | 站点、线路、车辆 | `Source/Core/traffic/traffic_save.md` |
| `world.scripts` | 全部 Script 的状态 | `Source/Core/story/script_save.md` |
| `world.player` | 时钟、生命值、物品、房间里的物品 | `Source/Core/player/player_save.md` |
| `ue`（JSON） | 操控的市民/车辆、位置朝向、视角、激活的武器、弹匣余量 | `Source/Forever/Mod/ForeverSaveSubsystem.md` |

`BuildArchive`/`BuildManifest` 接收 `SaveWorld`，即 map/populace/society/traffic/story/player 的指针。没生成的域传 nullptr，对应的段就不写。

读档时全部域按存档恢复，并跳过 game_start 广播。

## 为什么存结果、不存种子

世界生成不可复现：
- `GetRandom`/`GetRandomNormal` 每次调用都用 `random_device` 新建引擎；
- 每个 mod DLL 有自己的一份 Dependence 静态库，随机数状态也各自独立；
- 生成过程还依赖 `unordered_map` 的遍历顺序，有些地方以指针为键。

所以存档存的是生成结果本身，读档时按结果重建，不调用任何 mod 的 Layout/Distribute。好处是：只改生成算法不影响旧存档，也不用为此升版本号。

## 版本号（四层）

每一层只管自己的格式。改哪部分只升哪部分，旧存档不会因为改了一个域就全部要迁移。

| 层 | 位置 | 管什么 | 谁维护 |
|---|---|---|---|
| 文件格式版本 | 外层头 | 外壳布局：头字段、压缩方式、段表结构 | `SAVE_FILE_VERSION`/`SAVE_FILE_MIN_VERSION`（`Dependence/common/save_archive.h`） |
| 段版本 | 段表里每段各带一个 | 该段的数据布局 | `GetSaveSectionVersions`（本文件），每段 `{名字, current, minimum, required}` |
| mod 状态版本 | manifest 里每个 (concept, id) 各一个，每份 mod 状态前面也带一个 | mod 自己 `SaveState` 写出的数据 | mod 自己的 `GetStateVersion()` |
| 游戏版本字符串 | manifest | 只用于显示和排查，不参与判断 | `SAVE_GAME_VERSION`，发版时手动改 |

判断规则：
- 任何一层比当前代码新，就拒绝读档。
- 文件层和段层比当前旧时：比 minimum 还旧的拒绝；否则按版本号逐级迁移（迁移函数集中写在本文件里）。
- 游戏发布前，改了数据布局也**不升版本号、不写迁移**，旧存档失效也没关系，测试时重新存档即可；除非另有说明。发布之后才按"改了布局就升 current 并补迁移"执行。
- mod 层不由 Core 迁移：`StateReader::GetSavedVersion()` 交给 mod，由 mod 自己在 `LoadState` 里判断旧数据怎么读。

`required` 字段：
- `required=true` 的段缺失时，`ValidateArchive` 直接拒绝。比如只有 manifest 的 P0 旧存档。
- 人口、社会、交通、脚本状态、玩家、ue 段是 `required=false`：较早阶段的存档没有这些段，读档时对应的域重新生成，没有脚本状态段时 game_start 照常广播。新存档里所有段都有，不会走到这条路。

## 不存的内容

- 门开合到一半的动画状态：读档后门全部关着。
- 行驶中车辆的速度。
- 市民正在走的路线：存档时按"已经走到目的地"记，见 `Forever/Framework/ForeverPopulaceFrameworkComponent.md`。
- 手机和 App 的画面状态。
- 对话队列和谜题的中间状态：这两种情况下拒绝保存。
- UE 侧运行时增删的树：目前没有调用方。以后若要增删树，必须先改 Core 的 Vegetation 再同步到 UE，保证存档只存 Core 那一份。

## 跨域引用

`common/save_refs.h`（`SaveRefs` 命名空间）统一各域段之间的引用编码，只用各类的公开接口：
- Room/Component：（所属 Building 名字, 在 `GetRooms()`/`GetComponents()` 里的下标）；
- Time：年月日时分秒毫秒逐个写。

同一个文件里还有几个读写工具：
- `LiveIds`/`LookupId`：mod 实例到注册 id 的映射；
- `WriteModState`/`ReadModState`：mod 状态，即状态版本 + JSON 文本。

## 关键设计

- **manifest**（`SaveManifest`）：
  - `mods`：按 `(config.json 的 "<concept>_mods" key, modId)` 记，带参数字符串、`stateVersion`、`counter`。同一个 id 可能在不同 concept 里重名，所以必须带 concept。当前取"保存时启用的全部 mod"，比"真正用到的"略宽，以后可以收窄。
  - `storyScripts` / `mainStory`：Story screen 的脚本只存名字（不含路径和扩展名），读档时按 `Config::GetScriptPath` 重新解析成本机路径。
  - `scripts` / `layouts`：资源检查用的脚本名和 `.layout` 模板名（模板名取自各建筑的 `GetFloorSpecs` 快照）。
  - `gameVersion`：只用于显示和排查。
- **存档里不出现文件路径**：脚本、布局、mod 都只存名字和 id；`dll_paths/layout_paths/resource_paths` 不进存档。
- **21 个 concept 的统一操作表**（`GetConceptOps`，用模板 + 成员函数指针生成）：枚举存活实例（计数器、状态版本）、临时建实例取状态版本、恢复计数器、检查是否已注册。
- **加载前校验分两步**：
  1. `ValidateArchive`（不改状态）：段版本 → manifest → 每个 `(concept, id)` 都有已发现的 DLL 提供 → 每个 mod 声明的依赖也在存档的 mod 集合里 → 脚本和布局资源都能在配置的目录里找到。资源检查前会先 `Config::RescanFiles`，因为文件列表是启动时扫描缓存的。
  2. `ApplyManifestConfig` 把存档的 mod 集合和剧情脚本设成当前 Config（只改内存，不写回 config.json），并 `ReloadModArgs`。之后做两项检查：
     - `ValidateModsRegistered`：Registry 只在第一次 `Get()` 时加载 DLL，游戏内读档需要启动时没加载的 DLL，会在这里报出来，提示重启。
     - `ValidateModStateVersions`：只对 `stateVersion>0` 的 mod 临时建实例比较。

     任何一步失败，调用方用返回的快照 `RestoreConfig` 恢复。
- **命名计数器恢复**：`RestoreInstanceCounters` 由 `AForeverFrameworkActor::EnsureMapGenerated` 在地图按存档恢复之后、其余域恢复之前调用。之后新建的 mod 实例会从存档时的编号接着往下编，不和恢复出来的名字撞。
- **JsonStateWriter/Reader**：Writer 用"层级栈 + 闭合时挂回父节点"的方式构建 JSON，不持有指向容器内部的指针；Reader 只做 const 访问。二进制数据用 base64 存成字符串。

## 后续可选

- 读档时检查 mod DLL 的版本或哈希（现在只检查在不在）。
- 游戏内读档时加载启动时没加载的 DLL（现在只能重启）。
- 存档缩略图和元数据（保存时间、游戏内时间、操控的人物），供存档列表界面用。
- 缩小存档体积（目前压缩后约 20MB，大头是地形格子和导航图）。

## 依赖关系

- 依赖：`Dependence/common/{save_archive,mod_state,json}.h`、`common/config.h`、`common/registry.h`、各域的 `*_save.h`。
- 被谁依赖：`Source/Forever/Mod/ForeverSaveSubsystem`、`Source/Forever/Framework/ForeverFrameworkActor`。
