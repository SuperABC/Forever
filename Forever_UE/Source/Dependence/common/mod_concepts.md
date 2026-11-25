# mod_concepts.h / mod_concepts.cpp

## 职责

宿主和mod dll之间的**导出符号约定**：23个concept各自的三个导出函数名(`GetMod<Concepts>`/`RegisterMod<Concepts>`/
`FinishMod<Concepts>`)、每个dll一份的依赖声明符号`GetModDependencies`，以及concept名到`config.json`里key的换算
(`ModConceptJsonKey`：`"Terrains"`->`"terrain_mods"`)。

## 关键设计

- **放在Dependence而不是Core**：这张表是"mod dll必须导出什么"的约定本身，所有加载mod的宿主都要用——游戏里是Core的
  `ModLoader`/`Config`，mod作者手里是只链接`Dependence.lib`的`ModTest`(`Source/Test/Mod`)。放在Core的话，没有Core的
  ModTest就得自己再抄一份。
- **`GetModConceptDescriptors()`是这份约定唯一的权威列表**，顺序即配置界面Mod screen按钮的显示顺序。新增concept时
  在这里加一行，再去改各个按concept展开的地方(`Core/common/registry.*`、`Core/common/save.cpp`、
  `Test/Mod/mod_checks.cpp`)。
- 表里只有符号名字符串，不涉及任何`<Concept>Factory`类型——调用方自己把`GetProcAddress`拿到的指针转成
  `void(*)(<Concept>Factory*)`。

## 依赖关系

- 依赖：只用标准库。
- 被谁依赖：`Core/common/loader.h`(转include，`ModLoader`和`Config`沿用原来的名字)、`Core/common/config.cpp`
  (`ConceptKeyToJsonKey`转调`ModConceptJsonKey`)、`Test/Mod`(ModTest自己的dll加载)。

## 待办/后续阶段

- 无。
