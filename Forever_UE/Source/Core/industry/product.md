# product.h / product.cpp

## 职责

`Product`：一种产品的**类型目录条目**，独占持有一个`ProductMod*`——和`Vehicle`
持有`VehicleMod`同一个模式（见`traffic/vehicle.h/.md`），构造函数传入
`ProductFactory*`+id，内部调`factory->CreateProduct(id)`拿到`ProductMod*`，
析构时`factory->DestroyProduct(mod)`。**`Product`由`Industry`创建、持有一份，作为全局目录**
（`std::map<string, Product*>`，key=mod id，约定与type相同，见`industry.md`）。

`Product`只是纯粹的配方/分类定义，不存任何数量——"当前存了多少"这类库存数据
全部放在`Storage`自己的存量表里（见`storage.md`）。一种产品在全城只需要一份
`Product`实例（Industry的目录），不需要每个仓库各自创建一份`Product`对象来
存量。

## 关键设计

### 批次配方：`batchSize`/`ingredients`/`byproducts`

配方直接内嵌在`Product`本身，不是独立的Recipe类，是**批次配方**：

- `batchSize`（对应产品设计里的"x"）：一批最少产多少份。
- `ingredients`：`unordered_map<type, 数量>`，生产**一批**（即`batchSize`份）
  需要多少某种原料。
- `byproducts`：同上，生产一批会额外产生多少某种副产品。

工坊(`Manufacture`)的目标产量是"每天产几批"（正整数，见`manufacture_mod.h`的
`targets`字段），当天实际产量是"能凑出的最大整数批次数"，不是连续效率——
`batchSize`不要求是整数（"一批"可以是任意正数份），但"批次数"必须是整数，具体
的批次展开/抵消算法见`manufacture.md`。

### 价格

全部是`int64`、单位分。mod给出零售价/进口价/出口价，要求`零售价>进口价>出口价>0`，不满足整个product判无效
（构造函数里销毁mod并置空，`IsValid()`为false）。城内批发价`wholesalePrice`不是mod字段，初始=
(进口价+出口价)/2，之后由市场调控（`SetWholesalePrice`），进存档（`industry_save.md`）。批发价是Society结算
货物流水时用的价格；零售价是商店卖给市民的价格。

### 服务类型：`serviceType`/`serviceQuality`

市民可直接消费的产品带服务类型（如food）和质量；`serviceType`为空表示暂不开放市民直接购买（小麦/牛肉等原料）。
`Storage`的服务类型需求按它匹配。

### 分类标签：`categories`

`categories`是一个字符串数组，不是专门的Category类——和`Storage::
categories`（见`storage.md`）比较时只要有任一字符串相等就算匹配，仓库/工坊的
输入cache据此判断"能不能收这个产品"。分类匹配只在`Industry::DistributeSupply`
判断"可取可放仓库收不收这个产品"时用到，`Product`自己不做比较逻辑。

### 创建失败：`IsValid()`

`Product`构造函数不保证`mod`非空——`id`对应的产品没有被注册，或者注册了但没有
在`config.json`的`"product_mods"`数组里启用，`CreateProduct`都会返回
`nullptr`；价格关系不合法同样判无效。`Product::IsValid()`供`Industry::CreateProduct()`创建完之后立刻
检查，失败就整个丢弃这个`Product`，不会把半成品塞进目录。

## 依赖关系

- 依赖：`Dependence/industry/product_mod.h`/`product_factory.h`（`ProductMod`/
  `ProductFactory`）。
- 被谁依赖：`Industry`（持有所有权，`CreateProduct`，按type查目录）、`Society`的`Settlement`
  （读批发价）、`Manufacture`（配方展开算法要反复查`Product`的
  `ingredients`/`byproducts`/`batchSize`，见`manufacture.md`）。
