# product_burger.h / product_burger.cpp

## 职责

`BurgerProduct`(`product_burger`)：product域的默认实现之一。

## 关键设计

- 构造函数只负责登记id；`categories`/`batchSize`/`ingredients`/价格都在各自的`SetProperty()`里填：小麦(`grain`，一批10份，无原料)、牛肉(`meat`，一批10份，无原料)、汉堡(`food`，一批5份，需要3份小麦+2份牛肉)。
- 价格(分)，零售>进口>出口：小麦800/600/400；牛肉1600/1200/800；汉堡5000/3000/2000。
- 汉堡`serviceType=food`、`serviceQuality=0.1`(纯预制菜)；小麦/牛肉暂不开放市民直接购买，`serviceType`留空。
- 验证`Manufacture::ComputeRecipe()`的批次配方展开算法和`Industry::GlobalAllocate()`的全城统一调配，见`Core/industry/industry.md`。
- `categories`和`storage_*.md`里三个仓库的`categories`一一对应(`grain`/`meat`/`food`)。

## 依赖关系

- 依赖：`industry/product_mod.h`。
- 被谁依赖：`basic.cpp`注册；`manufacture_*.*`的`targets`按id引用；`Core/industry/product.h`消费。

## 待办/后续阶段

- 批大小/用量/价格是占位测试值，按实际存量变化速度调整。
