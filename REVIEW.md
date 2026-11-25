# 全项目 Review（2026-11-25）

基于提交 `7731158`（Design small map and fix bugs）。每一项前面的 `[ ]` 留给你逐项过的时候打勾/批注。

**怎么读这份清单**

- 编号：A=Core 感知具体 mod，B=UE 感知具体 mod，C=bug，D=设计问题，E=违反 `CONVENTIONS.md`，F=文档/注释遗留，G=现有测试失败。
- 可信度：
  - **已确认**：读过对应代码，行号核对过。
  - **潜伏**：代码确实有问题，但当前没有调用路径会触发。
  - **推测**：有依据但没有实际复现/回退验证。
- 行号对应本次 review 结束时的工作区（含这次改的注释，未提交）。
- 除了 C6（`Tools/ModWeb` 构建），本次没有改任何运行时逻辑，下面的问题都还没修。

**覆盖范围**

全项目约 12.5 万行代码、384 份 md。

- mod 感知（A、B）：对 Core/Dependence/Forever 三层的字符串字面量和标识符做了全量扫描，再逐处读上下文核对，这部分比较全。
- 找 bug（C）：重点精读 + 模式扫描，不是逐行通读。精读过的：`Core/common` 全部、`traffic.cpp`/`vehicle.cpp`、`room.cpp`、`economy.cpp`、`market.cpp`、`election.cpp`、`script.cpp` 的非词汇表部分、`map.cpp` 的前 1200 行和寻路部分、`ForeverFrameworkActor.cpp` 的前 1200 行、`ForeverModSubsystem.cpp`。Basic 各 mod、大部分 UI、存档编解码、`building.cpp`、`route.cpp` 主体只做了扫描，没有精读。

---

## A. Core 感知具体 mod

### - [ ] A1 每日需求的服务类型写死 `"food"`/`"video"`（已确认）
- 位置：[economy.cpp:31-35](Forever_UE/Source/Core/society/economy.cpp:31)、308-310
- 问题：`kMealServiceType="food"`、`kVideoServiceType="video"`，以及视频需求的财富阈值 `kVideoWealthLow/High`（2000 元/5000 元）都是 Core 常量。"市民需要什么服务、需要多少"是具体 mod（餐厅/视频公司）的业务。
- 影响：换一套组织 mod（没有 food/video 服务）后，Core 仍然只会产生这两种需求；新增一种服务类型必须改 Core。
- 建议：需求由 mod 声明。代码里的注释已经写了"先写死，以后由 scheduler 自定义"——比如让 `SchedulerMod` 或一个新的需求声明接口返回"(服务类型, 份数, 预算规则)"列表，Core 只做撮合。

### - [ ] A2 `AssignVehicles` 写死载具种类 `"car"`/`"truck"`（已确认）
- 位置：[traffic.cpp:564-565](Forever_UE/Source/Core/traffic/traffic.cpp:564)、584-585；常量在 31-34 行
- 问题：成年市民 20% 概率分一辆 `"car"`，组织随机 1-5 次分 `"truck"`。种类字符串是 Basic 的 `vehicle_sedan`/`vehicle_truck` 注册的，Core 不该知道。
- 附带：成年年龄用自己的 `kAdultAge = 18`，而经济/选举用的是可调政策 `GovernmentPolicy::adultAge`，两处会不一致。
- 建议：改成由 mod 声明"谁拥有哪类车、概率多少"（例如 `OrganizationMod`/`SchedulerMod` 上的字段，或车辆 mod 注册时带"适用所有者"），Core 只负责找车位和建资产。

### - [ ] A3 车辆种类查不到时兜底 `"car"`（已确认）
- 位置：[vehicle.cpp:16](Forever_UE/Source/Core/traffic/vehicle.cpp:16)，[vehicle.h:36](Forever_UE/Source/Core/traffic/vehicle.h:36) 的注释也写着"查不到返回 car"
- 建议：没注册种类就是空串，或者注册时强制要求种类非空。

### - [ ] A4 按名字特判占位 mod（已确认）
- 位置：[traffic.cpp:404](Forever_UE/Source/Core/traffic/traffic.cpp:404)（`station_empty`）、[465](Forever_UE/Source/Core/traffic/traffic.cpp:465)（`route_empty`）、[phone.cpp:54](Forever_UE/Source/Core/player/phone.cpp:54)（`app_empty`）
- 问题：只有这三个 concept 特判了占位 id，其余 20 个没有；`<concept>_empty` 是 `Forever_Mod/Empty` 这个具体 mod 的 id。
- 建议：二选一——占位 mod 不应该出现在 `config.json` 的启用列表里（那就不需要特判）；或者在 Factory 注册时加一个通用的"占位"标记，Core 统一跳过。

### - [ ] A5 实体脚本兜底 `script_empty`，Core 强依赖一个具体 mod（已确认）
- 位置：`MakeScript` [script.cpp:864](Forever_UE/Source/Core/story/script.cpp:864)；[cabin.cpp:20](Forever_UE/Source/Core/map/cabin.cpp:20)、[map.cpp:3907](Forever_UE/Source/Core/map/map.cpp:3907)；默认值在 `Core/map/building.h:563`、`room.h:193`、`zone.h:117`，以及 Dependence 的 `building_mod.h:57/109/152`、`room_mod.h:52`、`zone_mod.h:91`、`feature_spec.h:22`
- 问题：没指定 ScriptMod 的实体脚本一律用 `"script_empty"`。`Script::Script` 在 mod 创建失败时抛 `NullPointerException`（[script.cpp:26](Forever_UE/Source/Core/story/script.cpp:26)），而配置界面允许把 `script_empty` 取消勾选。
- 影响：禁用 `script_empty` 后生成世界会抛异常退出。
- 建议：没指定 ScriptMod 时用 Dependence 里一个内建的"直通"ScriptMod（不经过 Factory），而不是依赖外部 mod 提供的 id。

### - [ ] A6 按 id 前缀 `zone_` 判断预设类型（已确认）
- 位置：[map.cpp:511](Forever_UE/Source/Core/map/map.cpp:511)
- 问题：`preset.modId.rfind("zone_", 0) == 0` 决定预设走园区还是建筑。`<concept>_<name>` 只是命名约定，`registry.md` 里明确写了 Core 不依赖它。
- 建议：直接问 `zoneFactory.CheckRegistered(id)` / `buildingFactory.CheckRegistered(id)`，或者在路网文件的预设里显式带 concept 字段。

### - [ ] A7 主线脚本退回名为 `test` 的脚本（已确认）
- 位置：[config.cpp:502-504](Forever_UE/Source/Core/common/config.cpp:502)
- 问题：`GetMainStoryScriptPath()` 返回 `GetScriptPath("test")`，`test` 是 `Forever_Mod/Test` 的脚本名。
- 建议：没配 `story_scripts` 就是没有主线脚本，不兜底。

### - [ ] A8 默认地形 `plain` 的 UE 资产路径写在 Core（已确认）
- 位置：[map.cpp:3040-3050](Forever_UE/Source/Core/map/map.cpp:3040)（`GetTerrainAppearances`），默认值在 [map.h:37](Forever_UE/Source/Core/map/map.h:37)
- 问题：这条不是感知 mod，而是 Core 里出现了 `/Game/Asset/Terrain/Plain/...` 三个 UE 资产路径，属于分层问题。
- 建议：默认地形的外观放到 UE 层兜底，或者做成一个 `terrain_plain` mod。

### - [ ] A9 `TerrainMod` 基类里写死 Megascans 目录约定（已确认）
- 位置：[terrain_mod.h:42](Forever_UE/Source/Dependence/map/terrain_mod.h:42)
- 问题：辅助函数拼 `/Game/3rdParty/Megascans/Surfaces/<folder>/Medium/<id>_tier_2/Textures/...`。这是 Basic 那几个地形用的资产包的目录结构，放在所有 mod 共用的基类里。
- 建议：挪到 Basic 的公共头里。

### - [ ] A10 `isConstruction` 回调的语义迁就具体路网 mod（已确认）
- 位置：[map.cpp:202-204](Forever_UE/Source/Core/map/map.cpp:202)
- 问题：注释写明"路网 mod(棋盘格/井字)把可建设当成 z=0 的平地用，所以给它们的是 `IsGroundConstruction`；非 0 高度的平台只有 `roadnet_file` 会用，它自己不看这个回调"。接口语义是按三个具体 mod 的用法定的。
- 建议：回调给出中性信息（是否平坦 + 高度），由 mod 自己决定要不要 z=0。

### - [ ] A11 Core 注释里点名具体 mod（已确认，软性）
- 例子：`map.cpp` 里的 `JingRoadnet`/`roadnet_jing.md`、`vehicle.cpp:31` 的 `vehicle_bus.script`、`economy.cpp` 的"视频公司"、`traffic.cpp` 的 `room_parking`/"机场的飞机、港口的船"、`populace.cpp` 的 `scheduler_basic`。
- 这次没动。要不要清理看你的尺度：举例说明是有用的，但严格按"Core 不感知 mod"应该换成中性描述。

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
- 位置：[ForeverFrameworkActor.cpp:649-650](Forever_UE/Source/Forever/Framework/ForeverFrameworkActor.cpp:649)
- 问题：脚本词汇 `run`/`stop` 是通用的，实现却是 `StartBusTestRun`/`StopBusRun`。随 B3 一起改。

### - [ ] B6 触发盒大小按 `!= "car"` 判断（已确认）
- 位置：[VehicleElement.cpp:165](Forever_UE/Source/Forever/Element/VehicleElement.cpp:165)
- 建议：所有车都按自己网格的包围盒算，不需要分种类。

### - [ ] B7 按具体载具/武器命名的原生类（已确认，受约束）
- 位置：`Element/PlaneElement.*`、`BoatElement.*`、`PlaneWheel.*`、`VehicleWheelAnimInstance.*`（里面直接用 `APlaneElement`）、`Player/ForeverRocketProjectile.*`；`BoardTruck`（[ForeverPopulaceFrameworkComponent.cpp:1168](Forever_UE/Source/Forever/Framework/ForeverPopulaceFrameworkComponent.cpp:1168)，实际是"员工上组织的车"，和卡车无关）；`ForeverExplosionEffect.h:38` 的注释举例 `/Test/Effect/BP_GrenadeExplosion`
- 背景：安装版引擎下 mod 不能带 UE C++，这些类只能留在主程序。它们是通过 mod 给的蓝图路径间接选中的，UE 代码里没有按 mod id 分派，所以算"命名和归属"问题而不是硬违例。
- 建议：按能力命名（飞行载具/浮力载具/带尾迹的发射物），`BoardTruck` 改成中性名字。

### - [ ] B8 `ForeverModSubsystem` 另建一套 Factory 做校验，只覆盖 16 个 concept（已确认）
- 位置：[ForeverModSubsystem.cpp:152 起](Forever_UE/Source/Forever/Mod/ForeverModSubsystem.cpp:152)
- 问题：每次开局用局部 `ModLoader` + 局部 Factory 把 dll 再注册一遍，逐个 id 创建临时实例打日志。Terrain/Roadnet/Zone/Green/Building/Weapon/Party 七个 concept 没覆盖；和 Core 的 `Registry` 重复；还带来 C5 的副作用。
- 建议：直接对 `Registry::Get()` 里的 Factory 做校验，或者删掉这段（CoreTest 的 mods 套件已经在做同样的事）。

### - [ ] B9 Basic 的脚本资源放在主程序的 `Resource/Story/`（已确认）
- 位置：`Forever_UE/Resource/Story/` 下共 26 个 `.script`，其中 `asset_burger.script`、`vehicle_bus.script`、`weapon_pistol.script`、`organization_shop.script`、`job_shop_saler.script` 这类按 mod id 命名的占大多数
- 问题：按 mod id 命名的这些是 Basic 各 mod 自己的脚本，和主程序资源混放（`citizen_common`、`entity_event` 这类通用脚本我没有逐个判断归属）。`Forever_UE/Resource/Mod/Basic/` 已经存在（放图片和地图文件）。
- 建议：挪到 `Resource/Mod/Basic/` 下。

### - [ ] B10 UE 注释里点名具体 mod（已确认，软性）
- `BuildingElement.cpp:645`、`ForeverBuildingFrameworkComponent.cpp:82`（Mohar），`ForeverAvatarSubsystem.cpp:107`（Yuanshen），`ForeverWeaponVisualComponent.h`（`weapon_pistol`）。这次没动。

---

## C. Bug

### - [ ] C1 线路车辆的"游戏秒/真实秒"系数写死成 120（已确认）
- 位置：[route.cpp:21](Forever_UE/Source/Core/traffic/route.cpp:21)，用在 294、721、734 行
- 问题：`kGameSecondsPerRealSecond = 120` 对应 `timeFlowRatio = 2.0`。但 `Player::timeFlowRatio` 默认 10.0，`test.script`/`ys.script` 都用 `global_setting` 设成 1，而且脚本可以运行时改。
- 影响：
  - 721 行的 `speedMapPerRealSecond` 被 UE 用作公交从运动学切到物理时的初速度（[ForeverTrafficFrameworkComponent.cpp:1637](Forever_UE/Source/Forever/Framework/ForeverTrafficFrameworkComponent.cpp:1637)）。倍率为 1 时初速度快一倍。
  - 734 行的 `dwellRemainingSeconds` 是停站剩余真实秒（同文件 1652 行在用）。倍率为 1 时短一半。
  - 294 行的 `easeSeconds` 换算同样偏差。
- 建议：`Route` 从调用方拿实际倍率（UE 侧已经有 `GameSecondsPerRealSecond()`，在 `ForeverPopulaceFrameworkComponent.cpp:779`）。
- 注：我在这行注释和 `route_mod.md` 里标了这个问题。

### - [ ] C2 `Traffic::elapsedSeconds` 是 `float` 且一直累加（已确认，长时间游玩才触发）
- 位置：[traffic.h:199](Forever_UE/Source/Core/traffic/traffic.h:199)，`Traffic::Tick` 累加；`Route::Update(float)` [route.h:109](Forever_UE/Source/Core/traffic/route.h:109)、`lastElapsedSeconds`
- 问题：`float` 只有 24 位有效数字。累计超过 2^24 秒（约 194 游戏天）后精度是 2 秒；倍率为 1、60 帧时每帧增量正好 1 游戏秒，加不进去，线路车辆停住。在这之前精度逐步变差，车辆运动会一顿一顿。`jump_time` 会加速到达这个点。
- 建议：改 `double`（存档里对应字段一起改）。

### - [ ] C3 `Script::caches` 静态缓存从不清空（已确认）
- 位置：[script.cpp:16](Forever_UE/Source/Core/story/script.cpp:16)，[108 行](Forever_UE/Source/Core/story/script.cpp:108)命中缓存就直接返回
- 问题：按文件路径缓存解析结果，整个进程生命周期不清。没有任何清理接口。
- 影响：编辑器不关的情况下，改了 `.script` 再开 PIE 读到的还是旧内容。`Milestone*` 也不释放。
- 建议：每局开始（`Registry::RefreshMods` 那个阶段）清一次，或者按文件修改时间失效。
- 我没有在编辑器里实际复现，是从代码推出来的；如果你平时改脚本后重开 PIE 是生效的，告诉我，我再查是哪里绕过了缓存。

### - [ ] C4 `Player::Tick` 每帧截断毫秒，不累计余数（已确认）
- 位置：[player.cpp:52](Forever_UE/Source/Core/player/player.cpp:52)
- 问题：`int deltaMs = static_cast<int>(delta * 60 * 1000 * timeFlowRatio)`，小数部分每帧丢掉。
- 影响：游戏时钟速度随帧率变化（倍率 1、144 帧时每帧 416.67ms 截成 416，慢约 0.16%）；倍率小到每帧不足 1ms 时时钟完全不走。
- 建议：保留一个 `double` 余数累加。

### - [ ] C5 mod 命名计数器新开局不复位（已确认）
- 位置：各 mod 的 `static int count`；只有读档时 [save.cpp:135](Forever_UE/Source/Core/common/save.cpp:135) 的 `SetInstanceCounter` 会恢复
- 问题：`Registry` 常驻持有 dll，计数器跨局累加。同一进程第二次开局（再开 PIE、回菜单重开），建筑/房间等名字的编号接着上一局涨。另外 B8 的校验每次开局会为 16 个 concept 的每个 id 多建一个实例，再吃掉一个编号，所以 UE 和 CoreTest 的名字也差一格。
- 影响：同种子在不同会话、UE 与 CoreTest 之间生成的名字不一致（名字是 `buildings` 表的键，也出现在地址里）。不会崩。
- 建议：开局时把所有 mod 的计数器清零（复用存档那套 `setCounter`）。

### - [x] C6 `Tools/ModWeb` 构建已坏（已确认，已修）
- 位置：[build_web.bat](Forever_UE/Tools/ModWeb/build_web.bat)、[web_main.cpp](Forever_UE/Tools/ModWeb/web_main.cpp)
- 问题：还引用 2026-11-13 改名前的 `puzzle_basic.*`/`app_basic.*`；源文件清单缺后来加的 `Dependence/common/random.cpp`、`mod_test.cpp`；网页版不编 `utility.cpp`，缺 `debugf`。
- 修了什么：include 和源文件清单换成 `puzzle_tetris`/`puzzle_anykey`/`app_bounce`/`app_notes`，补上两个 `.cpp`，在 `web_main.cpp` 里加了 `debugf` 桩（打到 stderr）。
- 验证：`build_web.bat` 现在能产出 `dist/forever_mods.html`（13.8MB）。**没有在浏览器里打开验证运行**。

### - [ ] C7 销毁车辆不释放车位和行程（潜伏）
- 位置：[vehicle.cpp:36](Forever_UE/Source/Core/traffic/vehicle.cpp:36)（`~Vehicle`）、[traffic.cpp:632](Forever_UE/Source/Core/traffic/traffic.cpp:632)（`DestroyVehicle`）、[614](Forever_UE/Source/Core/traffic/traffic.cpp:614)（`CreateVehicle` 同名覆盖分支）
- 问题：析构不调 `ReleaseSlot`/`ClearReservation`，`ParkingSlot::parkedVehicle`/`reservedBy` 留悬空指针，车位永远不空闲。`DestroyVehicle` 只取消组织任务行程，不取消通勤行程（`trips` 里留悬空 `Vehicle*`）；同名覆盖分支两种行程都不取消。
- 当前状态：`Traffic::DestroyVehicle` 没有任何调用方，车名也不会重复，所以还没触发。
- 建议：在 `~Vehicle` 里释放车位和预订；`DestroyVehicle` 补上 `FindTripByVehicle` 的取消。

### - [ ] C8 `Container::GetValue`/`SetValue` 按值跨 DLL 传 `ValueType`（潜伏）
- 位置：[utility.h:40-41](Forever_UE/Source/Dependence/common/utility.h:40)
- 问题：`GetValue` 按值返回 `pair<bool, ValueType>`，`SetValue` 按值收 `ValueType`。实现在宿主（`Script`），mod 通过虚函数调用。`ValueType` 是含 `std::string` 的 variant，值是超过 15 字符的字符串时就是"一侧分配、另一侧释放"，正是 `CONVENTIONS.md` 第 4 节禁止的情况。
- 当前状态：mod 里只有 `organization_shop`/`restaurant`/`video_company` 在调，读写的都是整数，所以没炸。
- 建议：改成按引用输出（`bool GetValue(name, ValueType& out)`）和 `const ValueType&` 入参。

### - [ ] C9 异常处理不全（已确认）
- [ForeverFrameworkActor.cpp:287](Forever_UE/Source/Forever/Framework/ForeverFrameworkActor.cpp:287)：`RunGenerationStep` 只 `catch (const ExceptionBase&)`。`std::out_of_range`、`std::bad_alloc`、`stod` 的异常等标准库异常会直接崩，而不是走"打日志退出"。
- [config.cpp:415](Forever_UE/Source/Core/common/config.cpp:415)：`AddResourcePath` 的 `recursive_directory_iterator` 没带 `error_code`，遇到无权限子目录抛 `filesystem_error`（`AddDllPath` 那边是带了的）。
- `Config::ReadConfig`：`dll_paths` 等数组里出现非字符串元素时 `AsString()` 抛 `JsonFormatException`，没人接，启动即崩。

### - [ ] C10 存档直接覆盖写（已确认）
- 位置：[ForeverSaveSubsystem.cpp:191](Forever_UE/Source/Forever/Mod/ForeverSaveSubsystem.cpp:191)
- 问题：`SaveArrayToFile` 直接写目标路径。写到一半失败（磁盘满、进程被杀）会毁掉原来的存档。
- 建议：先写 `.tmp` 再改名。

### - [ ] C11 小问题（已确认，优先级低）
- [room.cpp:369](Forever_UE/Source/Core/map/room.cpp:369)：`Room::AddAsset` 用 `insert_or_assign`，同名资产会被覆盖并泄漏。资产名目前唯一，所以没触发。
- [config.cpp:196](Forever_UE/Source/Core/common/config.cpp:196)：`seed` 写成负数或超大浮点数时，`double` 转 `unsigned long long` 是未定义行为。
- [ForeverFrameworkActor.cpp:837](Forever_UE/Source/Forever/Framework/ForeverFrameworkActor.cpp:837)：`ApplySpawnNpc` 里 `*player->GetTime()` 没判空（世界生成完之后 `player` 一定非空，实际不会触发）。
- `Map::CreateBuildingAt`（`map.cpp:664`）：建筑重名时 `delete building`，但之前已经登记的 `entityNav[building]` 和 `pendingBuildingRoadAccess` 里的 `owner` 指针还留着。mod 名字唯一，所以没触发。
- `Config::GetScriptPath`/`GetResourceFilePath`：遍历 `unordered_map`，多个资源目录里有同名文件时取哪个不确定；`GetResourceFilePath` 每次 post 都重新递归扫盘。

---

## D. 设计问题

### - [ ] D1 23 个 concept 的清单在六七处重复维护
- `loader.cpp` 的符号表、`registry.cpp` 的 `RegisterMods`/`ReloadModArgs`/`CheckModRegistered` 三处、`registry.h` 的成员和 getter、`save.cpp` 的 `ConceptOps` 表、`ForeverModSubsystem`（只有 16 个）。
- 新增 concept 要改所有这些地方，漏一处不会编译报错。`Config` 里"单选 concept"的清单（`Roadnets`/`Names`）也重复了两份（[config.cpp:37](Forever_UE/Source/Core/common/config.cpp:37)、[591](Forever_UE/Source/Core/common/config.cpp:591)）。
- 建议：一张表（X-macro 或 tuple）生成全部。

### - [ ] D2 每个 `<Concept>Mod` 各写一遍相同的 6 个虚函数
- `UnitTest`/`SaveState`/`LoadState`/`GetStateVersion`/`GetInstanceCounter`/`SetInstanceCounter` 在 23 个 `*_mod.h` 里各一份，没有公共基类。
- 按你之前"不要动 Mod 基类继承"的要求，这条只报告，不建议我主动改。

### - [ ] D3 `Registry` 单例从不卸载 dll
- `RefreshMods` 只补加载新路径。编辑器开着时重编 mod dll 不会生效（文件还被占用），从配置里移除的 dll 也一直留在进程里（靠"不在启用表"屏蔽）。
- 是否要支持热重载看你的工作流；至少文档里要写清楚（`ForeverFrameworkActor.h` 里"EndPlay 让 dll 卸载"的旧注释我已经改掉）。

### - [ ] D4 `ApplyChange` 是约 40 个 `dynamic_cast` 的长链
- 位置：[ForeverFrameworkActor.cpp:371 起](Forever_UE/Source/Forever/Framework/ForeverFrameworkActor.cpp:371)
- 每个 Change 最坏要过 40 次 `dynamic_cast`；新增 Change 类型要在这条链里找位置。
- 建议：按 `Change::GetType()` 字符串建分发表，各域/各组件注册自己认识的类型。

### - [ ] D5 Roadnet 的启用方式和别的 Factory 不一致
- `RoadnetFactory` 用 `SetConfig(id, bool)`/`GetRoadnet()`（见 [map.cpp:180-193](Forever_UE/Source/Core/map/map.cpp:180)），其余 22 个用 `SetModArgs` + `IsEnabled`。`InitRoadnet` 里还要手动清上一局留下的启用标记。

### - [ ] D6 Core 的警告玩家看不到
- `debugf` 只走 `OutputDebugString`（附加调试器才能看到），`Config` 的警告走 `std::cerr`，都不进 `Forever.log`。Core 里有上百条 `Warning:` 日志实际上是丢的。
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

---

## E. 违反 `CONVENTIONS.md`

### - [ ] E1 `.vcxproj.filters` 漏文件
- `Dependence.vcxproj.filters` 缺 `common/random.h`、`common/random.cpp`。
- `Test.vcxproj.filters` 缺 `econ_report.h`、`econ_report.cpp`。
- （`.vcxproj` 本身都列全了，只影响 VS 里的筛选器显示。）

### - [ ] E2 头文件没有配对 `.cpp`（第 2 节要求每个 `.h` 配一个）
- Core：`common/save_refs.h`
- Dependence：`map/transit_data.h`、`polit/party_mod.h`、`polit/policy_ids.h`
- Basic：`player/puzzle_common.h`、`traffic/bus_stop_name.h`（`map/room_furniture_catalog.h` 是工具生成的，可以豁免）

### - [ ] E3 头文件没有配对 `.md`
- Core：`map/terrain_coverage.h`
- Basic：`traffic/bus_stop_name.h`（10 个小游戏共用 `puzzle_games.md`，符合"同类可共用"）
- Test/Core：`child_world.h`、`process_util.h`、`test_framework.h`、`world_build.h`

### - [ ] E4 其它
- [room.h:14-15](Forever_UE/Source/Core/map/room.h:14) 自己写了 `class Script; class Vehicle;` 前向声明（约定是统一放 `class.h`）。
- `config.cpp` 第一行是 `#include <set>`，在自己的头文件之前；`implement.cpp` 的 include 分组也不合规。

---

## F. 文档和注释

### 这次改了什么（119 个文件，未提交）

- **重写**：`Source/Core/README.md`（原文还写着"其余 domain 目前没有 Core 层本体类"）、`Forever/Framework/ForeverFrameworkComponent.md`（原文是迁移期的"旧 Framework Actor 对照表"）。
- **`CONVENTIONS.md`**：
  - 第 3 节新增一条，把"Core 和 Forever(UE) 层不感知任何具体 mod"写成明文规则。**这是我加的规则文字，请确认措辞。**
  - 第 3 节补了 `Source/Test`；第 5 节补了"mod id 一律 `<concept>_<name>`、Basic 每个 mod 一组文件"；第 9 节补了 Yuanshen；修正了不存在的示例名（`PengzhanBuilding`、`terrain_basic`、`ForeverGameMode`）和 `class.h` 分组（加 Polit）。
- **`Dependence/README.md`**：concept 表补 Weapon；参数传递流程从 `ForeverModSubsystem` 改成 `Registry::ReloadModArgs`；修正共享原语的文件名。
- **批量修正的过时引用**：
  - 20 份 Factory 文档里不存在的 `MergeTemp` 两段式注册。
  - 13 处 `Source/Basic/<domain>/<concept>_basic.*`（Basic 早已按 mod id 拆文件）。
  - 已合并进 CoreTest 的 `EconSim`、已改名的 `GlobalAllocate`（现在是 `Distribute`）、`InitAirportVehicles`（现在是 `InitStockedVehicles`）。
  - "mod id 不带 concept 前缀"的旧说法（4 处文档 + 2 处代码注释）。
  - `time_flow_ratio` 默认值（文档写 2.0，实际 10.0）。
- **按实际代码改写的段落**：建筑 LOD 操作队列（`ForeverBuildingFrameworkComponent.md`、`BuildingElement.md`，原文描述的 `BuildFarMesh`/`doorTierOpQueue` 等都不存在）、`ForeverModSubsystem.md`、`terrain_factory.md`、`roadnet_mod.md` 的 `GreenSite` 结构、门资产路径的填写位置、`VehicleRig/README.md` 的新增载具步骤。
- **代码注释约 20 处**：指向已改名函数/文件的引用、`party_mod.h` 里指向 Claude 记忆文件的引用（改成指向 `CONVENTIONS.md`）、`ForeverFrameworkActor.h` 里 `EndPlay` 的过时解释、`vehicle_mod.h` 里"搭乘未实现"的说法。
- 约 30 处"已删/原来是…"的纯过程叙述改成了当前状态描述。

### - [ ] F1 文档里的日期和变更记录没有清理
- 97 份 md 里有 936 处 `2026-xx-xx` 标记，很多是"(2026-11-22 改)""原来是…现在…"这种变更记录。`CONVENTIONS.md` 第 1 节要求文档只写当前状态。
- 这次没动：量大，而且哪些是"值得保留的踩坑记录"需要你定尺度。

### - [ ] F2 仍有 154 份 md 被引用扫描标记
- 我用脚本扫了"md 里用反引号引用、但代码里已不存在的标识符/文件"。修完后还剩 154 份、568 处。
- 我看过的大多是误报：简称（如用 `IndustryBuilding` 指代高/中/低三个类）、UE 引擎自带的标识符、`h/.cpp` 这种写法、故意写出来的"不要这样做"的名字。但**没有逐份确认**，里面可能还有真过时的。
- 扫描脚本和报告在本次会话的临时目录里，需要的话我可以重跑并把报告放进仓库。

### - [ ] F3 `Core/story/story.md`、`ForeverWeaponComponent.md` 等的"待办"里提到不存在的类
- 例如 `AddMessage`/`PopMessages`、`ArrestPlayerChange`/`ReleasePlayerChange`。它们是作为"还没做"列出的，不算错，但如果这些计划已经放弃，应该删掉。

---

## G. 现有测试失败（不是这次改动引起的）

CoreTest quick（`--suite all --quick`）：7939 通过，20 失败。我的代码改动只有注释和 `Tools/ModWeb`，不影响这些套件。报告在 `Forever_UE/Saved/TestReports/review_20261125/core`。

### - [ ] G1 route 套件 11 项失败（已确认是旧问题）
- 11-24 13:23 的旧报告（`Saved/TestReports/core`）里就有同样的 11 项。
- 内容：
  - 每个种子都有 39 个带导航节点的房间没接进行人图（`every room with a navigation node is connected to the pedestrian graph`）。
  - 反向路线不是正向路线的倒序（9-10 条）。
  - 路线缓存：反向没命中同一条缓存、新增了缓存项、24 小时无命中的项没保留。
  - 随机车位到车位的车辆路线失败率超过 5%（18/300）。

### - [ ] G2 polit 套件 8 项失败（推测与今天的配置改动有关）
- 只出现在 seed 12：两个党派创建时都没拿到会议室，后面的党代会、会议室冲突测试跟着失败。
- 11-24 的旧报告里 polit 是全过的。中间只隔了提交 `7731158`（改了 `config.json`：小地图）。我没有回退验证，是推测。
- 值得看的点：地图上没有 `room_meeting` 房间时党派就没有会议室，这本身是个脆弱点（会议室只出现在 `ResidenceHighBuilding` 的一种布局里）。

### - [ ] G3 perform 套件 1 项失败
- `at most 5% of random slot-to-slot routes fail`（14/200，seed 11），和 G1 的车辆路线失败是同一类。旧报告里 perform 有 4 项失败。

### 构建
- `build.bat` 全量通过：`Framework.sln`（0 错误，20 个警告）、UE 模块 `ForeverEditor`、四个外部 mod 均成功。
- UE 侧带画面的测试和 PIE 没有跑。
