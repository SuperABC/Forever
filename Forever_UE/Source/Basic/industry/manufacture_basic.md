# manufacture_basic.h / manufacture_basic.cpp

## 职责

工业系统测试案例的三个工坊：`FarmManufacture`(农场，无原料产小麦)、`RanchManufacture`(牧场，无原料产牛肉)、`FoodFactoryManufacture`(食品加工厂，小麦+牛肉产汉堡)。

## 关键设计

- 只在`SetTargets()`里声明`targets`：农场`product_wheat` x5批、牧场`product_beef` x5批、食品加工厂`product_burger` x20批；配方(原料、批大小)写在`ProductMod`身上，见`product_basic.md`。
- 用来验证`Industry::Tick()`每日三阶段(`WorkAccount` -> `GlobalAllocate` -> `StartProduce`)，见`Core/industry/industry.md`。`targets`的value(每天产几批)是占位测试值。
- `GetName()`必须全局唯一：static计数器+id+`GetName`里现拼，见`CONVENTIONS.md`。

## 依赖关系

- 依赖：`industry/manufacture_mod.h`。
- 被谁依赖：`basic.cpp`注册；`Core/industry/manufacture.h`消费。

## 待办/后续阶段

- `targets`的批数是占位测试值，跑通后按实际存量变化速度调整。

## 日产能
单个工坊满负荷日产能：小麦厂200份（20批×10）、牛肉厂200份（20批×10）、汉堡厂500份（100批×5，日耗小麦300、牛肉200）。
