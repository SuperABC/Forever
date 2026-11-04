# product_basic.h / product_basic.cpp

## 职责

工业系统测试案例的三个产品：`WheatProduct`(小麦)、`BeefProduct`(牛肉)、`BurgerProduct`(汉堡)。

## 关键设计

- `SetProperty()`里填`categories`/`batchSize`/`ingredients`：小麦(`grain`，一批10份，无原料)、牛肉(`meat`，一批10份，无原料)、汉堡(`food`，一批5份，需要3份小麦+2份牛肉)。
- 用来验证`Manufacture::ComputeRecipe()`的批次配方展开算法和`Industry::GlobalAllocate()`的全城统一调配，见`Core/industry/industry.md`。具体数值是占位测试值。
- `categories`和`storage_basic.md`里三个仓库的`categories`一一对应(`grain`/`meat`/`food`)。

## 依赖关系

- 依赖：`industry/product_mod.h`。
- 被谁依赖：`basic.cpp`注册；`manufacture_basic.*`的`targets`按id引用；`Core/industry/product.h`消费。

## 待办/后续阶段

- 数值是占位测试值，按实际存量变化速度调整。

## 价格（分）
小麦 零售800/进口600/出口400；牛肉 1600/1200/800；汉堡 5000/3000/2000，汉堡`serviceType=food`、`serviceQuality=0.1`。小麦/牛肉暂不开放市民直接购买。
