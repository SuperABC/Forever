# manufacture.h / manufacture.cpp

## 职责

`Manufacture`：实际生产东西的单位。**"工厂"这个中文叫法在本项目里已经是"mod创建/
销毁管理器"(`XxxFactory`)的专用术语**，容易混淆，这个概念这次统一叫**工坊**，
类名(`Manufacture`)本身不变。构造函数传入`ManufactureFactory*`+id，内部调
`factory->CreateManufacture(id)`拿到`ManufactureMod*`——照抄`Vehicle`/`Product`/
`Storage`的模式。自带`inputCache`/`outputCache`两个内部`Storage`实例（直接构造，
不走mod系统，见`storage.md`"两种构造方式"一节），负责当天原料/产出的暂存中转。

`Manufacture`由`Industry`按name创建、持有（`unordered_map<string, Manufacture*>`，
见`industry.md`）。

## 批次化的配方展开+副产物抵消算法（`ComputeRecipe()`）

老工程的`Manufacture::SetProperty()`有一套"目标产量→副产物覆盖抵消→配方递归展开→
再次副产物抵消"四步算法，把一个工坊可能兼产多种互相关联产品的复杂情况化简为"净外部
原料需求表+净外部产出表"——**这次完整保留这套算法**，但把老工程的"单位配方+连续
效率"语义改造成"批次配方+整数批次"语义。构造函数里跑一次（这一阶段没有"运行期
改配方"的需求，不需要支持重新计算）：

1. **估算副产物覆盖量**（`estimatedByproducts`）：假设`mod->targets`里配置的每个
   目标产品都**原样满批**达成，按老工程同样的公式累加副产品总量。这一步只是为了给
   第2步提供"够不够覆盖整批"的判断依据，不是最终结果——如果直接把这一步的结果当成
   `byproducts`最终值，会把"实际上根本没有独立生产、只是被别的target的副产物覆盖掉"
   的那部分批次的副产品也算进去，多算了。
2. **副产物覆盖抵消，收敛成整数`targets`**：`coveredBatches = floor(覆盖量 /
   该产品的batchSize)`，`activeN = max(0, 配置的N - coveredBatches)`——这是和
   老工程连续效率版本的关键差异，老工程这里是`scalar - bpCoverage`直接相减得到
   一个连续值，这次因为产量必须是整数批次，要先除以`batchSize`换算成"覆盖了几整批"
   再做整数减法。
3. **按收敛后的`targets`重新算一遍真正的`byproducts`**，同时记录`byproductsByTarget`
   （每个target各自贡献了多少副产品）——这是这次新增的一份簿记，老工程没有，是为了
   支撑批次系统特有的"这一轮没能凑够配置批次数时，副产品也要跟着按比例打折"这个
   需求（见下面`WorkAccount()`一节），老工程是连续效率、全工坊只有一个`currentWorkload`
   标量，不需要按target拆分。
4. **递归展开原料需求**：遇到某个原料本身也是本工坊自己的一个target（中间产品自产
   自用）就继续展开，否则算作外部原料，按绝对量（不是批次数）累加——和老工程逻辑
   一致，只是老工程按"单位×连续scalar"算量，这次按"每批用量×(绝对量/batchSize)
   换算出的批次数"算量。
5. **副产物再次抵消外部原料需求**：绝对量层面直接相减，不需要整数化（原料需求本身
   是连续的，只有"今天产几批"这个决策点需要整数化）。

`inputCache`/`outputCache`的容量在这一步算完之后设置：`inputCache`按净外部原料
需求总量，`outputCache`按"净活跃批次的产出总量+全部副产品总量"，照抄老工程的
"按满负荷需求定容量"思路。

**测试案例说明**：农场/牧场没有原料也没有副产品，食品加工厂只有一个target(汉堡)、
原料是纯外部小麦/牛肉——这几个案例都不会触发上面第1/2/3步的"多target互相覆盖抵消"
分支，也不会触发第4步的"中间产品递归展开"分支。这几步算法**照样完整实现**（不是
为了这次的测试案例才简化掉），只是验证顺序上这次的测试案例够不到这些分支，后续如果
要验证这些分支需要设计更复杂的测试案例（比如两个target互为原料/副产品的场景）。

## 每日两步：`WorkAccount()` / `StartProduce()`

老工程按`WorkAccount→InitDelivery→StartProduce`三阶段跑，中间的`InitDelivery`
(工坊自己去对接固定的上下游仓库) 这次被`Industry::GlobalAllocate()`的全城统一调配
取代，不再是`Manufacture`自己的方法——`Manufacture`这次只保留头尾两步，`Industry::
Tick()`在跨天时对**全体**工坊按"先全体`WorkAccount`、再统一`GlobalAllocate`、再
全体`StartProduce`"的顺序调用，不是每个工坊各自跑完三步，这样能保证同一天内所有
结算/调配互不干扰，见`industry.md`。

**`WorkAccount()`**：把上一轮`StartProduce()`存进`pendingProduction`的"这批实际
产了几批"结算进`outputCache`。**结算要按实际达成比例，不能按理论满批量结算**——
如果这一轮受原料/空间限制只产出了配置批次数的一部分，副产品也要跟着打同样的折扣：
`actualRatio = pendingProduction[target] / targets[target]`，用这个比例乘
`byproductsByTarget[target]`里记的"理论满批副产品量"，得到这一轮实际应该入库的
副产品量。

**`StartProduce()`**：用`inputCache`当前存量（这一刻应该已经被本轮
`GlobalAllocate()`补过货）和`outputCache`剩余空间，算这一轮实际能产几批。**这次的
简化版本**：不按每个target单独限制，而是先分别算出"原料满足率"（所有原料里
`现存量/标准需求量`最小的那个）和"空间满足率"（`outputCache`剩余空间/标准满负荷
产出总量），取两者较小值作为`overallRatio`，再统一乘到每个target自己的`activeN`
上取整——这是照抄老工程"取最紧张的那个比例限制"的思路，但从老工程的"单一
`currentWorkload`标量、不区分target"简化为"仍然是单一`overallRatio`，但按每个
target的`activeN`分别取整批次数"。测试案例(每个工坊只有一个target)这个简化和
"精确到按target分摊限制"是等价的；如果后续要验证多target互相竞争同一种紧缺原料的
场景，需要重新设计这一步为"按target分摊"，目前先用这个更简单的版本跑通。

`StartProduce()`最后按`overallRatio`预扣`inputCache`里对应的原料——为下一轮
`StartProduce()`腾出准确的"当前存量"基准，照抄老工程的"预扣"设计，不是等
`WorkAccount()`结算时才扣。

## 创建失败：`IsValid()`

`Manufacture`构造函数不保证`mod`非空——`id`对应的工坊类型没有被注册，或者注册了
但没有在`config.json`的`"manufacture_mods"`数组里启用，`CreateManufacture`都会
返回`nullptr`。`Manufacture::IsValid()`供`Industry::CreateManufacture()`创建完
之后立刻检查，失败就整个丢弃这个`Manufacture`（`inputCache`/`outputCache`两个
`Storage`此时可能已经`new`出来了，析构函数会正常delete掉，不会泄漏）。

## 依赖关系

- 依赖：`Dependence/industry/manufacture_mod.h`/`manufacture_factory.h`
  （`ManufactureMod`/`ManufactureFactory`）、`Core/industry/product.h`
  （`Industry::FindProduct()`查配方/分类，配方展开算法要反复用到）、
  `Core/industry/storage.h`（`inputCache`/`outputCache`两个内部实例）。
- 被谁依赖：`Industry`（持有所有权，按name索引，`Tick()`跨天时按"全体
  WorkAccount→GlobalAllocate→全体StartProduce"的顺序驱动）。
