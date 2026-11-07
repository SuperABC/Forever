# product_mod.h / product_mod.cpp

## 职责

`ProductMod`：一种产品的**类型定义**(不是某个仓库里的一份库存，库存数据在`Storage`自己的存量表里)。

## 关键设计

- **分类标签`categories`**决定哪些仓库能存它：和`StorageMod::categories`同一套字符串标签，任一交集即可入库。
- **批次配方**：`batchSize`是"一批最少产多少份"，`ingredients`/`byproducts`的value含义是"生产一批(`batchSize`份)需要/产生多少"，不是"每1份"。`batchSize`不要求是整数，但工坊每天配置的产量必须是整批(`batchesPerDay`为正整数，见`manufacture_mod.h`)。
- **`SetProperty()`两段式**：具体子类在这里填四个字段，不在构造函数里赋值。
- 带存档钩子，默认不存任何东西，见`common/mod_state.md`。
- `retailPrice`/`importPrice`/`exportPrice`：int64、单位分，要求 零售价 > 进口价 > 出口价 > 0；`Core::Product`构造时校验，不满足则判无效。零售价是商店卖给市民的单价，进口价/出口价是与城市外交易的价格，城市内批发价由Core初始化为二者平均值（不是mod字段）。
- `serviceType`/`serviceQuality`：市民可直接消费时的服务类型和质量；`serviceType`为空表示暂不开放市民直接购买（小麦/牛肉）。

## 依赖关系

- 依赖：`common/mod_state.h`。
- 被谁依赖：`product_factory.h`创建它；`Core/industry/product.h`/`manufacture.h`消费它；具体实现在`Source/Basic/industry/product_basic.*`和外部mod里。

## 待办/后续阶段

- 无。
