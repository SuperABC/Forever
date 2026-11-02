# manufacture_mod.h / manufacture_mod.cpp

## 职责

`ManufactureMod`：工坊(实际生产东西的单位)的类型定义。"工厂"这个叫法在本项目里专指"mod创建/销毁管理器"(`XxxFactory`)，容易混淆，所以这个概念中文统一叫"工坊"，类名`Manufacture`不变。

## 关键设计

- **配方写在`ProductMod`身上**，工坊mod只声明"每天要产哪些产品、各产几批"：`targets`(产品type -> 每天目标产几批)，value必须是正整数，含义是"这个产品`batchSize`的多少倍"，不是绝对数量。可以同时配置多个目标产品。
- **`SetTargets()`两段式**：具体子类在`SetTargets()`里填`targets`，不在构造函数里赋值；构造函数只负责id/count这类登记，Core创建完mod实例后立刻调一次。和`ProductMod`/`StorageMod`同一套约定。
- Core层`Manufacture::SetProperty()`把`targets`按"副产物覆盖抵消+配方递归展开"算法转换成"每天净需要多少外部原料"，见`Core/industry/manufacture.md`。
- 带存档钩子(`SaveState`/`LoadState`/`GetStateVersion`/命名计数器)，默认不存任何东西，见`common/mod_state.md`。

## 依赖关系

- 依赖：`common/mod_state.h`。
- 被谁依赖：`manufacture_factory.h`创建它；`Core/industry/manufacture.h`消费它；具体实现在`Source/Basic/industry/manufacture_basic.*`和外部mod里。

## 待办/后续阶段

- 无。
