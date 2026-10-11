# manufacture.h / manufacture.cpp

## 职责

`Manufacture`：实际生产东西的单位。**"工厂"这个中文叫法在本项目里已经是"mod创建/
销毁管理器"(`XxxFactory`)的专用术语**，容易混淆，这个概念统一叫**工坊**，
类名(`Manufacture`)本身不变。构造函数传入`ManufactureFactory*`+id，内部调
`factory->CreateManufacture(id)`拿到`ManufactureMod*`——和`Vehicle`/`Product`/
`Storage`同一个模式。自带`inputCache`/`outputCache`两个内部`Storage`实例（直接
构造，不走mod系统，见`storage.md`"两种构造方式"一节），负责当天原料/产出的
暂存中转。

`Manufacture`由`Industry`按name创建、持有（`unordered_map<string,
Manufacture*>`，见`industry.md`）。

## 关键设计

### 批次化的配方展开+副产物抵消算法（`ComputeRecipe()`）

一个工坊可能兼产多种互相关联产品，这套算法把复杂情况化简为"净外部原料需求表+净
外部产出表"，语义是"批次配方+整数批次"（一批最少产x份，当天产几批必须是整数）。
构造函数里跑一次（这一阶段没有"运行期改配方"的需求，不需要支持重新计算）：

1. **估算副产物覆盖量**（`estimatedByproducts`）：假设`mod->targets`里配置的
   每个目标产品都**原样满批**达成，按公式累加副产品总量。这一步只是为了给第2步
   提供"够不够覆盖整批"的判断依据，不是最终结果——如果直接把这一步的结果当成
   `byproducts`最终值，会把"实际上根本没有独立生产、只是被别的target的副产物
   覆盖掉"的那部分批次的副产品也算进去，多算了。
2. **副产物覆盖抵消，收敛成整数`targets`**：`coveredBatches = floor(覆盖量 /
   该产品的batchSize)`，`activeN = max(0, 配置的N - coveredBatches)`——因为
   产量必须是整数批次，要先除以`batchSize`换算成"覆盖了几整批"再做整数减法。
3. **按收敛后的`targets`重新算一遍真正的`byproducts`**，同时记录
   `byproductsByTarget`（每个target各自贡献了多少副产品）——这份簿记是为了
   支撑批次系统特有的"这一轮没能凑够配置批次数时，副产品也要跟着按比例打折"这个
   需求（见下面`WorkAccount()`一节）。
4. **递归展开原料需求**：遇到某个原料本身也是本工坊自己的一个target（中间产品
   自产自用）就继续展开，否则算作外部原料，按绝对量（不是批次数）累加——按
   "每批用量×(绝对量/batchSize)换算出的批次数"算量。
5. **副产物再次抵消外部原料需求**：绝对量层面直接相减，不需要整数化（原料需求
   本身是连续的，只有"今天产几批"这个决策点需要整数化）。

`inputCache`/`outputCache`的容量在这一步算完之后设置：`inputCache`按净外部
原料需求总量，`outputCache`按"净活跃批次的产出总量+全部副产品总量"，按满负荷
需求定容量。

**测试案例说明**：农场/牧场没有原料也没有副产品，食品加工厂只有一个target
(汉堡)、原料是纯外部小麦/牛肉——这几个案例都不会触发上面第1/2/3步的"多target
互相覆盖抵消"分支，也不会触发第4步的"中间产品递归展开"分支。这几步算法完整
实现（不是为了这次的测试案例才简化掉），只是验证顺序上这次的测试案例够不到这些
分支，后续如果要验证这些分支需要设计更复杂的测试案例（比如两个target互为
原料/副产品的场景）。

### 每日两步：`WorkAccount()` / `StartProduce()`

`Manufacture`只有头尾两步，上下游对接由`Industry::GlobalAllocate()`的全城统一
调配负责，不是`Manufacture`自己的方法——`Industry::Tick()`在跨天时对**全体**
工坊按"先全体`WorkAccount`、再统一`GlobalAllocate`、再全体`StartProduce`"的
顺序调用，不是每个工坊各自跑完三步，这样能保证同一天内所有结算/调配互不干扰，
见`industry.md`。

**`WorkAccount()`**：把上一轮`StartProduce()`存进`pendingProduction`的"这批
实际产了几批"结算进`outputCache`。**结算要按实际达成比例，不能按理论满批量
结算**——如果这一轮受原料/空间限制只产出了配置批次数的一部分，副产品也要跟着
打同样的折扣：`actualRatio = pendingProduction[target] / targets[target]`，
用这个比例乘`byproductsByTarget[target]`里记的"理论满批副产品量"，得到这一轮
实际应该入库的副产品量。

**`StartProduce()`**：用`inputCache`当前存量（这一刻应该已经被本轮
`GlobalAllocate()`补过货）和`outputCache`剩余空间，算这一轮实际能产几批。
**当前版本**：不按每个target单独限制，而是先分别算出"原料满足率"（所有原料里
`现存量/标准需求量`最小的那个）和"空间满足率"（`outputCache`剩余空间/标准
满负荷产出总量），取两者较小值作为`overallRatio`，再统一乘到每个target自己的
`activeN`上取整——取最紧张的那个比例限制，仍然是单一`overallRatio`，但按每个
target的`activeN`分别取整批次数。测试案例(每个工坊只有一个target)这个简化和
"精确到按target分摊限制"是等价的；如果后续要验证多target互相竞争同一种紧缺
原料的场景，需要重新设计这一步为"按target分摊"，目前先用这个更简单的版本跑通。

`StartProduce()`最后按`overallRatio`预扣`inputCache`里对应的原料——为下一轮
`StartProduce()`腾出准确的"当前存量"基准，不是等`WorkAccount()`结算时才扣。

### 创建失败：`IsValid()`

`Manufacture`构造函数不保证`mod`非空——`id`对应的工坊类型没有被注册，或者
注册了但没有在`config.json`的`"manufacture_mods"`数组里启用，
`CreateManufacture`都会返回`nullptr`。`Manufacture::IsValid()`供
`Industry::CreateManufacture()`创建完之后立刻检查，失败就整个丢弃这个
`Manufacture`（`inputCache`/`outputCache`两个`Storage`此时可能已经`new`出来
了，析构函数会正常delete掉，不会泄漏）。

## 依赖关系

- 依赖：`Dependence/industry/manufacture_mod.h`/`manufacture_factory.h`
  （`ManufactureMod`/`ManufactureFactory`）、`Core/industry/product.h`
  （`Industry::FindProduct()`查配方/分类，配方展开算法要反复用到）、
  `Core/industry/storage.h`（`inputCache`/`outputCache`两个内部实例）。
- 被谁依赖：`Industry`（持有所有权，按name索引，`Tick()`跨天时按"全体
  WorkAccount→GlobalAllocate→全体StartProduce"的顺序驱动）。

## 待办/后续阶段

- `StartProduce()`的`overallRatio`按整个工坊统一限制，没有"按target分摊"——
  多target互相竞争同一种紧缺原料的场景需要重新设计这一步，目前的测试案例
  （每个工坊只有一个target）不触发这个差异。
