# 存档/读档功能（本地版，基于云端 archive.md，已对照当前代码核实并补全）

## Context
Forever 要往"高自由度 GTA"方向做，存档是第一块基础设施。需求六条：
1. Tab 暂停界面，有保存、加载两个按钮，各自弹出文件选择窗口。
2. 存档是**单个文件**。
3. 开始菜单也能加载存档。
4. **存档里不出现任何文件路径**，换电脑、换文件夹都能用。
5. 加载时检查存档用到的 mod 是否都在，缺 DLL 就拒绝并报错。
6. 每个 mod 可以有专属序列化函数，把自己的内部数据写进存档。

云端方案（`archive.md`）的整体设计不变。我对照当前代码逐条核实了它依赖的前提，全部成立，只有个别位置需要更正；另外补上了它写完之后新增的状态（细高度、植被、装饰模型等）。

### 已核实的前提
- **世界生成不可复现**：`GetRandom` / `GetRandomNormal` 每次调用都用 `random_device` 新建引擎（`Dependence/common/utility.cpp:35,43`），生成过程还依赖 `unordered_map` 的遍历顺序。所以存档**存完整的生成结果**，加载时按结果重建，不重新生成。
- **可复用的现成设施**：
  - `Dependence/common/json.h`（`JsonValue` / `JsonReader` / `FastWriter`），Core、Basic、mod、UE 都已在用。
  - `UForeverConfigBridgeSubsystem::SelectFile/SelectFolder`，以及匿名命名空间里的 `GetDialogParentWindowHandle()`（`ForeverConfigBridgeSubsystem.cpp:24`）。目前没有"保存文件"对话框。`DesktopPlatform` 已经是依赖。
  - `ValidateAndStartGame`（`ForeverConfigBridgeSubsystem.cpp:136`）里现成的依赖校验和启动流程，以及 `UForeverModSubsystem::EnsureModsRegistered`。
  - `Config` 的 `GetScriptPath / GetLayouts / GetConceptMods / GetModEnables / IsModIdEnabled / WriteConfig`，以及 `Registry::ReloadModArgs`。Registry 只在第一次 `Get()` 时注册 DLL。
- **暂停界面还不存在**：`PausePanel` 只出现在 `ForeverPlayerController.md` 里。Tab 键没有被占用，Esc 绑的是 `CloseMap`。
- **mod 基类有 21 个**：App、Asset、Building、Component、Job、Manufacture、Name、Organization、Product、Puzzle、Roadnet、Room、Route、Scheduler、Script、Station、Storage、Terrain、Vehicle、Weapon、Zone。各 Factory 的 `liveInstances` 是私有的；mod 实例名来自 DLL 里的静态计数器，**不能当作稳定键**。

### 对云端方案的更正
- `Node::count` 定义在 **Dependence**（`general_geometry.cpp:11/50`），不在 Core。Dependence 是静态库，宿主（Core/UE）和每个链接了它的 DLL（Basic.dll，以及构造 Node 的外部 mod）各有一份。加载后宿主这份要 `SetCount(maxId+1)`；mod 那份照旧由 `DistributeRoadnet` 传入的 `nodeStaticCount` 同步，读档时不会再跑 roadnet mod，所以不受影响。
- `premise` 是 milestone 节点上的字段（`milestone.h:155`），不在 `Script` 上。脚本状态要存每个 milestone 节点的 `premise`，以及 `Script::actives/variables`。
- "开局随机占有一个市民"实际发生在 `Core/common/implement.cpp:36-47`（`game_start` 被处理成 `ChangeControlChange`，再由 `ForeverFrameworkActor.cpp:214` 执行）。读档时要跳过 `BroadcastGameStart`（`ForeverFrameworkActor.cpp:466`）。
- `ForeverCharacter::PossessedBy` 当前在 `ForeverCharacter.cpp:155-206`：181-183 行删除肩上武器和背包，185-200 行重建并各塞 120 发弹药。

### 云端方案之后新增、必须存的状态
- `Element::fineHeights`（`map.h:40`，按需分配的 17×17 细高度数组），和地形格子放在同一个二进制段。
- `Map::vegetation` 的 `TreeInstance` 列表（`vegetation.h:78`）。**存展开后的结果**，读档时不重跑任何 `LayoutTrees`，因为里面有随机。各 mod 的 `trees`（`TreeRequests`）和 `Roadnet::trees` 只是生成用的中间数据，不用存。
- Zone 的 `direction` 和 `decorations`（`zone.h:133-134`），Building 的 `direction` 和 `decorations`（`building.h:476/485`）。直接存 `DecorationInstance`，不重新换算。
- 每层的 `FloorAssetSpec::buildShell`，跟着 `mod->floors` 快照一起存。
- UE 的 `UForeverVegetationFrameworkComponent::AddTree/RemoveTree` 目前没有调用方，运行时增删的树暂不存。规定：以后若有游戏逻辑要增删树，必须先改 Core 的 Vegetation 再同步到 UE，保证存档只存 Core 一份。这条写进 `.md`。

## 总体设计

### 1. 存档文件格式：单文件，不含路径
- **外层（UE 负责）**：`magic "FEVR"` + `fileVersion` + 未压缩长度 + zlib 压缩体，压缩用 UE 的 `FCompression`。Core 不依赖压缩库。扩展名 `.fsav`。
- **压缩体**：Core 产出的分段容器，新增 `Dependence/common/save_archive.{h,cpp,md}`。
  - 小结构（manifest、各类记录、mod 快照、脚本状态）用 `JsonValue`。
  - 大数组（地图格子含细高度、导航图、`throughLines`、行道树、人际关系）用二进制段。
  - 段列表：`manifest`、`world.map.*`、`world.vegetation`、`world.populace`、`world.society`、`world.traffic`、`world.story`、`player`、`ue`。
- **怎样做到不含路径**：
  - 脚本和布局只存**名字**，加载时解析方式和初始化时一样：脚本用 `Config::GetScriptPath(裸名)`，布局按 `Config::GetLayouts()` 的模板名（文件名去扩展名）匹配。
  - `config.json` 里的 `dll_paths / layout_paths / resource_paths` 一律不进存档，只存各 mod 的 `(概念, id, 参数串)`。
  - `/Game/...` 这类引擎虚拟资产路径照原样存，它们不是文件系统路径。

### 1.5 版本号
分四层，每一层只管自己的格式。哪部分改了就只升哪部分的号，不会因为改了一个域就让所有旧存档都要迁移。

| 层级 | 位置 | 管什么 | 谁维护 |
|---|---|---|---|
| 文件格式版本 `fileVersion` | 外层头，紧跟 `magic "FEVR"` | 外壳布局：头字段、压缩方式、段表结构 | `Dependence/common/save_archive.h` 里的 `constexpr int SAVE_FILE_VERSION` |
| 段版本 `sectionVersion` | 段表里每个段各带一个 | 该段的数据布局（字段增删、二进制编码变化） | `Core/common/save.h` 集中列一张表（`SAVE_VERSION_MAP_CELLS`、`SAVE_VERSION_POPULACE` 等），改某段格式时只升那一个 |
| mod 状态版本 `stateVersion` | manifest 里每个 `(概念, id)` 各记一个 | 该 mod 自己 `SaveState` 写出的数据 | mod 自己：基类再加 `virtual int GetStateVersion() const { return 0; }` |
| 游戏版本字符串 `gameVersion` | manifest | 只用于显示和排查（如 `"0.1.0"`），不参与判断 | 一个常量，发版时手动改 |

**判断规则**（全部在加载前校验时做，失败就拒绝并报出具体是哪一层、哪个段或哪个 mod）：
- **存档的版本比当前代码新**：任何一层都拒绝加载，提示"存档来自更新的版本"。旧代码读不懂新格式。
- **存档的版本比当前代码旧**：
  - 文件层和段层：按版本号逐级执行迁移函数（v1→v2→…→当前），迁移函数集中登记在 `save.cpp`，每个段一张"从版本 N 升到 N+1"的表。还不支持的旧版本（比当前代码设定的最低可读版本还旧）拒绝加载。
  - mod 层：不由 Core 迁移。`StateReader` 提供 `GetSavedVersion()`，mod 在 `LoadState` 里自己判断旧版本数据怎么读。`stateVersion` 为 0 表示这个 mod 没声明版本，按 0 原样交给它。
- **版本相同**：直接读。
- **mod 版本的预检查**：加载前要拿到当前 mod 的 `GetStateVersion()`，需要一个实例。校验时用 `Factory::Create` 临时建一个再销毁。这会让命名计数器多加一，但读档完成后计数器会统一恢复成存档值（见第 3 节），不受影响。

**写入方式**：`StateWriter` 在调用 `mod->SaveState` 前自动记下 `mod->GetStateVersion()`，mod 不用自己写版本。往返一致性测试（见"验证"）同时比较各层版本号。

### 2. mod 与资源检查
- **manifest 内容**：
  - 存档用到的每个 `(conceptKey, modId, 参数串)`。必须按"概念 + id"记，`product_wheat` 和 `weapon_pistol` 这类 id 在不同概念里重名。
  - 依赖对（`GetModDependencies`）。
  - 用到的脚本裸名：主线脚本，加上所有 Door/Job/Organization/Scheduler/Vehicle/Story 的 `scriptModName` 和 milestone 名。给 `Script` 新增 `loadedMilestones` 来汇总。
  - 用到的布局模板名：所有已存建筑 `FloorLayoutSpec.templateName` 去重。
  - 版本信息：各 `(概念, id)` 的 `stateVersion`、`gameVersion`（文件版本在外层头，段版本在段表，见 1.5 节）。
- **加载前校验**（菜单和游戏内加载都在 `OpenLevel` 之前做）：
  0. 版本检查（见 1.5 节）：文件版本、各段版本、各 mod 的状态版本，比当前新就拒绝，比当前旧就确认有迁移路径。
  1. 用 `Config::GetModEnables()` 和 DLL 概念表检查每个 `(概念, id)` 都有 DLL 提供，缺一个就拒绝，并列出缺失项。报错风格沿用 `ValidateAndStartGame`。
  2. 复用 `ValidateAndStartGame` 的依赖校验。
  3. 资源检查：新增 Core 纯函数 `Config::CheckSaveResources(scripts, layouts)`，返回缺失列表，UE 只负责显示。
  4. 通过后，把存档里的 `<概念>_mods` 写入 Config，再调 `Registry::ReloadModArgs()`。
  5. 游戏内加载时，如果需要的 DLL 在启动时没被发现，直接拒绝并提示重启。这是现状限制，写进 `.md`。
- 不做 DLL 哈希或版本校验，列为后续可选项。

### 3. mod 专属序列化钩子
- 新增 `Dependence/common/mod_state.{h,cpp,md}`：宿主实现、mod 调用的抽象类 `StateWriter` / `StateReader`。
  - 全部是虚函数，键值式接口：`SetInt / SetDouble / SetBool / SetString / SetBytes`、`BeginObject / EndObject / BeginArray`，Reader 有对应的 `Get*`。
  - 字符串以 `const char*` 返回，指向宿主内存，mod 自己拷贝。
  - 内存分配全在宿主侧，不跨 DLL 释放。
- 在 21 个 mod 基类**末尾**追加两个虚函数：`virtual void SaveState(StateWriter&) const {}` 和 `virtual void LoadState(StateReader&) {}`，默认空实现。
  - **这是对 mod 基类签名的改动，属于你这次需求第 6 条明确要求的**（依据：[[no-mod-class-signature-changes]]）。
  - 改完要重编 Basic 和三个外部 mod 工程。
- **保存并恢复每个 mod 类的静态命名计数器**（如 `ResidenceBuilding::count`）。目的是读档后新建的实例不会和恢复出来的名字撞车。
  - 计数器是各 mod 类私有的 static，只有 mod DLL 自己能读写。所以在 21 个基类末尾，和上面两个钩子一起再追加：
    - `virtual int GetInstanceCounter() const { return -1; }`，-1 表示这个类没有计数器。
    - `virtual void SetInstanceCounter(int) {}`
  - 每个带 `static int count` 的 mod 类都实现这两个函数，直接读写自己的 `count`。这包括 Basic 里的全部 mod，以及 `Forever_Mod/{Empty,Test,Wxdj}` 里的 mod。
  - **保存**：遍历所有存活的 mod 实例，按 `(概念, id)` 记下 `GetInstanceCounter()`，同一个 `(概念, id)` 有多个实例时取最大值。结果写进 manifest 的 `counters` 表。
  - **读档**：所有 mod 实例都 `Factory::Create` 完之后（创建过程本身会让计数器递增），对每个 `(概念, id)` 取任一实例调 `SetInstanceCounter(存档值)`，恢复成保存时的值。
  - 保存时一个实例都没有的类型（比如生成完就销毁的 Terrain/Roadnet mod）不用恢复：读档后也没有它们的恢复名字，不会撞车。
  - 没实现这两个函数的第三方 mod 会退回 -1，不恢复。万一撞名，由现有的 `Map::AddZone/AddBuilding` 重名检查兜底并打警告。这一点写进 `mod_state.md`。
- 宿主包装类（Building / Zone / Room / Component / Job / Organization / Scheduler / Vehicle / Route / Station / Asset / Script / Storage / Manufacture …）：
  - 存档时把 `mod->SaveState` 的结果放进记录的 `modState` 字段。
  - 读档时先用存档里的参数串 `Factory::Create`，再调 `LoadState`。
- 基类里的声明式公共字段（例如 `BuildingMod` 的 footprint、floors（含 buildShell）、singles、rows、cabins、doorSpecs、decorations）由 Core 在包装类里直接快照，因为 UE 运行时会懒读这些字段。mod 自己的私有数据才走 `SaveState`。
- Basic 里有状态的 mod 按需实现，例如 Job/Organization/Scheduler 的 `occupantName/plans`、`ManufactureMod::targets`。

### 4. Core 序列化：两遍，先建对象，再连引用
新增 `Core/common/save.{h,cpp,md}`（总编排）和 `SaveContext`（键 ↔ 对象指针表）。每个域给类加 `Save / LoadCreate / LoadLink`，实现放在各自的 `*_save.cpp` 里，配 `.md`，同步 vcxproj 和 filters。

**稳定键**
- Zone / Building / Door / Station / Vehicle：名字逐字存，加载时用 `SetName` 覆盖。
- Room：建筑名 + number。
- Component：(建筑名, 序号)。
- Citizen / SchoolClass / Organization：数组下标。
- Job：(组织下标, 序号)。
- Road：名字；小路用"所属 lot + 序号"。
- 顶层 Lot：下标。
- Node：整数 id。

**第一遍：按依赖顺序创建对象**
1. **头**：manifest；各计数器，包括宿主的 `Node::count`、`doorCount`、`doorNameSequence`、`vehicleCounter`、`stationCounter`；`Populace::currentYear`；Player 时钟。
2. **Map 格子**：宽高、地形调色板、Element（地形 / 高度 / 水 / hatches / **fineHeights**）、`terrainTextures`。
3. **路网**：Node → Connection → Road（含小路及其 openings）→ Intersection → 顶层 Lot；Junction；车行图和步行图（步行图里共享的边要去重）；`navAnchorNodes`；`throughLines`。这些都整块转储，不尝试重建。
4. **Zone / Building**：
   - 创建 mod 并填入快照，加上 `direction` 和 `decorations`。
   - 给 `Building::Layout` 加"恢复模式"重载：不调 `mod->Layout`，按 floors 里的模板名 + 朝向 + 包围盒重新实例化 Floor / Component / Room，并显式带入 Component/Room 的顺序和 Room 导航节点 id。`singles/rows` 是 `unordered_map`，顺序必须单独存。
   - Door 全量转储，包括 `flippedSide`、`access`、`allowedCitizenNames`、`options`、脚本状态。
5. **植被**：`TreeInstance` 列表和 mesh 路径表。
6. **Populace**：Citizen 全量、SchoolClass、Relation（二进制段）。**Society**：Organization 和 Job。**Traffic**：Station/Route 的外壳和 Vehicle。另外还有 Asset 树和 Industry（目前为空，先留出 schema）。

**第二遍：连接引用**
- Lot / Zone / Building / Room 的拥有者和租户。
- Citizen 的 spouse / children / room / job / scheduler / experiences。
- Society 内部的链接。
- Traffic：Map 恢复后重新 `Route::Build`，再绑定车辆和房间。
- Script：先 `ReadMilestones`，再覆盖每个节点的 `premise` 以及 `actives/variables`。
- 定时器集合（`jobTimerSet` / `schedulerTimerSet` / `organizationTimerSet`），按下标 + 节点名存。
- `Traffic::elapsedSeconds/lastTickTime`：公交车位置由它们算出来。

**不需要重建的 mod**：Terrain / Roadnet / Name / Product 这类只在生成时用的 mod，加载后不重建。

### 5. UE 层
- **新增 `Mod/ForeverSaveSubsystem`**（`UGameInstanceSubsystem`）：
  - 提供 `SaveToFile`、`RequestLoad(file)` 和 `pendingLoad`。
  - 负责读写文件、压缩解压、校验、报错。
  - 封装保存和打开两种文件对话框，过滤器 `Forever Save (*.fsav)|*.fsav`。
  - 把 `GetDialogParentWindowHandle()` 抽成共享函数。
- **新增 `UI/ForeverPausePanelWidget`**：
  - `BindWidget` 的 `SaveButton` / `LoadButton`。
  - 打开/关闭的输入模式切换套用 `MapWidget` 的写法。
  - 在 `NativeOnKeyDown` 里处理 Tab，因为 UI-only 输入模式下 Enhanced Input 不会触发。
  - `SetIsFocusable(true)`。
- **`ForeverPlayerController`**：
  - 新增 `pauseWidgetClass/pauseWidget` 和 `TogglePause()`。
  - `GetBindingDefinitions` 加 `{TogglePause, EKeys::Tab}`，`KeyBindings.ini` 加 `TogglePause=Tab`。
  - 删掉 `MAINCONTROLLER_TODO.md` 和 `ForeverPlayerController.md` 里关于 Tab 的待办。
- **暂停**：用全局时间缩放（打开设 0，关闭设 1）。`bFrozenForLod`（`ForeverBuildingFrameworkComponent.cpp:138-146`）和谜题也用时间缩放 0、结束时改回 1，所以时间缩放已经是 0 时不允许打开暂停界面，避免互相解除。打开暂停界面前，先确认地图、手机、背包、谜题都没开着。
- **`ForeverConfigMenuWidget`**：新增 `LoadGameButton`。点击后弹出打开对话框，交给 SaveSubsystem 校验；失败时写进现有的 `FailureMessage`；成功时走 `EnsureModsRegistered` + `OpenLevel(World)`。
- **游戏内加载**：校验通过后设置 `pendingLoad`，再 `OpenLevel(World)` 重新进入，和菜单加载走同一条路径。
- **`AForeverFrameworkActor`**：
  - 有 `pendingLoad` 时，各个 `Ensure*Generated` 改走加载。
  - 跳过 `BroadcastGameStart`。
  - `GenerateTerrain / Roadnet / Zones / Buildings / Vegetation / Vehicles` 照常运行，读的是恢复后的 Core 数据。
- **保存前在 UE 侧同步状态**：
  - 把活动市民 Actor 的位置写回 `Citizen::SetPosition`，正在走路的落到 `walkDestination`。
  - 调 `WaitForPendingPathfinding()`。
  - 有对话队列在处理、或者谜题进行中时，拒绝保存并给出提示。
- **`ue` 段**：玩家 Pawn 变换、当前占有的对象（市民下标或车辆名）、`UForeverWeaponComponent` 的当前弹匣和装备。
- **`ForeverCharacter::PossessedBy`**：改成只在 Player 的槽位为空时才初始化武器和背包，否则存进去的背包永远会被覆盖。
- **加载后**：重新计算 `Room::occupants` 和 `Player::currentRoom`（生成时已经在体积内的 Actor 会不会重新触发 Overlap，需要在 PIE 里验证）。门全部恢复为关闭，不存门的动画状态。

**你需要在编辑器里做的事**
- 新建 `WBP_PausePanel`，父类 `UForeverPausePanelWidget`，里面放名为 `SaveButton` 和 `LoadButton` 的按钮。
- 给 `WBP_ConfigMenu` 加一个名为 `LoadGameButton` 的按钮。
- 在 `BP_ForeverPlayerController` 里指定 `pauseWidgetClass`。

### 6. 分阶段交付
加载路径是"用存档结果替换生成"。中间阶段，**还没序列化的域**允许临时回退到重新生成，用开发开关控制，最后一个阶段移除这个开关。每个阶段结束我都会在本地完整编译：Framework.sln → UBT ForeverEditor → 三个外部 sln。然后由你在 PIE 里验证。
- **P0 基础设施**（已完成，用户 PIE 验证通过）：`save_archive`（含文件版本和段表版本）、`mod_state`（21 个基类的 `SaveState/LoadState`、`GetStateVersion` 和计数器接口，Basic 与外部 mod 实现计数器接口，并重编外部 mod）、`ForeverSaveSubsystem`、文件对话框、`.fsav` 外壳、manifest、mod 和资源检查、报错显示。
- **P1 UI**（已完成，2026-10-18 编译通过）：Tab 暂停界面、开始菜单的加载按钮、时间缩放暂停、修正 `PossessedBy`。
- **P2+P3 地图与建筑（合并，已完成，用户 PIE 验证通过）**：格子（含细高度）、道路、节点、导航图、`throughLines`、植被，以及 Zone / Building（含 direction 和 decorations）/ Floor / Room / Component / Door。
  - **为什么合并**：生成建筑时会改动路网和地形，包括开出入口、切割 Lot、断开贯通线、挖洞。存档时这些改动已经做过，所以只恢复地图再重新生成建筑会叠加两次。地图和建筑必须一起按存档恢复。
  - **实现**：`Core/map/map_save.*`（`MapSaveCodec`），对象字段原样写出。Floor 的全部内容直接存，不再需要 `Building::Layout` 恢复模式。
  - **mod 公共字段改用快照**：UE 运行时要读的 mod 公共字段改由包装类在生成时拷贝快照，包括 `Building::GetFloorSpec/GetCabins/GetStationMod`、`Zone::GetWalls/GetGates`、Room 属性。读档时 mod 是重新创建的空实例。
  - **门的 options**：已经写进存档，但等 P5 跳过 `game_start` 广播后再恢复。
- **P4 人口与社会（已完成，用户 PIE 验证通过）**：Citizen、关系、Job、Organization、定时器。
  - **实现**：`Core/populace/populace_save.*`、`Core/society/society_save.*`；跨域引用编码在 `Core/common/save_refs.h`。
  - **归属关系**：Zone/Building/Room 的 owner、tenants、occupants 存在人口段里。
  - **mod 名字**：Job 和组织脚本里的 `$$self.name` 改回存档时的名字。
  - **可以缺省**：两段都是 `required=false`，P2 的存档读档时这两个域重新生成。
  - **暂不恢复**：市民的 options 已经写进存档，但等 P5 再恢复。
- **P5 交通与剧情（已完成，用户 PIE 验证通过）**：Station / Route / Vehicle、Script 状态、Story。
  - **交通**：`Core/traffic/traffic_save.*`。线路存 `Build` 的结果（站点表、连线、停靠顺序），连线几何读档时用 `BuildEdgeGeometry` 重算。
  - **脚本状态**：`Core/story/script_save.*`，按持有者稳定键覆盖。
  - **读档跳过 game_start 广播**：门、市民、车辆的交互选项按存档恢复。
  - **玩家操控对象从 P6 提前到这里**：存在 `ue` 段，读档后重新操控存档时的市民，开车时再上车。不提前的话，跳过 game_start 后玩家会停在默认相机上。
- **P6 玩家与 UE 状态（已完成，用户 PIE 验证通过）**：时钟、生命、Asset 树、占有对象、武器、保存前同步、拒绝保存的条件，并移除临时回退。
  - **玩家段**：`Core/player/player_save.*`，存时钟、day、时间流速、生命值、5 个身体槽位、房产、所在房间，以及房间里的物品。
  - **ue 段**：新增操控对象的位置朝向、视角、激活的肩膀武器、弹匣余量。
  - **保存前同步**：先 `WaitForPendingPathfinding`，再把市民 Actor 的位置写回 Core（`SyncPositionsToCore`）。
  - **拒绝保存**：对话或谜题进行中时拒绝保存。
  - **车辆位置**：读档后按 `Vehicle` 记录的位置生成，不再放回车位。
  - **临时回退**：缺段时重新生成保留为旧存档的兼容方式。新存档包含全部段，不会走回退。

## 关键文件
- **新增**：
  - `Dependence/common/{save_archive,mod_state}.*`
  - `Core/common/save.*`，以及各域的 `*_save.cpp`
  - `Forever/Mod/ForeverSaveSubsystem.*`
  - `Forever/UI/ForeverPausePanelWidget.*`
  - 每个新文件都配 `.md`
- **修改**：
  - Core：`Core/common/config.*`；`Core/map/{map,building,zone,room,door,roadnet,vegetation}.*`；`Core/populace/*`；`Core/society/*`；`Core/traffic/*`；`Core/story/{script,milestone}.*`；`Core/player/*`
  - Dependence：21 个 `Dependence/*/*_mod.h`
  - UE：`Forever/Framework/ForeverFrameworkActor.cpp`；`Forever/Player/{ForeverPlayerController,ForeverCharacter}.*`；`Forever/Input/ForeverKeyBindingSubsystem.cpp`；`Forever/UI/ForeverConfigMenuWidget.*`；`Forever/Mod/ForeverConfigBridgeSubsystem.cpp`
  - 配置：`Resource/Config/KeyBindings.ini`
  - 其他：Basic 和 `Forever_Mod/*` 里受影响的 mod；所有 vcxproj 和 filters

## 风险与待办（写进各 `.md`）
- **性能和体积**：导航图、`throughLines`、关系、植被是大头，P2 和 P4 阶段要用日志实测数量。
- **Room 顺序**：`Building::Layout` 恢复模式依赖 Component/Room 的顺序，必须显式存。
- **名字冲突**：mod 的静态命名计数器会随存档保存并恢复（见第 3 节），读档后新建的实例从保存时的计数接着往下编号。没实现计数器接口的第三方 mod 不恢复，撞名时由重名检查兜底。
- **兼容性**：四层版本号（见 1.5 节）。存档存的是结果，所以只改生成算法不用升版本；改存档里任何一段的数据布局时，必须升该段的版本并补一个迁移函数，这条写进 `save.md`。
- **不存**：手机 / App / Puzzle 画布、门动画、行驶中的车速、UE 侧运行时增删的树（目前没有调用方）。
- **后续可选**：DLL 版本/哈希校验、游戏内中途加载 DLL、存档缩略图和元数据头。

## 验证
1. **我在本地完整编译**：Framework.sln（MSBuild）→ UBT ForeverEditor → `Forever_Mod/{Empty,Test,Wxdj}`。编辑器开着时 Live Coding 会挡住编译，我会提示你关掉。
2. **往返一致性测试**：新增控制台命令 `Forever.SaveRoundTrip`，流程是保存 → 加载 → 再保存，比较两份未压缩的 manifest 和各段哈希，应当完全一致。这是不需要手动玩就能抓到遗漏字段的主要手段。
3. **PIE 手动测试**：
   - 按 Tab 保存，换个位置、换个占有对象，再加载。确认玩家位置、时间、背包、市民、门、建筑装饰、行道树和园区树、河岸细高度都一致。
4. **缺 mod 测试**：移走存档用到的某个 mod DLL。菜单加载和游戏内加载都应拒绝，并列出缺失的 `(概念, id)`。
5. **缺资源测试**：移走某个 `.script` 或 `.layout`。加载应拒绝，并列出缺失的名字。
6. **可移植性测试**：把存档拷到另一个文件夹，或另一台机器上加载。检查存档里没有盘符，也没有 `Resource/`、`Forever_Mod/` 之类的路径。
