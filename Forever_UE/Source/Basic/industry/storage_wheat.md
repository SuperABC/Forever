# storage_wheat.h / storage_wheat.cpp

## 职责

`WheatStorage`(`storage_wheat`)：storage域的默认实现之一。

## 关键设计

- `SetProperty()`里填`categories`和`capacity`；`categories`分别对应`product_*.md`里三个产品的`categories`，任一交集即可入库。
- `capacity`是共享容量池，不是每种产品各自的配额，见`Dependence/industry/storage_mod.md`。
- `GetName()`全局唯一(static计数器+id+现拼)，计数器存档见`Dependence/common/mod_state.md`。

## 依赖关系

- 依赖：`industry/storage_mod.h`。

## 待办/后续阶段

- `capacity`是占位测试值，按实际存量变化速度调整。
