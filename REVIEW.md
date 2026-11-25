# 全项目 Review（2026-11-25）

初版基于提交 `7731158`；第二轮修复、第三轮（ModTest 拆分）、第四轮（引用检查）和第五轮（A7–A11）都在 `29b82f6`（Review whole project）之上，尚未提交。**已修的条目已经从清单里删掉**，下面 A–G 只剩还没处理的（`[ ]`）；修了什么见各轮小结。

**怎么读这份清单**

- 编号：A=Core 感知具体 mod，B=UE 感知具体 mod，C=bug，D=设计问题，E=违反 `CONVENTIONS.md`，F=文档/注释遗留，G=现有测试失败。编号和初版保持一致，所以有空缺：A6–A11、B8、C1、C2、C3、C5、C6、C10、D10 和整个 E 节（E1–E4）已修并删掉，文中别处提到这些编号时指的就是它们。
- 可信度：
  - **已确认**：读过对应代码，行号核对过。
  - **潜伏**：代码确实有问题，但当前没有调用路径会触发。
  - **推测**：有依据但没有实际复现/回退验证。
- 行号对应第五轮结束时的工作区。

**覆盖范围**

全项目约 12.5 万行代码、384 份 md。

- mod 感知（A、B）：对 Core/Dependence/Forever 三层的字符串字面量和标识符做了全量扫描，再逐处读上下文核对，这部分比较全。
- 找 bug（C）：重点精读 + 模式扫描，不是逐行通读。精读过的：`Core/common` 全部、`traffic.cpp`/`vehicle.cpp`、`room.cpp`、`economy.cpp`、`market.cpp`、`election.cpp`、`script.cpp` 的非词汇表部分、`map.cpp` 的前 1200 行和寻路部分、`ForeverFrameworkActor.cpp` 的前 1200 行、`ForeverModSubsystem.cpp`。Basic 各 mod、大部分 UI、存档编解码、`building.cpp`、`route.cpp` 主体只做了扫描，没有精读。

---

## 第五轮：A7–A11

按你的 5 点指示改的，条目已从 A 节删掉。

| 条目 | 做了什么 |
|---|---|
| A7 主线脚本兜底 | 删掉 `Config::GetMainStoryScriptPath()`。没配 `story_scripts` 就是没有主线脚本，存档清单和 UE 的依赖校验也不再兜底 |
| A8 平原材质 | Core 里 `plain` 不再声明材质（`defined=false`），三个 UE 资产路径从 Core 删掉。UE 的 `TerrainMaterialBuilder` 加了兜底：所有没声明材质的地形都用 `/Game/Asset/Terrain/Plain` 那套贴图 |
| A9 Megascans 路径 | `terrain_mod.h` 的 `MakeMegascansLayer` 删掉。Basic 没有现成的地形公共头，海滩、山、海、河四个 mod 各自写完整路径 |
| A10 `isConstruction` | 回调只表示"这格是平的"，不保证在 z=0，高度用已有的 `getHeight` 查。虚函数签名没变。棋盘格和井字路网自己加了 z=0 判断；`Map::IsGroundConstruction` 删掉 |
| A11 注释 | Core 里 17 处点名具体 mod 的注释换成中性描述 |

**行为上的变化**

- A7：`config.json` 没有 `story_scripts` 时，以前会去读名为 `test` 的脚本，现在主线是空的。
- A8：没调 `SetMaterial` 的 mod 地形以前画成灰色并报 Error，现在用平原的贴图、只打一条 Log。
- A10：第三方路网 mod 如果依赖"`isConstruction` 为真就一定在 z=0"，现在需要自己判断高度。`Forever_Mod` 下的四个 mod 都不用这个回调。

**没动的**

- A11 里"商店"这个词：`Economy` 自己有 `ShopItem` 这个概念，我只把明显当例子举的几处换成了"卖方""零售"，结构体名没改。
- `phone.h` 注释里的 `app_empty`：它描述的是代码里真实存在的特判（A4），代码没改，注释就没动。

**验证**

- `build.bat` 全量通过（Framework、UE 模块、四个外部 mod）。
- ModTest：按 `config.json` 1421 项全过，全量 1468 项全过。
- CoreTest quick：7232 通过、20 失败，失败的 20 项和之前逐条相同（G 节）。通过数没变，plateau 套件改过的那条检查仍然通过。
- 没有进 UE 编辑器验证。A8 的兜底材质需要进编辑器看一眼平原显示是否正常；A7 需要确认开局流程不受影响。

---

## 第四轮："引用了就必须声明依赖"——试过，已撤掉

**结论**

- 你提了"引用了某个 mod 就必须声明依赖"，我在 ModTest 里做成了硬性检查。
- 做完发现引用方向不等于依赖方向：楼用 `stationMod` 点名站点、房间用 `storageType`/`manufactureTypes` 点名仓库和工坊，但语义上是站点依赖楼、仓库和工坊依赖房间。
- 你决定去掉这项检查。现在 ModTest 不查依赖声明是否完整，谁依赖谁由作者按语义写。
- 依赖表里站点 → 楼、仓库/工坊 → 房间这 7 条我中途删过，已经原样恢复。

**我中途说错的一句**

- 我说过"启用了房间却没启用它点名的工坊，生成世界时会找不到 mod"。这是没查代码就下的结论，是错的。
- 实际上宿主缺了被托管的 mod，Core 只是跳过：[traffic.cpp:375](Forever_UE/Source/Core/traffic/traffic.cpp:375)、[industry.cpp:53](Forever_UE/Source/Core/industry/industry.cpp:53)。

**这一轮留下来的东西**

- **引用存在性检查变准了**（`referenced mods are provided by a loaded dll`）。
  - 原来只看少数几个字段，而且不管 Core 会不会真的用到。
  - 现在覆盖脚本、房间/组件、站点、仓库/工坊、武器/产品/弹药、载具、岗位、内部建筑、配方。
  - 园区和建筑的引用是在 `Layout` 里填的，ModTest 拿 8 块样例地块各调一次。
  - 脚本只在 Core 真的会建 Script 时才算。第三轮说 Wxdj 的建筑引用了 `script_empty`，其实它没有 `milestoneNames`，不算。
- **依赖成环检查**（`GetModDependencies has no cycle`）。成环的 mod 都启用后在配置界面里互相锁死，这条规则原来只写在 `basic.md` 里。这项是我自己加的，你没要求；不想要的话说一声就去掉。
- **Basic 依赖表补了 17 个 mod 的声明**（[basic.cpp](Forever_UE/Source/Basic/basic.cpp)）。这些是强制检查那一版跑出来的真漏项，按 `basic.md` 原有的标准本来就该有：
  - 会建脚本但没声明 `script_empty`：3 个住宅园区、`room_residence`、`organization_manufacture`、`asset_burger`、`asset_grenade`、5 个武器资产、5 个武器。禁用 `script_empty` 时它们会让 `Script` 构造抛异常。
  - `building_train_station` 用了占位房间槽，补了 `room_empty`。
- **默认 ScriptMod 的 id 收成了一个常量**：`Dependence/story/script_mod.h` 的 `kDefaultScriptModId`，Core 原来写死的三处 `"script_empty"` 改用它。A5 本身没有解决。

**验证**

- Framework 编译通过。这一步只动了 ModTest 和 `basic.cpp`；UE 模块和外部 mod 在上一步的全量 `build.bat` 里编过，之后没有再改它们依赖的头文件。
- ModTest 按 `config.json`：1421 项全过，核对了 137 处引用。
- ModTest 全量（5 个 dll、150 个 mod）：1468 项全过，核对了 142 处引用。
- 单测 Wxdj、不带 `--with`：3 项失败（依赖和引用的 `room_empty`、`component_empty` 没有 dll 提供），符合预期。
- 成环检查在强制检查那一版验证过：临时给 Yuanshen 加了一条互相依赖，ModTest 报出 `building_yuanshen -> script_yuanshen -> building_yuanshen`，已还原。
- CoreTest quick：7232 通过、20 失败，失败的 20 项和之前逐条相同（G 节）。
- 没有进 UE 编辑器验证。

**没有测试兜底的事**

- 依赖声明写得全不全。改 mod 时要对着 `basic.md` 的判定标准自己核对。
- 可以只启用农场房间而不启用农场工坊：房间照常生成但不生产，没有任何提示。

---

## 第三轮：ModTest 拆分和 mod id 唯一性

按你的两点指示做的：

**1. mod id 必须全局唯一，由测试保证**

- 规则写进了 `CONVENTIONS.md` 第 5 节：所有 mod 的 id 全局唯一，不管属于哪个 concept、由哪个 dll 提供；不支持不同 concept 共用一个 id。
- 所以 D10 不再是问题（只用 id 做键是成立的），已从清单里删掉。
- 两处检查：ModTest 对它加载的全部 dll 查重名；CoreTest 的 mods 套件对 `config.json` 的 `dll_paths` 再查一次。

**2. 把对 mod 的测试拆成 ModTest**

- 新工程 [Source/Test/Mod](Forever_UE/Source/Test/Mod/ModTest.md)，产出 `x64/Release/ModTest.exe`，已加进 `Framework.sln`。**只链接 `Dependence.lib`，include 路径里没有 Core**，mod 作者不需要 Core 和 UE。
- 从 CoreTest 搬过去的：`UnitTest`、漏包随机、实例名唯一、各 concept 的字段检查、房间家具布局检查。新加的：id 合法性（非空、不含空白）、id 重名、`GetModDependencies` 声明的依赖是否存在、dll 是否导出 `BindRandomHost`。
- CoreTest 的 mods 套件只留"mod 接进 Core 之后"的检查：id 全局唯一、`Registry` 认得每个 mod、计数器复位、`Config` 的 mod 参数。
- 为了让不依赖 Core 的 ModTest 能加载 mod，把 23 个 concept 的导出符号表从 `Core/common/loader.cpp` 挪到了 `Dependence/common/mod_concepts.h/.cpp`，Core 这边转 include，行为不变。
- `run_tests.bat` 的 `core` 管线现在先跑 ModTest 再跑 CoreTest。
- 用法：
  - mod 作者：`ModTest MyMod.dll --with Basic.dll --with Empty.dll`（`--with` 带上被引用/依赖的 mod 所在的 dll）。
  - 你这边全量：`ModTest x64\Release ..\Forever_Mod`（重名检查覆盖全部 mod）。

**验证**

- `build.bat` 全量通过（Framework 含 ModTest、UE 模块、四个外部 mod）。
- ModTest 实跑了五种情况，行为都符合预期：
  - 按 `config.json`：3 个 dll、146 个 mod，1252 项全过。
  - 全量（Basic + `Forever_Mod` 下全部）：5 个 dll、150 个 mod，1293 项全过。
  - 单测 Wxdj、不带 `--with`：报 3 项失败（依赖的 `room_empty`、`component_empty` 和引用的 `script_empty` 没有 dll 提供）。带上 Basic 和 Empty 后全过。
  - 把 `Wxdj.dll` 复制一份改名后一起测：两个 id 都报重名，并列出是哪两个 dll。
  - 给一个不是 mod 的 dll：报"不是 mod dll"。
- `run_tests.bat quick core`：ModTest 1252 项全过；CoreTest 7232 通过、20 失败，失败的 20 项和之前逐条相同（G 节）。通过数比上一轮少，是因为逐 mod 的检查搬去了 ModTest。

**顺手修的一个小问题**：`run_tests.bat` 用 `%DATE%` 截子串拼报告目录名，在这台机器的区域格式下会拼出 `周三022611_171703` 这种名字。改成从 PowerShell 取时间戳了。

**需要你知道的**

- **给作者的 SDK 还没有打包脚本**。我假定作者除了 Dependence 的头文件和 lib，还能拿到 `ModTest.exe` 以及 `Basic.dll`/`Empty.dll`（用来 `--with`）。如果你不打算发 Basic/Empty 的 dll，那"引用/依赖的 mod 必须存在"这类检查对作者就会报失败，需要另外定策略。
- **站点的一条检查变弱了**：原来是"不是公交站就必须有端口"，这要求测试认识 `"bus"`。现在只查"声明了端口就必须有 `stationType`"。
- **家具布局检查**：模型名在内置尺寸表（Basic 的 `room_furniture_catalog.h`）里的才查占地重叠；mod 自带的模型只查锚点在不在房间里，并记一条 note，不判失败。"大房间里家具太少"只对用了内置家具的 mod 才算问题。
- ModTest 不检查"字段里引用了某个 mod，但 `GetModDependencies` 没声明依赖它"。第四轮试过做成硬性要求，后来撤掉了，原因见上一节。（当时说 Wxdj 的建筑漏声明 `script_empty` 是我看错了：它没有 `milestoneNames`，Core 不会给它建脚本，不算引用。）
- `Dependence/common/random.h` 里有一句我第二轮写的注释（"两个 concept 注册了同名 id 会共用一条流"），内容仍然属实，这轮没再动它。

---

## 第二轮修复小结（按你的 5 点指示）

| 指示 | 做了什么 | 状态 |
|---|---|---|
| 1. E 板块全部修复 | E1–E4 全修。其中 include 分组实际扫出 55 个文件不合规，重排了 52 个 | 已修，已编译 |
| 2. mod id 格式只是弱约定 | 去掉了三处假定格式的代码（A6、`CheckModIdentity` 的前缀检查、CoreTest 的 `_empty` 后缀判断），改了 `CONVENTIONS.md` 和相关文档。见下面"mod id 格式"一节 | 已修，CoreTest 通过 |
| 3. C1/C2/C3/C5/C10 | 全修 | 已修，已编译；UE 里的实际表现没跑 |
| 4. B8 | 删掉了 `ForeverModSubsystem` 里重复的 Factory 校验，函数改名 `MountModContent` | 已修，已编译 |
| 5. 刷新本文件 | 本次 | — |

**验证**

- `build.bat` 全量通过：`Framework.sln`、UE 模块 `ForeverEditor`、四个外部 mod 均 0 错误。
- CoreTest quick（第二轮当时）：7940 通过，20 失败。失败的 20 项和修复前逐条相同（见 G 节，都是旧问题）；通过数多 1 是新加的计数器复位测试。presets、save、random 三个和这次改动直接相关的套件全过。
- **没有做的验证**：没有开编辑器/PIE。C1（公交切物理的初速度和停站时间）、C3（改脚本后重开 PIE 生效）、C5（第二次开局名字从头编）、C10（存档覆盖）在 UE 里的实际表现都没跑过，只有编译和 CoreTest 能覆盖的部分是验证过的。

### mod id 格式（指示 2）

规则：`<concept>_<name>` 只是弱约定，mod id 可以是任意字符串，任何实现都不能假定 id 的格式。已写进 `CONVENTIONS.md` 第 5 节。

改掉的三处格式假定：

- [map.cpp:512](Forever_UE/Source/Core/map/map.cpp:512)：`PlacePresets` 原来用 `rfind("zone_")` 判断预设是不是园区，改成 `zoneFactory.CheckRegistered(id)`（即 A6）。
- [mod_test.cpp](Forever_UE/Source/Dependence/common/mod_test.cpp)：`CheckModIdentity`（每个 mod 的通用单元测试）原来要求 type 以 `<concept>_` 开头，去掉了这条检查，只要求 type 和 name 非空。
- 测试里原来按 `_empty` 后缀识别占位模板，改成按提供它的 dll（`Empty.dll`）识别。这段代码第三轮随逐 mod 的检查搬到了 [ModTest 的 main.cpp](Forever_UE/Source/Test/Mod/main.cpp)。

没改、需要你知道的两类：

- **按完整 id 精确比较的特判**（`station_empty`、`route_empty`、`app_empty`、`script_empty`）不算"假定格式"，但属于感知具体 mod，仍在 A4、A5 里。
- **"id 跨 concept 不重名"**：第三轮按你的决定定成了硬规则，由测试保证（原 D10）。

---

## A. Core 感知具体 mod

### - [ ] A1 每日需求的服务类型写死 `"food"`/`"video"`（已确认）
- 位置：[economy.cpp:32-36](Forever_UE/Source/Core/society/economy.cpp:32)、309-311
- 问题：`kMealServiceType="food"`、`kVideoServiceType="video"`，以及视频需求的财富阈值 `kVideoWealthLow/High`（2000 元/5000 元）都是 Core 常量。"市民需要什么服务、需要多少"是具体 mod（餐厅/视频公司）的业务。
- 影响：换一套组织 mod（没有 food/video 服务）后，Core 仍然只会产生这两种需求；新增一种服务类型必须改 Core。
- 建议：需求由 mod 声明。代码里的注释已经写了"先写死，以后由 scheduler 自定义"——比如让 `SchedulerMod` 或一个新的需求声明接口返回"(服务类型, 份数, 预算规则)"列表，Core 只做撮合。

### - [ ] A2 `AssignVehicles` 写死载具种类 `"car"`/`"truck"`（已确认）
- 位置：[traffic.cpp:565-566](Forever_UE/Source/Core/traffic/traffic.cpp:565)、585-586；常量在 32-35 行
- 问题：成年市民 20% 概率分一辆 `"car"`，组织随机 1-5 次分 `"truck"`。种类字符串是 Basic 的 `vehicle_sedan`/`vehicle_truck` 注册的，Core 不该知道。
- 附带：成年年龄用自己的 `kAdultAge = 18`，而经济/选举用的是可调政策 `GovernmentPolicy::adultAge`，两处会不一致。
- 建议：改成由 mod 声明"谁拥有哪类车、概率多少"（例如 `OrganizationMod`/`SchedulerMod` 上的字段，或车辆 mod 注册时带"适用所有者"），Core 只负责找车位和建资产。

### - [ ] A3 车辆种类查不到时兜底 `"car"`（已确认）
- 位置：[vehicle.cpp:16](Forever_UE/Source/Core/traffic/vehicle.cpp:16)，[vehicle.h:36](Forever_UE/Source/Core/traffic/vehicle.h:36) 的注释也写着"查不到返回 car"
- 建议：没注册种类就是空串，或者注册时强制要求种类非空。

### - [ ] A4 按名字特判占位 mod（已确认）
- 位置：[traffic.cpp:405](Forever_UE/Source/Core/traffic/traffic.cpp:405)（`station_empty`）、[466](Forever_UE/Source/Core/traffic/traffic.cpp:466)（`route_empty`）、[phone.cpp:54](Forever_UE/Source/Core/player/phone.cpp:54)（`app_empty`）
- 问题：只有这三个 concept 特判了占位 id，其余 20 个没有；`<concept>_empty` 是 `Forever_Mod/Empty` 这个具体 mod 的 id。
- 建议：二选一——占位 mod 不应该出现在 `config.json` 的启用列表里（那就不需要特判）；或者在 Factory 注册时加一个通用的"占位"标记，Core 统一跳过。

### - [ ] A5 实体脚本兜底 `script_empty`，Core 强依赖一个具体 mod（已确认）
- 位置（兜底用的 id 第四轮收成了 Dependence 的 `kDefaultScriptModId`，问题本身没变）：`MakeScript` [script.cpp:879](Forever_UE/Source/Core/story/script.cpp:879)；[cabin.cpp:20](Forever_UE/Source/Core/map/cabin.cpp:20)、[map.cpp:3908](Forever_UE/Source/Core/map/map.cpp:3908)；默认值在 `Core/map/building.h`、`room.h`、`zone.h`，以及 Dependence 的 `building_mod.h`、`room_mod.h`、`zone_mod.h`、`feature_spec.h`
- 问题：没指定 ScriptMod 的实体脚本一律用 `"script_empty"`。`Script::Script` 在 mod 创建失败时抛 `NullPointerException`（[script.cpp:27](Forever_UE/Source/Core/story/script.cpp:27)），而配置界面允许把 `script_empty` 取消勾选。
- 影响：禁用 `script_empty` 后生成世界会抛异常退出。
- 建议：没指定 ScriptMod 时用 Dependence 里一个内建的"直通"ScriptMod（不经过 Factory），而不是依赖外部 mod 提供的 id。

---

## B. UE（Forever 模块）感知具体 mod

### - [ ] B1 开局装备写死 16 个资产 id（已确认）
- 位置：[ForeverCharacter.cpp:193-245](Forever_UE/Source/Forever/Player/ForeverCharacter.cpp:193)
- 问题：`PossessedBy` 里直接 `CreateAsset("asset_pistol")`、`asset_rifle`、`asset_container`、五种 `asset_ammo_*`、`asset_shotgun`、`asset_sniper`、`asset_rpg`、`asset_camera`、`asset_grenade`、`asset_smoke_grenade`，连数量（120 发、10 个）都写在 UE 里。
- 建议：改成脚本做（`game_start` 里用 `grant_asset` 之类的 Change），UE 不发装备。

### - [ ] B2 `TestNavigate` 只认 `green_park`（已确认）
- 位置：[ForeverPlayerController.cpp:54](Forever_UE/Source/Forever/Player/ForeverPlayerController.cpp:54)
- 建议：测试入口去掉类型过滤（任意带活动圆的绿化区），或者整个挪到脚本。

### - [ ] B3 整套公交专用子系统（已确认，改动量最大）
- 位置：[ForeverTrafficFrameworkComponent.cpp:246](Forever_UE/Source/Forever/Framework/ForeverTrafficFrameworkComponent.cpp:246)、[1549](Forever_UE/Source/Forever/Framework/ForeverTrafficFrameworkComponent.cpp:1549)，以及 `FBusState`、`busStates`、`busPhysicsClasses`、`EnsureBusPhysicsElement`、`BusToPhysics`/`BusToKinematic`、`CollectBusLoops`、`StartBusTestRun`/`StopBusRun`、`ResyncBusesAfterTimeSkip`；`Controller/BusController.*`
- 问题："近处切物理、远处走时刻表"这套机制靠 `GetStationType() == "bus"` 分派，类名和成员全按公交命名。
- 建议：抽象成能力而不是车型——例如 `RouteMod` 上加一个"近处切换为物理表示"的开关，UE 这边改名成"线路载具的物理接管"，任何打开这个开关的线路都能用。

### - [ ] B4 线路调试网格按 `bus`/`train`/`plane` 各一套（已确认）
- 位置：[ForeverTrafficFrameworkComponent.cpp:953-959](Forever_UE/Source/Forever/Framework/ForeverTrafficFrameworkComponent.cpp:953)、1018-1020，头文件里对应的 `busRouteMesh`/`trainRouteMesh`/`planeRouteMesh` 等成员
- 建议：按 `stationType` 字符串建一个 `TMap`，颜色由 mod 给或按哈希取。

### - [ ] B5 `vehicle_command` 直接映射到公交函数（已确认）
- 位置：[ForeverFrameworkActor.cpp:653-654](Forever_UE/Source/Forever/Framework/ForeverFrameworkActor.cpp:653)
- 问题：脚本词汇 `run`/`stop` 是通用的，实现却是 `StartBusTestRun`/`StopBusRun`。随 B3 一起改。

### - [ ] B6 触发盒大小按 `!= "car"` 判断（已确认）
- 位置：[VehicleElement.cpp:165](Forever_UE/Source/Forever/Element/VehicleElement.cpp:165)
- 建议：所有车都按自己网格的包围盒算，不需要分种类。

### - [ ] B7 按具体载具/武器命名的原生类（已确认，受约束）
- 位置：`Element/PlaneElement.*`、`BoatElement.*`、`PlaneWheel.*`、`VehicleWheelAnimInstance.*`（里面直接用 `APlaneElement`）、`Player/ForeverRocketProjectile.*`；`BoardTruck`（[ForeverPopulaceFrameworkComponent.cpp:1168](Forever_UE/Source/Forever/Framework/ForeverPopulaceFrameworkComponent.cpp:1168)，实际是"员工上组织的车"，和卡车无关）；`ForeverExplosionEffect.h:38` 的注释举例 `/Test/Effect/BP_GrenadeExplosion`
- 背景：安装版引擎下 mod 不能带 UE C++，这些类只能留在主程序。它们是通过 mod 给的蓝图路径间接选中的，UE 代码里没有按 mod id 分派，所以算"命名和归属"问题而不是硬违例。
- 建议：按能力命名（飞行载具/浮力载具/带尾迹的发射物），`BoardTruck` 改成中性名字。

### - [ ] B9 Basic 的脚本资源放在主程序的 `Resource/Story/`（已确认）
- 位置：`Forever_UE/Resource/Story/` 下共 26 个 `.script`，其中 `asset_burger.script`、`vehicle_bus.script`、`weapon_pistol.script`、`organization_shop.script`、`job_shop_saler.script` 这类按 mod id 命名的占大多数
- 问题：按 mod id 命名的这些是 Basic 各 mod 自己的脚本，和主程序资源混放（`citizen_common`、`entity_event` 这类通用脚本我没有逐个判断归属）。`Forever_UE/Resource/Mod/Basic/` 已经存在（放图片和地图文件）。
- 建议：挪到 `Resource/Mod/Basic/` 下。

### - [ ] B10 UE 注释里点名具体 mod（已确认，软性）
- `BuildingElement.cpp:645`、`ForeverBuildingFrameworkComponent.cpp:82`（Mohar），`ForeverAvatarSubsystem.cpp:107`（Yuanshen），`ForeverWeaponVisualComponent.h`（`weapon_pistol`）。没动。

---

## C. Bug

### - [ ] C4 `Player::Tick` 每帧截断毫秒，不累计余数（已确认）
- 位置：[player.cpp:52](Forever_UE/Source/Core/player/player.cpp:52)
- 问题：`int deltaMs = static_cast<int>(delta * 60 * 1000 * timeFlowRatio)`，小数部分每帧丢掉。
- 影响：游戏时钟速度随帧率变化（倍率 1、144 帧时每帧 416.67ms 截成 416，慢约 0.16%）；倍率小到每帧不足 1ms 时时钟完全不走。
- 建议：保留一个 `double` 余数累加。

### - [ ] C7 销毁车辆不释放车位和行程（潜伏）
- 位置：[vehicle.cpp:36](Forever_UE/Source/Core/traffic/vehicle.cpp:36)（`~Vehicle`）、[traffic.cpp:633](Forever_UE/Source/Core/traffic/traffic.cpp:633)（`DestroyVehicle`）、[615](Forever_UE/Source/Core/traffic/traffic.cpp:615)（`CreateVehicle` 同名覆盖分支）
- 问题：析构不调 `ReleaseSlot`/`ClearReservation`，`ParkingSlot::parkedVehicle`/`reservedBy` 留悬空指针，车位永远不空闲。`DestroyVehicle` 只取消组织任务行程，不取消通勤行程（`trips` 里留悬空 `Vehicle*`）；同名覆盖分支两种行程都不取消。
- 当前状态：`Traffic::DestroyVehicle` 没有任何调用方，车名也不会重复，所以还没触发。
- 建议：在 `~Vehicle` 里释放车位和预订；`DestroyVehicle` 补上 `FindTripByVehicle` 的取消。

### - [ ] C8 `Container::GetValue`/`SetValue` 按值跨 DLL 传 `ValueType`（潜伏）
- 位置：[utility.h:40-41](Forever_UE/Source/Dependence/common/utility.h:40)
- 问题：`GetValue` 按值返回 `pair<bool, ValueType>`，`SetValue` 按值收 `ValueType`。实现在宿主（`Script`），mod 通过虚函数调用。`ValueType` 是含 `std::string` 的 variant，值是超过 15 字符的字符串时就是"一侧分配、另一侧释放"，正是 `CONVENTIONS.md` 第 4 节禁止的情况。
- 当前状态：mod 里只有 `organization_shop`/`restaurant`/`video_company` 在调，读写的都是整数，所以没炸。
- 建议：改成按引用输出（`bool GetValue(name, ValueType& out)`）和 `const ValueType&` 入参。

### - [ ] C9 异常处理不全（已确认）
- [ForeverFrameworkActor.cpp:291](Forever_UE/Source/Forever/Framework/ForeverFrameworkActor.cpp:291)：`RunGenerationStep` 只 `catch (const ExceptionBase&)`。`std::out_of_range`、`std::bad_alloc`、`stod` 的异常等标准库异常会直接崩，而不是走"打日志退出"。
- [config.cpp:415](Forever_UE/Source/Core/common/config.cpp:415)：`AddResourcePath` 的 `recursive_directory_iterator` 没带 `error_code`，遇到无权限子目录抛 `filesystem_error`（`AddDllPath` 那边是带了的）。
- `Config::ReadConfig`：`dll_paths` 等数组里出现非字符串元素时 `AsString()` 抛 `JsonFormatException`，没人接，启动即崩。

### - [ ] C11 小问题（已确认，优先级低）
- [room.cpp:369](Forever_UE/Source/Core/map/room.cpp:369)：`Room::AddAsset` 用 `insert_or_assign`，同名资产会被覆盖并泄漏。资产名目前唯一，所以没触发。
- [config.cpp:196](Forever_UE/Source/Core/common/config.cpp:196)：`seed` 写成负数或超大浮点数时，`double` 转 `unsigned long long` 是未定义行为。
- [ForeverFrameworkActor.cpp:841](Forever_UE/Source/Forever/Framework/ForeverFrameworkActor.cpp:841)：`ApplySpawnNpc` 里 `*player->GetTime()` 没判空（世界生成完之后 `player` 一定非空，实际不会触发）。
- `Map::CreateBuildingAt`（[map.cpp:665](Forever_UE/Source/Core/map/map.cpp:665)）：建筑重名时 `delete building`，但之前已经登记的 `entityNav[building]` 和 `pendingBuildingRoadAccess` 里的 `owner` 指针还留着。mod 名字唯一，所以没触发。
- `Config::GetScriptPath`/`GetResourceFilePath`：遍历 `unordered_map`，多个资源目录里有同名文件时取哪个不确定；`GetResourceFilePath` 每次 post 都重新递归扫盘。

---

## D. 设计问题

### - [ ] D1 23 个 concept 的清单在多处重复维护
- `Dependence/common/mod_concepts.cpp` 的符号表（第三轮从 `loader.cpp` 挪过来）、`registry.cpp` 的 `RegisterMods`/`ReloadModArgs`/`CheckModRegistered` 三处、`registry.h` 的成员和 getter、`save.cpp` 的 `ConceptOps` 表、`Test/Mod/mod_checks.cpp` 的 `ModTestHost`（第三轮新增，启动时会核对有没有和符号表对上）。`ForeverModSubsystem` 那份随 B8 删掉了。
- 新增 concept 要改所有这些地方，漏一处不会编译报错。`Config` 里"单选 concept"的清单（`Roadnets`/`Names`）也重复了两份（[config.cpp:37](Forever_UE/Source/Core/common/config.cpp:37)、[591](Forever_UE/Source/Core/common/config.cpp:591)）。
- 建议：一张表（X-macro 或 tuple）生成全部。

### - [ ] D2 每个 `<Concept>Mod` 各写一遍相同的 6 个虚函数
- `UnitTest`/`SaveState`/`LoadState`/`GetStateVersion`/`GetInstanceCounter`/`SetInstanceCounter` 在 23 个 `*_mod.h` 里各一份，没有公共基类。
- 按你之前"不要动 Mod 基类继承"的要求，这条只报告，不建议我主动改。

### - [ ] D3 `Registry` 单例从不卸载 dll
- `RefreshMods` 只补加载新路径。编辑器开着时重编 mod dll 不会生效（文件还被占用），从配置里移除的 dll 也一直留在进程里（靠"不在启用表"屏蔽）。
- 是否要支持热重载看你的工作流。C5 的计数器复位是这个设计下的补丁：dll 不卸载，static 状态就得手动清。mod 里如果还有别的 static 状态，同样会跨局残留。

### - [ ] D4 `ApplyChange` 是约 40 个 `dynamic_cast` 的长链
- 位置：[ForeverFrameworkActor.cpp:375 起](Forever_UE/Source/Forever/Framework/ForeverFrameworkActor.cpp:375)
- 每个 Change 最坏要过 40 次 `dynamic_cast`；新增 Change 类型要在这条链里找位置。
- 建议：按 `Change::GetType()` 字符串建分发表，各域/各组件注册自己认识的类型。

### - [ ] D5 Roadnet 的启用方式和别的 Factory 不一致
- `RoadnetFactory` 用 `SetConfig(id, bool)`/`GetRoadnet()`（见 [map.cpp:180-193](Forever_UE/Source/Core/map/map.cpp:180)），其余 22 个用 `SetModArgs` + `IsEnabled`。`InitRoadnet` 里还要手动清上一局留下的启用标记。`RoadnetFactory::CheckRegistered`/`GetRegisteredIds` 也不看是否启用，和别的 Factory 语义不同。

### - [ ] D6 Core 的警告玩家看不到
- `debugf` 只走 `OutputDebugString`（附加调试器才能看到），`Config` 的警告走 `std::cerr`，都不进 `Forever.log`。Core 里有上百条 `Warning:` 日志实际上是丢的（包括这次 `Script::ClearCache` 被跳过时的那条）。
- 政治域已经有"Core 产出文本、UE 注入 sink"的做法（`polit_log`），可以推广成通用的 Core 日志 sink。

### - [ ] D7 `Config::AddDllPath` 为了探测会 `LoadLibrary` 目录下每一个 dll
- 位置：[config.cpp:324](Forever_UE/Source/Core/common/config.cpp:324)
- 递归扫描 `dll_paths`，对每个 `.dll` 都加载一次再卸载，无关 dll 的 `DllMain` 也会被执行。`dll_paths` 配到范围大的目录时有风险。

### - [ ] D8 `resource_paths` 不做路径规范化
- 当前 `config.json` 里就有 `Forever_Mod/Test/Resource/../../../../../Projects/Forever/Forever_UE/Resource/Layouts` 这种写法。同一个目录的不同写法会被当成两个根。
- 建议：存之前做 `lexically_normal`。

### - [ ] D9 死代码
- `VehicleMod::drivable`/`capacity`：Core 和 UE 都没有读取（`Vehicle::IsDrivable`/`GetCapacity` 无调用方）。
- `UForeverBuildingFrameworkComponent::ResolveLodMaterial`/`defaultLodMaterial`：无调用方。
- `Traffic::Tick` 的 `crossedDay`、`post` 两个参数没用。
- `Map::FindPath` 的 `coords` 参数没用（函数里 `(void)coords`）。
- `Config::GetActiveDllPaths`/`IsDllActive`：B8 删掉后在 `Source` 和 `Tools` 里都没有调用方了（`config.md` 里还有它们的说明）。

---

## F. 文档和注释

### 第一轮改了什么（已随 `29b82f6` 提交）

- 重写 `Source/Core/README.md`、`Forever/Framework/ForeverFrameworkComponent.md`。
- `CONVENTIONS.md`：第 3 节新增"Core 和 Forever(UE) 层不感知任何具体 mod"；补 `Source/Test`、Yuanshen；修正不存在的示例名和 `class.h` 分组。
- `Dependence/README.md`：concept 表补 Weapon；参数传递流程改成 `Registry::ReloadModArgs`。
- 批量修正过时引用（`MergeTemp`、`<concept>_basic.*`、`EconSim`、`GlobalAllocate` 等），按实际代码改写建筑 LOD 队列等段落，约 20 处代码注释。

### 第二轮改了什么（未提交）

- `CONVENTIONS.md` 第 5 节：把我第一轮写的"所有 mod id 一律是 `<concept>_<name>`"改成"mod id 可以是任意字符串，任何实现都不能假定格式"。
- 去掉各处"id 必须是…""不带/带 concept 前缀"的说法：`mod_test.h/.md`、`random.h`、`green_factory.md`、`party_factory.md`、`basic.md`、`Basic/README.md`、`script.md`、`registry.h/.md`、`config.md`、`loader.md`、`VehicleRig/README.md`、`Test/README.md`。
- 跟着代码改的文档：`route.md`、`route_mod.md`、`traffic.md`、`traffic_save.md`（C1/C2）；`script.md`（C3）；`save.md`（C5）；`ForeverSaveSubsystem.md`（C10）；`ForeverModSubsystem.md` 重写、`ForeverConfigBridgeSubsystem.md`、`loader.md`、`Dependence/README.md`、`Forever_Mod/README.md`（B8）；`ForeverFrameworkActor.md`、`ForeverTrafficFrameworkComponent.md`、`map.md`、`Core/README.md`。
- 新文档：`terrain_coverage.md`、`bus_stop_name.md`。

### - [ ] F1 文档里的日期和变更记录没有清理
- 97 份 md 里有 936 处 `2026-xx-xx` 标记，很多是"(2026-11-22 改)""原来是…现在…"这种变更记录。`CONVENTIONS.md` 第 1 节要求文档只写当前状态。
- 没动：量大，而且哪些是"值得保留的踩坑记录"需要你定尺度。

### - [ ] F2 仍有约 150 份 md 被引用扫描标记
- 我用脚本扫了"md 里用反引号引用、但代码里已不存在的标识符/文件"。第一轮修完后还剩 154 份、568 处；第二轮没有重扫。
- 我看过的大多是误报：简称（如用 `IndustryBuilding` 指代高/中/低三个类）、UE 引擎自带的标识符、`h/.cpp` 这种写法、故意写出来的"不要这样做"的名字。但**没有逐份确认**，里面可能还有真过时的。

### - [ ] F3 "待办"里提到不存在的类
- 例如 `Core/story/story.md` 的 `AddMessage`/`PopMessages`、`ForeverWeaponComponent.md` 的 `ArrestPlayerChange`/`ReleasePlayerChange`。它们是作为"还没做"列出的，不算错，但如果这些计划已经放弃，应该删掉。

---

## G. 现有测试失败（不是这两轮改动引起的）

CoreTest quick（`--suite all --quick`）三轮都是同样的 20 项失败，逐条对比完全相同。第二轮的报告在 `Forever_UE/Saved/TestReports/review_20261125_fix/core`，第一轮的在 `review_20261125/core`；第三轮经 `run_tests.bat` 跑的那份因为时间戳问题（已修）落在了 `Saved/TestReports/周三022611_171703`。

### - [ ] G1 route 套件 11 项失败（已确认是旧问题）
- 11-24 13:23 的旧报告（`Saved/TestReports/core`）里就有同样的 11 项。
- 内容：
  - 每个种子都有 39 个带导航节点的房间没接进行人图（`every room with a navigation node is connected to the pedestrian graph`）。
  - 反向路线不是正向路线的倒序（9-10 条）。
  - 路线缓存：反向没命中同一条缓存、新增了缓存项、24 小时无命中的项没保留。
  - 随机车位到车位的车辆路线失败率超过 5%（18/300）。

### - [ ] G2 polit 套件 8 项失败（推测与小地图配置有关）
- 只出现在 seed 12：两个党派创建时都没拿到会议室，后面的党代会、会议室冲突测试跟着失败。
- 11-24 的旧报告里 polit 是全过的。中间隔了提交 `7731158`（改了 `config.json`：小地图）。我没有回退验证，是推测。
- 值得看的点：地图上没有会议室房间时党派就没有会议室，这本身是个脆弱点（会议室只出现在 `ResidenceHighBuilding` 的一种布局里）。

### - [ ] G3 perform 套件 1 项失败
- `at most 5% of random slot-to-slot routes fail`（14/200，seed 11），和 G1 的车辆路线失败是同一类。旧报告里 perform 有 4 项失败。
