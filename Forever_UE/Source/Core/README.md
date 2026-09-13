# Core 阶段3骨架总览

## 职责

阶段3的`Core`只有两组文件,不再有任何per-concept子目录(`industry/map/player/populace/
society/story/traffic`域文件夹本阶段全部搬去了`Source/Dependence`,详情见下方"实现期修正"),
`map/`下的`Terrain`/`Roadnet`/`Zone`/`Building`四个是唯二已经真正迁移的概念本体例外:

- `common/config.h/.cpp`:移植并裁剪自旧工程的`Config`类,负责读取`config.json`+扫描
  `dll_paths`目录,详见`common/config.md`。
- `common/loader.h/.cpp`:按concept把`Config`发现的合法mod dll真正`LoadLibrary`并
  注册进具体的`<Concept>Factory`,详见`common/loader.md`。
- **已迁移的概念本体（`Terrain`/`Roadnet`/`Zone`/`Building`，均在`map/`）都仿照老工程的
  持有模式：构造时`factory->Create<Concept>(id)`拿一个`<Concept>Mod`实例并独占持有，析构时
  `factory->Destroy<Concept>(mod)`，新增`GetMod()`把这个实例暴露给外部调用方直接读（不需要
  另外拷贝一份），各自的业务字段/方法见`map/*.md`**。`Building`中间曾经有一轮"mod按类型共享"
  的例外设计（`candidateWeights`/`RandomAcreage`机制当时天然需要按类型共享一个实例），这次
  会话把这些查询本身改成了不需要任何实例的static方法（通过`Factory`的额外函数指针参数注册，
  见`Source/Dependence/README.md`/`map/zone_mod.md`），共享模型不再必要，`Building`改回和
  `Terrain`/`Roadnet`/`Zone`完全一样的"一个本体独占一个mod、构造时创建、析构时销毁"默认
  模型，不再是例外。其余17个concept还没有Core层的本体类（只有`Source/Dependence`的
  `<Concept>Mod`/`<Concept>Factory`），等阶段4迁移到对应域时再按这个已验证过的模式补上，
  不在这之前提前搭骨架。

## 实现期修正:`<Concept>Factory`不在这里

原计划把`<Concept>Factory`(21个)放在`Source/Core/<domain>/`,和`Source/Dependence`的
`<Concept>Mod`分层。实现过程中发现这会导致Mod DLL无法调用`Factory`的方法——
`CONVENTIONS.md`要求Mod只link `Dependence.lib`,但`Factory`的方法实现如果编译进
`Core.lib`,这个非虚方法在Mod DLL里根本链接不到。核对旧工程后确认`BuildingFactory`本来
就和`BuildingMod`定义在同一个Dependence文件里,新工程已经按此修正——`<Concept>Factory`
全部挪回了`Source/Dependence/<domain>/`,和对应`<Concept>Mod`同目录。完整对照表见
`Source/Dependence/README.md`。

## 依赖关系

- 依赖:`Source/Dependence`(`config.cpp`用`common/json.h`解析JSON、`loader.h`的
  `RegisterConcept`模板方法接受任意`<Concept>Factory*`,不需要知道具体是哪个)。
- 被谁依赖:`Source/Forever/Mod/ForeverModSubsystem.cpp`(构造`ModLoader`实例、调用
  `Config::ReadConfig`/`Config::GetMods`/`ModLoader::RegisterConcept`)。

## 待办/后续阶段

- 阶段4:按系统迁移进度,`Core`会开始出现真正的领域系统类(如`Map`/`Story`),这些类会持有
  对应的`<Concept>Factory`实例并调用`ModLoader::RegisterConcept`,取代目前由
  `ForeverModSubsystem`临时代管两个Factory的做法。
- 阶段4:`Config`目前裁剪掉的部分(启用禁用状态、资源目录扫描、全局设置、剧情路径、运行时
  写回)按需加回,详见`common/config.md`。
