# Basic 概览

## 职责

`Source/Basic`是内置默认Mod集合，为`Source/Dependence`每个`<Concept>Mod`接口提供随引擎自带
的默认实现(`<Concept>Basic`系列类)。编译产出`Basic.dll`，和`Forever_Mod/`下的第三方Mod地位
完全相同，由`Config`/`ModLoader`在运行时按导出符号加载，不被`Forever.Build.cs`静态链接。

## 关键设计

- `Basic.vcxproj`是`DynamicLibrary`，输出目录和`Dependence.lib`/`Core.lib`相同
  (`Forever_UE/x64/<Config>/`)，`IncludePath`/链接只依赖`Dependence`，不依赖`Core`——这是
  Basic作为"Mod DLL"定位的必然要求，和`CONVENTIONS.md`"Mod只link Dependence.lib"的规则对齐。
- 根目录`Basic.cpp`为每个concept导出`GetMod<Concept>`/`RegisterMod<Concept>`/
  `FinishMod<Concept>`三个符号，按`XxxBasic::GetId()`注册具体实现。
- `Forever_UE/Resource/Config/config.json`的`dll_paths`需要包含Basic的输出目录，
  `Config::AddDllPath`才会扫描到`Basic.dll`。

## 依赖关系

- 依赖：`Source/Dependence/<domain>/<concept>_mod.h`(继承对应`<Concept>Mod`)、
  `Source/Dependence/<domain>/<concept>_factory.h`(`Basic.cpp`的`RegisterMod<Concept>`要用)。
- 被谁依赖：不被任何代码静态`#include`/链接——`Basic.dll`只在运行时被`Config`/`ModLoader`
  按符号名发现、加载，和`Forever_Mod/Test`、`Forever_Mod/Wxdj`处于同一层级。

## 待办/后续阶段

- 确认Mod和Basic在同一个Factory里注册时id冲突如何处理(目前`building_mods`等配置数组和
  Basic各自用独立的id空间，还没出现真正的冲突场景)。
