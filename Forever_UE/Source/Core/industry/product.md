# product.h / product.cpp

## 职责

`Product`：一种产品的**类型目录条目**，独占持有一个`ProductMod*`——照抄`Vehicle`
持有`VehicleMod`的模式（见`traffic/vehicle.h/.md`），构造函数传入`ProductFactory*`+id，
内部调`factory->CreateProduct(id)`拿到`ProductMod*`，析构时
`factory->DestroyProduct(mod)`。**`Product`由`Industry`按产品type创建、持有一份，作为
全局目录**（`unordered_map<string, Product*>`，key=type，见`industry.md`）。

**和老工程的关键区别**：老工程的`Product`身兼"类型定义(配方/分类)"和"某个仓库里的
一份库存(数量)"两个角色；这次拆开——`Product`只是纯粹的配方/分类定义，不存任何数量，
"当前存了多少"这类库存数据全部放在`Storage`自己的存量表里（见`storage.md`）。这样
一种产品在全城只需要一份`Product`实例（Industry的目录），不需要每个仓库各自创建一份
`Product`对象来存量。

## 批次配方：`batchSize`/`ingredients`/`byproducts`

配方直接内嵌在`Product`本身，不是独立的Recipe类——照抄老工程的设计，但把"单位配方"
改成了"**批次配方**"：

- `batchSize`（对应产品设计里的"x"）：一批最少产多少份。
- `ingredients`：`unordered_map<type, 数量>`，生产**一批**（即`batchSize`份）需要
  多少某种原料。
- `byproducts`：同上，生产一批会额外产生多少某种副产品。

老工程的配方是"单位配方"（数值代表"每1份"），工坊(`Manufacture`)的目标产量是连续
浮点数，靠"效率"连续插值算当天实际产量。这次工坊的目标产量改成"每天产几批"
（正整数，见`manufacture_mod.h`的`targets`字段），当天实际产量是"能凑出的最大整数
批次数"，不是连续效率——`batchSize`不要求是整数（"一批"可以是任意正数份），但"批次数"
必须是整数，这是这次和老工程最大的语义差异，具体的批次展开/抵消算法见
`manufacture.md`。

## 分类标签：`categories`

`categories`是一个字符串数组，不是专门的Category类——和`Storage::categories`
（见`storage.md`）比较时只要有任一字符串相等就算匹配，仓库/工坊的输入cache据此判断
"能不能收这个产品"。分类匹配只在`Industry`的全城统一调配阶段用到，`Product`自己不做
任何比较逻辑，只是转发`categories`数据。

## 创建失败：`IsValid()`

`Product`构造函数不保证`mod`非空——`id`对应的产品没有被注册，或者注册了但没有在
`config.json`的`"product_mods"`数组里启用，`CreateProduct`都会返回`nullptr`。
`Product::IsValid()`供`Industry::CreateProduct()`创建完之后立刻检查，失败就整个
丢弃这个`Product`，不会把半成品塞进目录，见`industry.md`。

## 依赖关系

- 依赖：`Dependence/industry/product_mod.h`/`product_factory.h`（`ProductMod`/
  `ProductFactory`）。
- 被谁依赖：`Industry`（持有所有权，`CreateProduct`，按type查目录供`Storage`/
  `Manufacture`用）、`Manufacture`（配方展开算法要反复查`Product`的
  `ingredients`/`byproducts`/`batchSize`，见`manufacture.md`）。
