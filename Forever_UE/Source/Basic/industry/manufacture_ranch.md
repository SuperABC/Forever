# manufacture_ranch.h / manufacture_ranch.cpp

## 职责

`RanchManufacture`(`manufacture_ranch`)：manufacture域的默认实现之一。

## 关键设计

- 只在`SetTargets()`里声明`targets`(产品id -> 每天产几批)：农场`product_wheat` x20批、牧场`product_beef` x20批、食品加工厂`product_burger` x100批；配方(原料、批大小)写在`ProductMod`身上，见`product_*.md`。
- 单个工坊满负荷日产能：小麦200份(20批x10)、牛肉200份(20批x10)、汉堡500份(100批x5，日耗小麦300、牛肉200)。
- 验证`Industry::Tick()`每日三阶段(`WorkAccount` -> `GlobalAllocate` -> `StartProduce`)，见`Core/industry/industry.md`。
- `GetName()`全局唯一：static计数器+id+`GetName`里现拼，见`CONVENTIONS.md`；`GetInstanceCounter`/`SetInstanceCounter`供存档保存/恢复该计数器，见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`industry/manufacture_mod.h`。
- 被谁依赖：`basic.cpp`注册；`Core/industry/manufacture.h`消费。

## 待办/后续阶段

- `targets`的批数是占位测试值，跑通后按实际存量变化速度调整。
