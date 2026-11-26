# Source/Test

自动化测试管线。Mod 自测(`Mod/`，`ModTest.exe`：给 mod 作者用，只依赖 Dependence，见 `Mod/ModTest.md`)、Core 无画面管线(`Core/`)和 UE 管线(`UE/`，模块 `ForeverTest`，有 boot/components/saveload(无渲染)和 perform/commute/ride/changes(perform、commute、ride 带窗口渲染)套件)。

## 用法
```
Source\Test\run_tests.bat [quick|full] [core|ue|all]
```
`core` 管线先对 config 里 `dll_paths` 下的全部 mod dll 跑 ModTest(报告在 `<out>/../mod`)，再跑 CoreTest。
单独跑 ModTest：`x64\Release\ModTest.exe <mod.dll|目录>... [--with <dll|目录>] [--config config.json] [--seed N] [--out dir]`，全量(含 `Forever_Mod` 下所有 mod、查重名)用 `x64\Release\ModTest.exe x64\Release ..\Forever_Mod`。
单独跑 CoreTest：`x64\Release\CoreTest.exe Resource\Config\config.json --suite random,mods,lint,world,econ,save,perform,route,green,presets,plateau,polit,transit,terrain,unload,fault|all [--seed S|--seeds N] [--size N] [--months N] [--quick|--full] [--out dir]`。
先编译：`MSBuild Source\Framework.sln /p:Configuration=Release /p:Platform=x64`(必须编整个 sln：只编 `Source/Test/Core/Test.vcxproj`(输出仍是 `CoreTest.exe`)不会重编 Core.lib，改了 Core 会测到旧代码)。改了 `Dependence` 头文件要把 `Forever_Mod` 下 Empty/Test/Wxdj/Yuanshen/Tutorial 也各自重编，lint 会检查 DLL 是否比头文件旧。UE 代码用 UBT 编 `ForeverEditor Win64 Development`。

## 套件
- random：同种子跨进程所有阶段严格一致、各阶段无未包作用域的随机、DLL 导出 BindModHost、源码里无绕过 GetRandom 的随机源、流状态快照恢复。
- mods：只查 mod 接进 Core 之后的事——config 的 `dll_paths` 下发现的 mod id 全局唯一、Core 的 `Registry` 认得每个 mod、`ResetInstanceCounters` 复位命名计数器、`Config` 对 mod 参数的读写/暂存。**对 mod 本身的测试(UnitTest、漏包随机、名字唯一、字段合法性、依赖声明)在 ModTest 里**，见 `Mod/ModTest.md`。
- lint：mod DLL 不旧于 Dependence 头文件。
- terrain：地形覆盖图烘焙(2026-11-24)。`terrain_coverage`的纯函数：覆盖度和=255、对称、直边单调、45°斜线边界模糊后沿对角线波纹≤8(不模糊>100)、mip链、降采样、非法输入、大图耗时。UE侧材质画面只能 PIE 目测。
- green：绿化区(2026-11-20)。纯函数(`polygon_util`)、直接构造`green_park`测凹多边形圆/连路圆校验丢弃，再用`Tools/MapEditor/samples/demo.roadnet`(一块连路、一块不连路的凹多边形绿化)+`demo.terrain`替换config里的路网/地形生成世界：圆在多边形内且互不重叠、装饰物在多边形内、`LocateGreen`地址往返、房间↔圆的行人路线、地图存档往返字节稳定、市民站在/走向绿化区的存档往返和Core行进到达。demo文件缺失=失败；`make_demo.js`改动后要重新生成。
- unload(2026-11-25)：一局结束后Core有没有泄漏、mod dll能不能卸干净。子进程里对已生成的世界连做3轮"销毁(和UE的`EndPlay`同一个顺序)→查泄漏→`Registry::UnloadMods`→重新加载→再生成"：销毁后没有活着的`Script`(`Script::ClearCache`返回true)、23个Factory里没有活着的mod实例、每个dll都卸载并且确实不在进程里(`GetModuleHandle`)、没有mod id还留着注册、卸载重载后同种子生成的世界指纹和第一次一致、相邻两轮销毁后的私有内存不持续上涨(涨幅小于一个世界的20%)。最后故意留一个取名mod的实例不销毁，查保护：只有它所在的dll被留下、别的照常卸载、实例还能用还能销毁、销毁后再卸一次全部走干净。这个套件是CoreTest里唯一会销毁世界的地方。
- fault：故障注入，逐条验证异常体系的规则(`Dependence/common/error.md`)。父进程里(不需要mod)：三类异常的类别/消息/抛出位置、不是`ExceptionBase`的异常按"当时是不是在mod里"归类、`ErrorScope`的顺序和过期清理、`GuardData`只接数据错误、`MOD_CALL`、`TryParseInt`/`TryParseDouble`和`stoi`/`stod`逐个输入一致、坏config读完是空的不是半份。子进程里(一个种子、256的小地图)：测试自带的假小游戏mod(登记在测试自己的工厂里，不经过dll)抛三种异常(标准库异常/`DataException`/`throw 42`)，都能接住、归类正确、指名到这个mod；坏的脚本/layout/表达式是数据错误且报告里带文件；出过这些错之后世界仍能销毁、没有泄漏、mod dll仍能全部卸掉。验证不了的：UE层`CoreGuard`的回菜单流程；异常从mod dll里抛出、穿过dll边界到宿主这一段(原来有一个专门出错的mod`puzzle_fault`覆盖它，2026-11-26删掉了)；宿主和mod分配器不同的问题(CoreTest和mod用同一个堆，只有UE里才不同)。
- plateau：非0高度地块(2026-11-23)。`Tools/MapEditor/samples/plateau.roadnet/.terrain`(make_plateau.js生成：抹平出z=3的平台+平台上方框路(节点z=3)+一块居住地块+一条贴合路下山)，config仅换路网/地形mod，其余mod照常自动填充地块。对每个所属地块z≠0的建筑/园区检查：平台格`IsConstruction`但高度不为0、地块z=平台高度、`GetGroundZ()`=地块z、房间导航节点z=`GetFloorWorldZ`、建筑门不低于地坪、园区大门/内部路z=地坪、地块边界路水平且同高。夹具缺失=失败。
- presets：路网预设(2026-11-21)。`Tools/MapEditor/samples/presets.roadnet`(make_presets.js生成：方框路+一块地+4个预设，含子地块里的、一个id不存在的)，config里`zone_residence_high --skip`：zone只有3个预设的、矩形(Quad中心位置)和编辑器模拟器一致、`--skip`生效、无效id只告警不崩。
- transit：MapEditor"线路"工具手动设计的火车线路(2026-11-23)。`Tools/MapEditor/samples/transit.roadnet`(make_transit.js生成：方框路+一块地+2个火车站预设+n0->n1平地铁轨、n1->n2曲线桥+一个线路环[s0停,n0,n2,s1停])，config里`route_mods`换成只有`route_train --skip`：环规划成1条线2段(停靠站之间)、每段所有片段首尾相接(<1e-3)且接缝朝向突变<25°、n0处右手铁轨出发点偏中轴+0.25而左手铁轨到达点偏-0.25(两轨相距0.5)、桥面升到节点高度。fixture缺失=失败；`make_transit.js`改动后要重新生成并同步`suite_transit.cpp`里的坐标。另有两份(同一个站在环里出现两次，往返各停一次)：`transit_twice`(东西两侧各一条铁轨，铁轨原路掉头)、`transit_hub`(没有铁轨，两个孤立节点夹在站两侧，节点处掉头)：1条线2段、两段都从该站出发回到该站、两次停靠落在不同轨道。第四份 `transit_bus`(2026-11-23，MapEditor"路网路线"手动公交)：方框路上顺时针 4 个站成一个环，config 里 `route_mods` 是 `route_bus --skip`：4 个 `BusStop_L0_S*` 站(没有自动的棋盘格站)、都挂在路上、不压同侧车辆开口，1 条线 4 段、每段连两个不同的站。四份依次各跑一遍子进程。
- route：先检查行人图边全是双向、每个有导航节点的房间都接进了行人图(不在图里的房间作终点永远NO_PATH、作起点会退化成"取离原点最近的节点"凭空算出路线，正反向结果不一致；失败时`Note`列出前12个房间)；随机行人房间→房间、车辆车位→车位的寻路耗时分布(mean/p50/p90/p95/p99/max + 对数直方图)、平均路径点数、失败率(>5%判失败)，样本由世界种子派生可复现。行人路线缓存：同一批起终点冷缓存/同方向再跑/反方向跑三轮耗时，检查命中后结果与冷算逐点相同、反方向是正方向倒过来(端点互换、长度一致、edges首尾相接)且没有新条目和未命中，条目数/字节/每条字节，25游戏小时过期(24h还在、26h被清、刷新过的保留)；车辆寻路不碰行人缓存。
- 内存统计(所有Core套件)：`RecordMemory(label)`(`test_framework.h`)记"memory <label> private"(进程私有内存MB)和"delta"(比上一次记录的增量)。世界套件的子进程自动记每个生成阶段的内存(`stage.<阶段> memory`)、生成后/套件结束后的内存和峰值工作集；mods(单测前后)、world(连通校验后)、econ(每月末，单调增长=泄漏或无上限缓存)、save(一份存档/回环重建后)、perform(寻路/每日tick前后)、route(各缓存状态/车辆寻路前后)各自在关键点记。
- perform：各阶段耗时、路线规划 p50/p95/max、每日 tick 耗时，预算见 `budgets.json`(只告警)。
- world：多种子世界不变量(建筑/房间/车行与步行连通——飞机位不计/人口/账本/股份)；机场恰2个机位且各停一架飞机，港口恰1个船位且停一艘船，所有 stocked 车位(`ParkingSlotSpec::stocked`)开局都停了车。
- polit：政治域(2026-11-21)，长期行为(选举、政策演化、党代会、权力)全靠它验证，PIE只看第一天的党内选举。纯函数/目录检查；带两党的世界推进>=13个月(步长<=1小时覆盖每月1号12:00~15:00的党代会窗口)，模拟党员按月轮换的出席(全到/部分/没人；没有会议室的党线上召开，全体党员都算到场、通知地址为空、照常选出党首)，逐步检查选举时点、政策单次变化幅度<=10%且在[min,max]内、执政期数、执政党不开会、start/end通知成对、党首只能是到场者、权力只属于执政党党员、账本总和恒0；跳时间补发、党员死亡被移出、会议室重叠兜底(最多4次；这个世界里没有任何党拿到会议室时这一项跳过并记一条note)；完整政治日志写到`<out>/polit_logs/polit_seed<S>_<tag>.log`并解析日志交叉验证；同种子两次日志逐字节一致；没启用党派时政策值始终是默认值。世界/econ/save/perf套件也带两党跑。
- econ：每月经济报表(`econ_report.*` 的 `Reporter`)写到 `<out>/econ_digests/econ_report_seed<S>.txt`；分红、倒闭重启场景作为检查项。`Tools/EconSim` 已移除。
  用 Core `WorldStep`(与 UE StepWorld 同一份)按日推进，逐日账本守恒，同种子逐日摘要必须一致。
- save：存档幂等、容器序列化/截断/损坏不崩、人口/社会段读回再存、随机流恢复。

多种子/世界在子进程里运行(崩溃只算该种子失败，失败信息带 `--seed`)。

## UE 管线
`UnrealEditor-Cmd Forever.uproject /Game/Levels/World -game -nullrhi -unattended -ForeverTest=boot,perform [-ForeverTestPerfSeconds=N] [-ForeverTestOut=dir]`。`UForeverTestSubsystem` 只在带 `-ForeverTest=` 时创建，等 `AForeverFrameworkActor::IsInitialGenerationDone()`，跑套件，写 `ue_<suite>.json`，以失败数为退出码。模块在 `Forever.uproject`(Developer 类型)和两个 Target(Shipping 不带)里注册。
- boot：Core 各域生成、建筑/市民/车辆计数、账本为0、无漏包随机、各类 Actor 数、内存。
- components：每栋建筑一个 BuildingElement、流式市民数有界、方向光为 Movable、UE NavigationSystem 关闭、玩家有 Pawn、各 FrameworkComponent 齐全。
- saveload：SaveToFile → RequestLoad → 等新世界生成完，比对市民/建筑/车辆/组织数、账本为0、日期。
- commute：`BeginTimeSkip` 跳到 07:50(约 24 游戏小时，真实约 70 秒)，时间流速调到 60 倍实时跑过早高峰到 09:00，记录帧时间分布、异步寻路队列峰值、排空耗时、账本为0。必须带渲染运行：跳时间要等黑屏渐变 widget 的 Tick，`-nullrhi` 下渐变不会完成，跳时间会一直卡住(超时失败)。
- changes：对每种 Change 在主线程上同步 `ApplyChange` 并计时(每种 10 次，first=首次，avg/max=除首次外的平均/最大)，指标 `change.<type>.first_ms/max_ms/avg_ms/real_args`，检查"单个 Change 主线程不超过 100ms"。参数尽量用世界里取到的真实市民/房间/载具；`real_args=0` 的用占位值，走的是"找不到目标"的快速失败路径，耗时只代表入口开销；`real_args=1` 也不保证真正执行了业务(例如目标被拒绝时同样很快)。会改世界状态，建议单独跑(`-ForeverTest=changes`，需要完整世界，带窗口)。不测：game_end、jump_time(见 commute)、for_range(容器)。start_puzzle 用未启用的 id 只测"忽略并打日志"的路径。
- ride：把玩家放上飞机、火车(搭乘模式)各 PerfSeconds 秒，记帧时间、检查一直在车上、能下车；世界里没有可搭乘的该类载具只记指标。需要带渲染。
- perform：站立空闲帧时间 avg/p50/p95/p99/max、>33ms/>100ms 帧数(无渲染，只测游戏线程)。

## 打包测试
`run_tests.bat <档> package`(`full all` 会自动带上)：跑 `pak.bat`(BuildCookRun：编译+烘焙+暂存+pak，日志 `<run>\package\pak.log`)，检查 `Saved\Archive\Windows\Forever.exe` 和 `Forever-Windows.pak` 存在，再用打包出来的 `Forever.exe /Game/Levels/World -nullrhi -ForeverTest=boot,components` 无渲染跑一遍，最后带窗口跑 `traffic` 180 秒，退出码 = 失败数。很慢(烘焙)，所以 quick 档 `all` 不跑。注意：打包目标是非编辑器的 Game，编辑器专用头文件/API(如 `StaticMeshCompiler.h`)必须包在 `#if WITH_EDITOR` 里，否则只有打包才会暴露编译错误。

## traffic 套件(UE，带窗口)
`-ForeverTest=traffic [-ForeverTestTrafficSeconds=N]`：跳到早高峰后时间 30 倍速，玩家(步行)每 6 秒被传到一辆行驶中的车旁边，反复触发载具 Actor 的流式生成/销毁和 Chaos 物理，记帧时间、传送次数、场上载具 Actor 峰值。进程崩了由外层按退出码判失败。它复现并帮助修掉了两个问题：打包版 Chaos 悬挂崩溃(交通组件改到 `TG_PostPhysics` tick)、掉头(180°折返)让 `BuildPath` 圆角算出几百亿 uu 的点而卡死(`d` 夹紧)。world 套件另有 `CheckVehicleEdgeGeometry` / `CheckRouteLegGeometry` 检查车行边和线路站间规划点有限且连续，boot 套件有同样的线路检查。

## 已知发现
- 已修：存档校验 `Config::FindMissingScripts` 原来只看 resource_paths，story_scripts 里的脚本(如 Test mod 的 test.script)不在其中时存档读不回("missing script: test")；现在 story_scripts 里存在的脚本也算。UE saveload 套件已全过(读档 19.6s，存档 42MB)。
- 已修：`CitizenElement::ComputeRoomLandingSpot` 的落脚点抖动用了没包作用域的 GetRandom，现在包进 `ue.citizen_landing` 域。
- 注意：Core 是静态库，ForeverTest 模块链接的是自己那份 Core 静态状态(Config/RandomRegistry 等)，读 Forever 模块里的状态必须走 Forever 导出的函数(如 `AForeverFrameworkActor::GetUnscopedRandomDraws`)。
- 已修(测试侧)：机场的飞机位(category=plane)不接车行路网，world 套件只统计接受 car 的车位；原先“第一个车位恰是机场位”还会让参照点本身孤立，造成 quick 档 seed 13 的大面积假失败。
- 已修：full 档 seed 14 的车库单向——挂车库入口的小路一端是死胡同(没接任何路)，`ResolvePathEndAnchors` 给该端留下两个互不相连的孤立锚点，到达侧车道无出边、离开侧车道无入边。`Map::ConnectPathRoad` 现在在死端把到达侧接到离开侧(掉头)。full 档 4 个 seed 停车位全部双向连通。
- 已修：UE 带渲染空闲卡顿(最长约 1.05s)是首次用到楼梯/墙网格时游戏线程同步加载+等静态网格编译；`UForeverBuildingFrameworkComponent::PrewarmMeshes` 在生成完成时一次性加载并 FinishCompilation，空闲最大帧降到约 46ms，0 帧超 100ms。
- 观察：commute 的跳时间(傍晚→次日 07:50)真实耗时 150~300+ 秒：时间步进每步 1 游戏小时、单步里要排空 leave_work 等计时器(单个动作 10~17ms)，不能再切片，所以单帧很长；测试超时放宽到 900s，并记 `commute.skip_seconds`。
- 已修：`Traffic::BeginCommute`/任务分配里的 `PickFreeSlot` 由 UE 层运行时直接调用，没有随机作用域(每次通勤会算一次漏包抽取)，现在在函数里自己包 `core.traffic`。
- 顶层 `seed` 只决定 Core 各域，mod 流需要 `--seed` 才固定；测试自动给每个 mod 条目注入派生种子。
- `Traffic::Init` 必须在 `Checkin` 之前(UE 同序)，否则 `AssignVehicles` 崩溃。

- 发现：搭乘飞机 15 秒内最长帧约 667ms、p99 约 280ms(火车最长 121ms)，疑似登机/远景流式加载卡顿，未查。

- 发现(changes)：首轮实测 enter_vehicle 102ms(生成并附身载具)、create_timer 约 50ms、remove_timer 约 49ms、global_message 约 42ms，是主线程耗时最长的几个，其余都在 10ms 以内。create_timer/remove_timer 单次 50ms 这么大值得查；`leave_work` 等计时器每次执行 10~17ms 与此一致。
- 已修：脚本 `start_puzzle` 指向未注册/未启用的 puzzle id 原来会因 `Puzzle::Puzzle` 抛未捕获的异常(现在归为 DataException)崩游戏；现在 `UPuzzleWidget::StartPuzzle` 先创建新的这局并捕获，失败就打 Warning 日志并忽略(正在进行的上一局保留)。changes 套件用未启用的 id 覆盖这条路径(首次约 30ms，是首个 C++ 异常的开销)。

## 2026-11-25～26 这批改动里还没在 UE 里验证的
这两天的全项目 review 一共改了 25 轮，每轮只做了全量编译 + ModTest + CoreTest，**UE 管线一次没跑，编辑器/PIE 一次没开**。验证过一项就从这里删一项。
- 先跑 `run_tests.bat quick ue`，它能覆盖其中不需要人看的部分：
  - changes 套件：`Change` 改成按 `ChangeKind` 分派之后，UE 层 `ApplyChange` 的 37 个 `case` 没有在游戏里执行过。
  - saveload 套件：读档会销毁并重建世界，走一遍"各域销毁 → 注销并卸载 mod dll → 重新加载"。跑完在 `Forever.log` 里搜 `still alive after the game ended`，出现就说明有对象没跟着一局结束销毁、对应的 dll 没被卸载。
  - 任意套件的 `Forever.log` 里应该有 `LogForeverCore` 的行，Core 和 mod 里打的日志(比如 Basic 生成地形时的那几条)都在里面。
- 要进游戏看的：
  - "进游戏 → 退回菜单 → 再进游戏"多试几轮；在菜单界面替换一个 mod 的 dll 再开局。
  - 开局装备现在由主线脚本用 `grant_asset` 发(`Forever_Mod/Test/Resource/test.script`)：左肩手枪、右肩步枪、背包里的东西是否和原来一样，读档后不会重复发。
  - 手机桌面：`showOnDesktop=false` 的应用不出现，其它应用照常。
  - 线路调试线(颜色改成按站点类型名哈希)能否分得开；轿车旁边"上车"选项的触发范围(触发盒改成按车身包围盒外扩)是否合适。
  - 没调 `SetMaterial` 的地形用平原那套贴图显示是否正常。
  - 游戏时钟的速度不随帧率变。
  - 改了脚本文件后重开 PIE 能生效；连开两局时市民名字从头编；存档覆盖同名存档。

## 待做
- 车辆流式压测、带画面 GPU 采样。
- 字段检查(组织/路线/房间/地形/路网等)；已做：车辆、武器、产品、仓储、制造、岗位、建筑、站点、房间、调度、资产、地形(组织/路线/路网/地块/脚本/名字/组件/应用/谜题没有可校验的静态字段，靠 UnitTest 和 world 套件覆盖)。

## 字段检查(ModTest，`Mod/mod_checks.cpp`)
车辆(尺寸/载客量/蓝图)、武器(伤害/弹匣/射程，不开火的工具跳过)、产品(价格 retail>import>export>0、原料数量)、仓储(容量/能存能取/需求值)、制造(目标批数)、岗位(脚本 mod 已注册)、建筑(国有概率、脚本/车站 mod 已注册、电梯楼层范围)、站点(类型/端口，公交站贴路无端口)、房间(容量/租金/引用的 mod 已注册)、调度、资产(重量体积/武器类型/引用已注册)、地形(SetupTexture 后声明了材质或地图色)。占位模板按提供它的 dll(`Empty.dll`)识别，个别检查对它放宽。其余 concept 靠 UnitTest 与 world 套件覆盖。

## 引用和依赖(ModTest)
ModTest 把各 concept 字段里写到的别的 mod 的 id(脚本、房间/组件、站点、仓库/工坊、武器/产品/弹药、载具、岗位、内部建筑、配方)收集起来，核对每个都有 dll 提供；对每个 dll 的依赖声明查"被依赖的 id 存在"和"不成环"。不查依赖声明是否完整(引用方向不等于依赖方向，谁依赖谁按语义由作者写)。范围见 `Mod/ModTest.md`。
