# Core 概览

## 职责

`Source/Core`是和UE无关的纯C++静态库，持有各domain的真实领域系统类，消费
`Source/Dependence`提供的`<Concept>Mod`/`<Concept>Factory`可扩展点：

- `common/config.h/.cpp`：读取`config.json`+扫描`dll_paths`目录，详见`common/config.md`。
- `common/loader.h/.cpp`：按concept把`Config`发现的合法mod dll`LoadLibrary`并注册进具体的
  `<Concept>Factory`，详见`common/loader.md`。
- `map/`下`Terrain`/`Roadnet`/`Zone`/`Building`已有真正的领域本体类，仿照统一的持有模式：
  构造时`factory->Create<Concept>(id)`拿一个`<Concept>Mod`实例并独占持有，析构时
  `factory->Destroy<Concept>(mod)`，`GetMod()`把这个实例暴露给外部调用方直接读。各自的
  业务字段/方法见`map/*.md`。
- 其余domain目前还没有Core层本体类，只有`Source/Dependence`的`<Concept>Mod`/
  `<Concept>Factory`，等迁移到对应域时再按`map/`已验证的模式补上。

## 关键设计

- **`<Concept>Factory`放在`Source/Dependence`，不放在`Core`**——Mod DLL只link
  `Dependence.lib`，`Factory`的方法实现如果编译进`Core.lib`，这个非虚方法在Mod DLL里根本
  链接不到。完整对照表见`Source/Dependence/README.md`。

## 依赖关系

- 依赖：`Source/Dependence`(`config.cpp`用`common/json.h`解析JSON、`loader.h`的
  `RegisterConcept`模板方法接受任意`<Concept>Factory*`，不需要知道具体是哪个)。
- 被谁依赖：`Source/Forever/Mod/ForeverModSubsystem.cpp`(构造`ModLoader`实例、调用
  `Config::ReadConfig`/`Config::GetMods`/`ModLoader::RegisterConcept`)。

## 待办/后续阶段

- `Core`会按迁移进度逐步为其余domain补齐真正的领域系统类(如`Map`/`Story`)，取代目前由
  `ForeverModSubsystem`临时代管Factory的做法。
