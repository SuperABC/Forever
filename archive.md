# 存档/读档功能方案

## Context

Forever 要往"高自由度 GTA"方向做，存档是第一块基础设施。需求六条：Tab 暂停界面（保存/加载两个按钮，弹文件选择窗口）；存档是**单个文件**；开始菜单也能加载；**存档里不出现任何文件路径**（换电脑、换文件夹都能用）；加载时检查存档用到的 mod 是否都在，缺 DLL 就拒绝并报错；每个 mod 可以有专属序列化函数，把自己内部数据塞进存档。

调研得出的约束（已读代码确认）：
- 世界生成**完全不可复现**：`Dependence/common/utility.cpp` 的 `GetRandom` 每次都用 `random_device` 新建引擎，没有种子；生成中还依赖 `unordered_map` 遍历顺序。已和用户确认：**存完整生成结果**，加载时重建，不重新生成。
- 状态几乎都在 Core（纯 C++，不能出现 UE 类型）；UE 的 Actor 大多是 Core 数据的投影，唯一要单独收集的是玩家位置/被占有对象、武器弹药、行走中的市民位置。
- 仓库自带 JSON 库 `Dependence/common/json.h`（`JsonValue`/`JsonReader`/`FastWriter`），Core/Basic/Mod 都已在用。
- 文件对话框已有：`UForeverConfigBridgeSubsystem::SelectFile/SelectFolder`，`Forever.Build.cs` 已依赖 `DesktopPlatform`，缺一个"保存文件"变体。
- `PausePanel` 只存在于注释里；`Tab` 键未被占用（`Esc` 绑了 `CloseMap`）。
- 界面按项目惯例是"C++ 基类 + 蓝图布局（`BindWidget`）"。已确认：我写 C++ 基类，用户在编辑器里建蓝图。
- Mod 没有公共基类、没有版本/哈希概念；`liveInstances` 是 Factory 私有；Mod 实例名来自 DLL 内静态计数器（跨局不重置），**名字不是稳定键**。
- 约束来自 `CONVENTIONS.md`：跨 DLL 的方法必须 `virtual`、不能返回容器、分配释放要在同一侧；改 `Dependence` 头文件要重编 `Forever_Mod/{Empty,Test,Wxdj}`；每个 `.h/.cpp` 配 `.md`、vcxproj 同步；改动必须真实编译（我在 Linux 容器里编译不了 UE/MSBuild，需要用户编译后反馈）。

## 总体设计

### 1. 存档文件格式（单文件，无路径）

外层（UE 层负责压缩）：`magic("FVSV")` + 格式版本 + 未压缩长度 + zlib 压缩体（UE `FCompression`，Core 不依赖压缩库）。

压缩体 = Core 产出的"分段容器"（新增 `Dependence/common/save_archive.{h,cpp,md}`）：
- 段表 + 若干命名段。小结构（记录、manifest、mod 快照、脚本状态）用 `JsonValue`；大数组（地图格子、导航图、行道树、人际关系）用二进制段，避免 JSON 文本膨胀（地图约 10^6 格、导航图约 10^5–10^6 项、关系约 100×市民数）。
- 段：`manifest`、`world.map.*`、`world.populace`、`world.society`、`world.traffic`、`world.story`、`player`、`ue`。

**不存文件系统路径的做法**：
- 脚本和布局都只存**名字**，加载时和初始化时完全一样的方式解析：脚本用 `Config::GetScriptPath(裸名)`，布局按 `Config::GetLayouts()`（扫描 `resource_paths`/`layout_paths` 得到的 `.layout` 文件）里的文件名（不含扩展名，即模板名，如 `preset_single_room_fg+`）匹配。两者都在配置的资源根目录里查找，所以存档换电脑、换文件夹都能用，只要那台机器的 `config.json` 里配置了含这些文件的资源目录。
- 存档里**不放**绝对/相对路径，也**不嵌入**脚本/布局文件内容（用户已确认走"按名字到资源目录里找"的方式）。
- config.json 里的 `dll_paths/layout_paths/resource_paths` 一律不进存档，只存 mod 的 `(概念, id, 参数串)`。
- 存档里会出现的 `/Game/...` 之类**引擎内虚拟资产路径**（地面材质、建筑网格、车辆蓝图等，由 mod 提供）按原样存；它们不是文件系统路径，只要两台机器内容一致就可移植，由 mod 检查间接保证。

### 2. Mod 与资源检查（第 5 条 + 资源检查）

manifest 记录：存档用到的每个 `(conceptKey, modId, 参数串)`（按概念+id，**不能只用 id**，`product_wheat`、`weapon_pistol` 在 Product/Asset、Asset/Weapon 里重名）、各 mod 的依赖对（`GetModDependencies`）、**用到的脚本裸名列表**（主线脚本名 + 所有 Door/Job/Organization/Scheduler/Vehicle/Story 的 `scriptModName` 与 `milestoneNames`，即 `Script::loadedMilestones` 汇总）、**用到的布局模板名列表**（所有已存建筑 `FloorLayoutSpec.templateName` 去重）、格式版本、生成版本号。

加载前（菜单和游戏内都在 `OpenLevel` 之前）校验：
1. 用 `Config::GetModEnables()` / `dllConceptIds` 判断每个 `(概念, id)` 是否被已发现的 DLL 提供；缺任何一个 → 拒绝，报错列出"缺失的概念/mod id"（沿用 `ValidateAndStartGame` 的报错风格）。
2. 复用 `ValidateAndStartGame` 里现成的依赖校验。
3. **资源检查**：对 manifest 里每个脚本裸名调用 `Config::GetScriptPath`，对每个布局模板名在 `Config::GetLayouts()` 的文件名集合里查找；任何一个找不到 → 拒绝加载，报错列出缺失的脚本名/布局名和"请在配置的 resources 里补上"。检查逻辑放在 Core（新增 `Config::CheckSaveResources(...)` 之类的纯函数，返回缺失列表），UE 层只负责展示。
4. 通过后把存档里的 `<概念>_mods` 列表写入 `Config` 并调用 `Registry::ReloadModArgs()`，让当前启用的 mod 集合与存档一致。
5. 游戏内加载若发现需要"启动时没被发现的 DLL"：`Registry` 只在第一次 `Get()` 时注册 DLL 且不重载，无法中途补加，直接拒绝并提示重启（这是现状限制，在 `.md` 记录）。
- 不引入 DLL 哈希/版本号（会让同 mod 不同构建的存档全部拒读，过严）；作为后续可选项记入待办。

### 3. Mod 专属序列化钩子（第 6 条）

新增 `Dependence/common/mod_state.{h,cpp,md}`：
- 宿主实现、Mod 调用的抽象类 `StateWriter` / `StateReader`（全部 `virtual`，键值式：`SetInt/SetDouble/SetBool/SetString/SetBytes`、`BeginObject/EndObject/BeginArray`；Reader 对应 `Get*`，字符串以 `const char*`（指向宿主内存）返回，mod 自己拷贝）。因为实现编译在宿主侧，分配/释放不会跨 DLL。
- 在 21 个 `<Concept>Mod` 基类**末尾**追加：`virtual void SaveState(StateWriter&) const {}` 和 `virtual void LoadState(StateReader&) {}`（默认空实现，不返回容器）。需重编 `Source/Basic` 和三个外部 mod 工程（CONVENTIONS §9）。
- 宿主侧每个包装对象（Building/Zone/Room/Component/Job/Organization/Scheduler/Vehicle/Route/Station/Asset/Script/Storage/Manufacture…）序列化时，把 `mod->SaveState` 的结果放进记录的 `modState` 字段；加载时先 `Factory::Create<X>(id)` 用存档里的参数串创建，再 `LoadState`。
- 基类里的"声明式公共字段"（如 `BuildingMod` 的 `footprint/floors/singles/rows/cabins/doorSpecs/decorations/trees`）由 Core 在包装类里直接快照，因为 UE 层会在运行时懒读（`BuildingElement.cpp` 读 `floors`/`cabins`，`Zone::GetWalls()` 转发 `mod->walls`，`Room::IsResidential()` 转发 `RoomMod`）。mod 自己的私有数据才走 `SaveState`。
- Basic 里有状态的 mod（`JobMod/OrganizationMod/SchedulerMod` 的 `occupantName/plans`、`ManufactureMod::targets` 等）按需实现；无状态的保持空。

### 4. Core 序列化（两遍：先建对象，再连引用）

新增 `Core/common/save.{h,cpp,md}`（编排器）和 `SaveContext`（键 ↔ 对象指针表）。每个域给自己的类加 `Save(...) const` / `LoadCreate(...)` / `LoadLink(...)`，实现放在各自的 `*_save.cpp`，配 `.md`，同步 vcxproj 和 filters。

稳定键：
- 名字类对象（Zone/Building/Door/Station/Vehicle）：原名**逐字存**，加载时通过包装类的 `SetName` 覆盖（mod 的 `GetName()` 计数器不可靠）。
- Room：`建筑名+number`；Component：`(建筑名, 序号)`；Citizen/SchoolClass/Organization：数组下标（Citizen 名不保证唯一）；Job：`(组织下标, 序号)`；Road：名字，path road 用"所属 lot + 序号"；顶层 Lot：下标；Node：整数 id（需 `Node::SetCount(maxId+1)`，Core.lib 和 Basic.dll 里各有一份 `Node::count`，都要重新同步）。

创建顺序（依赖序）：
1. 头：版本、manifest、各计数器（`Node::count`、`doorCount`、`doorNameSequence`、`vehicleCounter`、`stationCounter`）、`Populace::currentYear`、`Player` 时钟。
2. Map 格子：宽高、地形调色板、`Element`（地形/高度/水）、细高度数组、hatches、`terrainTextures`（二进制段）。
3. Node → Connection → Road（含 path road）→ Intersection → 顶层 Lot；Junction、导航图（车行/步行，步行图边共享需去重）、`navAnchorNodes`、`throughLines`（整块转储，**不**尝试重建：依赖 `unordered_map` 遍历序和节点 id 分配序）。
4. Zone / Building：创建 mod、填快照、**跳过 `mod->Layout` 的随机决定**（需给 `Building::Layout` 加"恢复模式"重载，从 `floors` 模板名+朝向+包围盒重新实例化 `Floor`/Component/Room，并显式带入 Component/Room 顺序和 Room 导航节点 id）；Door 全量转储（含 `flippedSide`、`access`、`allowedCitizenNames`、`options`、脚本状态）；行道树。
5. Populace（Citizen 全量、SchoolClass、Relation 二进制段）、Society（Organization+Job）、Traffic（Station/Route 壳 + Vehicle）、Asset 树、Industry（现在为空，留 schema）。
6. 第二遍连引用：Lot/Zone/Building/Room 的拥有者与租户；Citizen 的 `spouse/children/room/job/scheduler` 与 `experiences`；Society 链接；Traffic 在 Map 恢复后重新 `Route::Build` 并绑定车辆与房间；Script 先 `ReadMilestones` 再覆盖 `premise/actives/variables`（给 `Script` 新增 `loadedMilestones` 记录加载过的裸名）；定时器集合（`jobTimerSet/schedulerTimerSet/organizationTimerSet`，按市民/组织下标+节点名存）；`Traffic::elapsedSeconds/lastTickTime`（公交位置是它的纯函数）。

Terrain / Roadnet / Name / Product 等"只在生成时用"的 mod 加载后不需要重建（Roadnet 数据已深拷贝进 `Roadnet`）；Room/Building/Zone/Component/Vehicle/Route/Station/Asset/Job/Org/Scheduler/Script 的 mod 实例必须重建。

### 5. UE 层

新增（`Source/Forever/...`，每个类配 `.md`）：
- `Mod/ForeverSaveSubsystem`（`UGameInstanceSubsystem`）：`SaveToFile`、`RequestLoad(file)`、`pendingLoad` 句柄、读文件/解压/校验/报错、`SaveFileDialog` 和 `OpenFileDialog` 封装。把 `ForeverConfigBridgeSubsystem.cpp` 里匿名命名空间的 `GetDialogParentWindowHandle()` 抽成共享函数。扩展名 `.fsav`，对话框过滤 `Forever Save (*.fsav)|*.fsav`。`Forever.Build.cs` 无需改动。
- `UI/ForeverPausePanelWidget`：`BindWidget` 的 `SaveButton`、`LoadButton`；套用 `MapWidget`/`InventoryWidget` 的打开/关闭输入模式模板；`NativeOnKeyDown` 处理 Tab（UI-only 输入模式下 Enhanced Input 不触发）；`SetIsFocusable(true)`。
- `ForeverPlayerController`：加 `pauseWidgetClass/pauseWidget`、`TogglePause()`、`BeginPlay` 创建、`SetupInputComponent` 绑 `TogglePause`；`ForeverKeyBindingSubsystem::GetBindingDefinitions()` 加 `{TogglePause, EKeys::Tab}`，`KeyBindings.ini` 加 `TogglePause=Tab`；删 `MAINCONTROLLER_TODO.md` 和 `ForeverPlayerController.md` 里的 Tab 待办。
- 暂停机制用 `UGameplayStatics::SetGamePaused`，**不用** `SetGlobalTimeDilation(0)`（会被 `ForeverBuildingFrameworkComponent::TickComponent` 的 `bFrozenForLod` 和 `PuzzleWidget::EndPuzzle` 改回 1）。打开暂停前先确认地图/手机/背包/谜题没开。
- `ForeverConfigMenuWidget`：加 `BindWidget UButton* LoadGameButton` + `HandleLoadClicked`（弹打开对话框 → `ForeverSaveSubsystem` 校验 → 失败写进已有的 `FailureMessage` → 成功走和 `ValidateAndStartGame` 同样的 `EnsureModsRegistered` + `OpenLevel(World)`）。
- 游戏内加载：校验通过后设置 `pendingLoad`，`OpenLevel(World)` 重新进入（与菜单加载走同一条路径，不做原地热替换）。

**用户在编辑器里要做的事**：新建 `WBP_PausePanel`（父类 `UForeverPausePanelWidget`，含名为 `SaveButton`/`LoadButton` 的按钮）；给 `WBP_ConfigMenu` 加名为 `LoadGameButton` 的按钮；在 `BP_ForeverPlayerController` 里指定 `pauseWidgetClass`。

`AForeverFrameworkActor` 改动：`BeginPlay` 里若存在 `pendingLoad`，各 `Ensure*Generated` 改走加载；**跳过** `BroadcastGameStart`（其 `game_start` 会触发 `ChangeControlChange` 随机占有一个市民，见 `ForeverGameMode.cpp`）；`Registry::ReloadModArgs()` 已在 `BeginPlay` 开头。`trafficFramework->GenerateVehicles` 等在加载后照常运行。

保存前的 UE 侧"冲刷"：把所有 `activeInstances` 市民 Actor 的位置写回 `Citizen::SetPosition`（行走中的路径终点按 `walkDestination` 落位）、`UForeverPopulaceFrameworkComponent::WaitForPendingPathfinding()`、拒绝在对话队列（`pendingQueue`）或谜题进行中保存（给出提示）。

额外存 `ue` 段：玩家 Pawn 变换；被占有对象（市民下标或车辆名）；`UForeverWeaponComponent` 的当前弹匣与装备武器。

`ForeverCharacter::PossessedBy`（`ForeverCharacter.cpp:176-201`）目前每次占有都删掉并重建肩上武器和背包、重新塞 120 发弹药 → 改成"仅当 `Player` 的槽位为空时才初始化"，否则背包永远存不住。

加载后需重算 `Room::occupants`/`Player::currentRoom`（Overlap-Begin 对"生成时已在体积内"的 Actor 是否重发需在 PIE 里验证）。门的 UE 动画状态不存，加载后全部关闭。

### 6. 分阶段交付（每阶段结束由用户编译验证）

加载路径是"替换生成"，中间阶段允许对**尚未序列化的域**临时回退到重新生成，用开发开关控制；最终阶段移除。
- **P0 基础设施**：`save_archive`、`mod_state`（含 21 个 Mod 基类的钩子，重编外部 mod）、`ForeverSaveSubsystem`、文件对话框、`.fsav` 外壳（压缩）、manifest 写入与 mod 检查、报错信息。
- **P1 UI**：Tab 暂停界面、开始菜单加载按钮、`SetGamePaused`、`PossessedBy` 修正。
- **P2 Map**：格子/道路/节点/导航图/行道树。
- **P3 建筑**：Zone/Building/Room/Component/Door + `Building::Layout` 恢复模式。
- **P4 人口与社会**：Citizen/关系/Job/Org/定时器。
- **P5 交通与剧情**：Station/Route/Vehicle、Script 状态、Story。
- **P6 玩家与 UE 状态**：时钟/生命/Asset 树、占有对象、武器、冲刷与拒绝保存条件、移除临时回退。

## 关键文件

- 新增：`Dependence/common/{save_archive,mod_state}.*`；`Core/common/save.*`；各域 `*_save.cpp`；`Forever/Mod/ForeverSaveSubsystem.*`；`Forever/UI/ForeverPausePanelWidget.*`；每个文件配 `.md`。
- 修改：`Core/common/config.{h,cpp}`（加资源检查）；`Dependence/*/*_mod.h`（21 个，追加两个虚函数）；`Core/map/{map,building,zone,room,door,roadnet}.*`；`Core/populace/*`；`Core/society/*`；`Core/traffic/*`；`Core/story/script.*`；`Core/player/*`；`Forever/Framework/ForeverFrameworkActor.cpp`；`Forever/Player/{ForeverPlayerController,ForeverCharacter}.*`；`Forever/Input/ForeverKeyBindingSubsystem.cpp`；`Forever/UI/ForeverConfigMenuWidget.*`；`Forever/Mod/ForeverConfigBridgeSubsystem.cpp`；`Resource/Config/KeyBindings.ini`；`Basic` 和 `Forever_Mod/*` 里受影响的 mod；所有 vcxproj/filters。
- 复用：`Dependence/common/json.h`（`JsonValue`/`JsonReader`）；`UForeverConfigBridgeSubsystem::SelectFile`、`GetDialogParentWindowHandle`；`ValidateAndStartGame` 的依赖校验与启动序列；`Config::GetModEnables/IsModIdEnabled/GetConceptMods/WriteConfig`；`Map::LocateBuilding/LocateRoom/FindDoorByName`、`Room::GetAddress`；`Node::SetCount`；`Time::ToString`；`UForeverPopulaceFrameworkComponent::WaitForPendingPathfinding`；`MapWidget` 的开关模板。

## 风险与待办（写入各 `.md`）

- 加载性能与体积：导航图和关系是大头，需在 P2/P4 用日志计数实测（目前只是估算），必要时改二进制紧凑编码。
- `Building::Layout` 恢复模式依赖 Component/Room 顺序；`singles/rows` 是 `unordered_map` 遍历序，必须显式存顺序。
- 名字覆盖：确认 Core 里只用包装类缓存的名字而非每次调 `mod->GetName()`；加载后若再创建同类新实例，名字可能与恢复的撞车（目前只有 Traffic 有自己的计数器）。
- 存档格式带版本号；生成代码改动后旧档仍可读（因为存的是结果），格式不兼容改动时才需要迁移。
- 不存：手机/App/Puzzle 的画布状态（UI 瞬时状态）、门的动画状态、行驶中车辆速度。
- 后续可选：DLL 版本/哈希校验、游戏内中途补加载 DLL、存档缩略图/元数据头（现在无法在不解压全文件的情况下预览）。

## 验证

我在 Linux 容器里**无法编译 UE/MSBuild**，验证分两部分：
1. 我这边：对纯 C++ 层（Dependence/Core/Basic）新增文件尽量用 `g++ -fsyntax-only` 做语法检查（若 `LoadLibrary` 等 Windows 依赖挡住就只检查新文件）；逐项对照 CONVENTIONS（`.md` 配对、include 顺序、vcxproj/filters 成对、跨 DLL 虚函数）。
2. 用户这边（每阶段）：
   - 按 CONVENTIONS §10 编译：`Framework.sln`（MSBuild）→ UBT `ForeverEditor Win64 Development` → 重编 `Forever_Mod/{Empty,Test,Wxdj}` 三个工程。
   - 往返一致性测试：加一个控制台命令 `Forever.SaveRoundTrip`——保存 → 加载 → 再保存，比较两份未压缩 manifest 与各段的哈希，应当一致（这是不用人工玩就能抓到遗漏字段的主要手段）。
   - PIE 手动：进游戏按 Tab → 保存 → 换位置/换占有对象 → 加载，确认玩家位置、时间、背包、市民、门状态一致。
   - 缺 mod 测试：把存档用到的某个 mod 的 DLL 移走，菜单加载和游戏内加载都应拒绝并列出缺失的 `(概念, id)`。
   - 缺资源测试：把存档用到的某个 `.script` 或 `.layout` 从配置的 resources 目录里移走（或去掉该资源目录），加载应拒绝并列出缺失的脚本名/布局名。
   - 可移植性测试：把存档拷到另一台机器/另一个文件夹加载；用文本编辑器/`strings` 检查存档里没有盘符或 `Resource/`、`Forever_Mod/` 之类路径。
   - 把编译报错或日志贴给我，我来修。
