# Source/Test

自动化测试管线。Core 无画面管线(`Core/`)和 UE 管线(`UE/`，模块 `ForeverTest`，有 boot/components/saveload(无渲染)和 perform/commute/ride/changes(perform、commute、ride 带窗口渲染)套件)。

## 用法
```
Source\Test\run_tests.bat [quick|full] [core|ue|all]
```
或直接 `x64\Release\CoreTest.exe Resource\Config\config.json --suite random,mods,lint,world,econ,save,perform,route,green|all [--seed S|--seeds N] [--size N] [--months N] [--quick|--full] [--out dir]`。
先编译：`MSBuild Source\Framework.sln /p:Configuration=Release /p:Platform=x64`(必须编整个 sln：只编 `Source/Test/Core/Test.vcxproj`(输出仍是 `CoreTest.exe`)不会重编 Core.lib，改了 Core 会测到旧代码)。改了 `Dependence` 头文件要把 `Forever_Mod` 下 Empty/Test/Wxdj/Yuanshen 也各自重编，lint 会检查 DLL 是否比头文件旧。UE 代码用 UBT 编 `ForeverEditor Win64 Development`。

## 套件
- random：同种子跨进程所有阶段严格一致、各阶段无未包作用域的随机、DLL 导出 BindRandomHost、源码里无绕过 GetRandom 的随机源、流状态快照恢复。
- mods：枚举所有已注册 mod，调 `UnitTest`，检查无漏包随机、名字唯一(`*_empty` 模板除外)，并按 concept 做字段合法性检查(见文末"字段检查")。
- lint：mod DLL 不旧于 Dependence 头文件。
- green：绿化区(2026-11-20)。纯函数(`polygon_util`)、直接构造`green_park`测凹多边形圆/连路圆校验丢弃，再用`Tools/MapEditor/samples/demo.roadnet`(一块连路、一块不连路的凹多边形绿化)+`demo.terrain`替换config里的路网/地形生成世界：圆在多边形内且互不重叠、装饰物在多边形内、`LocateGreen`地址往返、房间↔圆的行人路线、地图存档往返字节稳定、市民站在/走向绿化区的存档往返和Core行进到达。demo文件缺失=失败；`make_demo.js`改动后要重新生成。
- route：随机行人房间→房间、车辆车位→车位的寻路耗时分布(mean/p50/p90/p95/p99/max + 对数直方图)、平均路径点数、失败率(>5%判失败)，样本由世界种子派生可复现。
- perform：各阶段耗时、路线规划 p50/p95/max、每日 tick 耗时，预算见 `budgets.json`(只告警)。
- world：多种子世界不变量(建筑/房间/车行与步行连通——飞机位不计/人口/账本/股份)。
- econ：每月经济报表(原 EconSim 的 Reporter，`econ_report.*`)写到 `<out>/econ_digests/econ_report_seed<S>.txt`；分红、倒闭重启场景作为检查项。`Tools/EconSim` 已移除。
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
- 已修：脚本 `start_puzzle` 指向未注册/未启用的 puzzle id 原来会因 `Puzzle::Puzzle` 抛未捕获的 NullPointerException 崩游戏；现在 `UPuzzleWidget::StartPuzzle` 先创建新的这局并捕获，失败就打 Warning 日志并忽略(正在进行的上一局保留)。changes 套件用未启用的 id 覆盖这条路径(首次约 30ms，是首个 C++ 异常的开销)。

## 待做
- 车辆流式压测、带画面 GPU 采样。
- 字段检查(组织/路线/房间/地形/路网等)；已做：车辆、武器、产品、仓储、制造、岗位、建筑、站点、房间、调度、资产、地形(组织/路线/路网/地块/脚本/名字/组件/应用/谜题没有可校验的静态字段，靠 UnitTest 和 world 套件覆盖)。

## 字段检查(mods 套件)
车辆(尺寸/载客量/蓝图)、武器(伤害/弹匣/射程，不开火的工具跳过)、产品(价格 retail>import>export>0、原料数量)、仓储(容量/能存能取/需求值)、制造(目标批数)、岗位(脚本 mod 已注册)、建筑(国有概率、脚本/车站 mod 已注册、电梯楼层范围)、站点(类型/端口，公交站贴路无端口)、房间(容量/租金/引用的 mod 已注册)、调度、资产(重量体积/武器类型/引用已注册)、地形(SetupTexture 后有贴图路径)。`*_empty` 占位模板按 id 跳过。其余 concept 靠 UnitTest 与 world 套件覆盖。
