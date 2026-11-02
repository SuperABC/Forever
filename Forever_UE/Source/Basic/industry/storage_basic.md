# storage_basic.h / storage_basic.cpp

## 职责

工业系统测试案例的三个仓库：`WheatStorage`(`grain`，容量200)、`BeefStorage`(`meat`，容量200)、`BurgerStorage`(`food`，容量100)。

## 关键设计

- `SetProperty()`里填`categories`和`capacity`；`categories`分别对应`product_basic.md`里三个产品的`categories`，任一交集即可入库。
- `capacity`是共享容量池，不是每种产品各自的配额，见`Dependence/industry/storage_mod.md`。数值是占位测试值。

## 依赖关系

- 依赖：`industry/storage_mod.h`。
- 被谁依赖：`basic.cpp`注册；`Core/industry/storage.h`消费。

## 待办/后续阶段

- `capacity`是占位测试值，按实际存量变化速度调整。
