# Core 概览

## 职责

`Source/Core`是和UE无关的纯C++静态库，持有一局游戏里全部domain的领域系统类（世界数据+规则），消费
`Source/Dependence`提供的`<Concept>Mod`/`<Concept>Factory`可扩展点。**Core只处理所有mod的共性**：
它通过`<Concept>Mod`基类的字段/虚方法和Factory注册的静态函数指针与mod打交道，不应该认识任何具体mod的
id、类型名或它的业务含义。

## 目录

| 目录 | 内容 | 入口文档 |
|---|---|---|
| `common/` | `config`(读写`config.json`、扫描mod dll/资源目录)、`loader`(`ModLoader`：`LoadLibrary`并调用dll导出的注册符号)、`registry`(`Registry`全局单例，持有23个concept的Factory)、`implement`(`PostImplement`：mod通过`PostHandle`向Core查询的实现)、`world_step`(世界按时间推进一步的唯一实现)、`save`/`save_refs`(存档编排与跨域引用编码)、`class.h`(前向声明汇总) | `common/*.md` |
| `map/` | `Map`(地形网格、导航图、占位/落地流程)、`Terrain`、`Roadnet`、`Zone`、`Green`、`Building`/`Component`/`Room`、`Door`、`Cabin`(电梯)、`Vegetation`、`Lamps`、桥墩(`pier`)、`terrain_coverage`、`MapSaveCodec` | `map/map.md` |
| `populace/` | `Populace`、`Citizen`、`Household`、`Name`、`Scheduler`、学历/班级(`experience`/`school`)、存档 | `populace/populace.md` |
| `society/` | `Society`、`Organization`、`Job`、账本(`ledger`)、经济日结/月结(`economy`)、结算(`settlement`)、股市(`market`)、投资(`investor`)、国库(`treasury`)、迁入/离城、存档 | `society/society.md` |
| `polit/` | `Polit`、`Party`、可调政策(`policy`)、权力词汇(`power`)、选举、党代会、政治日志、存档 | `polit/polit.md` |
| `story/` | `Story`、`Script`、`Milestone`、脚本存档 | `story/story.md`、`story/script.md` |
| `industry/` | `Industry`、`Product`、`Storage`、`Manufacture`、存档 | `industry/industry.md` |
| `traffic/` | `Traffic`、`Vehicle`、`Station`、`Route`、路口通行(`junction`)、存档 | `traffic/traffic.md` |
| `player/` | `Player`(时钟、背包槽位、武器)、`Asset`/`EstateAsset`/`VehicleAsset`、`Phone`/`App`、`Puzzle`、`Weapon`、`Weather`、存档 | `player/player.md` |

## 关键设计

- **统一的mod持有模式**：领域本体类构造时`factory->Create<Concept>(id)`拿一个`<Concept>Mod`实例并独占持有，
  析构时`factory->Destroy<Concept>(mod)`(走mod注册的deleter，见`CONVENTIONS.md`第4节)，`GetMod()`把实例暴露
  给外部只读使用。mod输出的容器Core只读、只拷贝，不就地修改。
- **`<Concept>Factory`放在`Source/Dependence`，不放在`Core`**——Mod DLL只链接`Dependence.lib`。Factory实例
  由`Registry`单例持有(进程内唯一)，各域管理类在构造时通过`Registry::Get().Get<Concept>Factory()`取引用。
  完整concept对照表见`Source/Dependence/README.md`。
- **"启用"=已注册且在`config.json`对应的`<concept>_mods`数组里**：`Registry::ReloadModArgs()`把数组内容灌给各
  Factory，没启用的id`Create<Concept>`返回空，领域类把它当成"构造失败/这个mod不存在"处理。
- **世界推进只有一份实现**：`WorldStep::Step`按固定顺序推进各域，UE层(`AForeverFrameworkActor::StepWorld`)
  和测试程序(`Source/Test/Core`)共用；Core看不到的部分(应用脚本Change、Actor增删)由调用方通过
  `WorldStepHooks`注入。
- **随机数可复现**：Core各域在`CORE_RANDOM_SCOPE("core.<domain>")`作用域里取随机数，mod实例在自己的流里
  (`RandomRegistry`)，种子来自`config.json`的顶层`seed`和每个mod的`--seed`，见`Dependence/common/random.md`。
- **存档**：每个域一个`<Domain>SaveCodec`(各自的`*_save.h/.cpp`)，由`common/save`编排成分段文件，见
  `common/save.md`。

## 依赖关系

- 依赖：`Source/Dependence`(mod接口、Factory、几何/脚本原语、JSON、随机、存档归档)。
- 被谁依赖：`Source/Forever`(UE模块，`AForeverFrameworkActor`创建并驱动各域，各`UForever*FrameworkComponent`
  把域数据翻译成UE表现)、`Source/Test/Core`(CoreTest，不带UE直接跑Core)、`Tools/ModHost`等工具。
- Core不依赖`Source/Basic`和任何`Forever_Mod`——它们只在运行时以dll形式被`ModLoader`加载。

## 待办/后续阶段

- Core里还残留一些对具体mod的认识，应逐步改成"由mod声明、Core只读共性数据"的形式。已知位置：
  - `society/economy.cpp`：市民每日需求的服务类型`"food"`/`"video"`和视频需求的财富阈值是写死的。
  - `traffic/traffic.cpp`的`AssignVehicles`：市民分`"car"`、组织分`"truck"`两个载具种类写死，成年年龄用自己的
    常量(`kAdultAge`)而不是`GovernmentPolicy::adultAge`；`traffic/vehicle.cpp`：查不到种类时兜底成`"car"`。
  - 按名字特判占位mod：`station_empty`/`route_empty`(`traffic/traffic.cpp`)、`app_empty`(`player/phone.cpp`)；
    实体脚本没指定ScriptMod时兜底用`script_empty`(`map/`下各实体、`story/script.cpp`的`MakeScript`)，因此
    `script_empty`必须启用。
  - `common/config.cpp`的`GetMainStoryScriptPath`：没配`story_scripts`时退回名为`test`的脚本。
  - `map/map.cpp`的`GetTerrainAppearances`：没有mod的默认地形`plain`，其UE材质路径写在Core里。
