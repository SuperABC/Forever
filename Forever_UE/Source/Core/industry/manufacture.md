# manufacture.h / manufacture.cpp

## 职责

`Manufacture`：实际生产东西的单位。**"工厂"在本项目里已经是mod创建/销毁管理器（`XxxFactory`）的专用术语**，
生产单位统一叫**工坊**，类名不变。构造函数传入`ManufactureFactory*`+id，内部调`factory->CreateManufacture(id)`
拿到`ManufactureMod*`，再`SetTargets()`、`ComputeRecipe()`。自带`inputCache`/`outputCache`两个内部`Storage`
（直接构造，不走mod系统，见`storage.md`），负责原料/产出的暂存中转。由`Industry`按name创建并持有
（`std::map<string, Manufacture*>`）。

## 关键设计

### 批次化的配方展开+副产物抵消（`ComputeRecipe()`，构造时跑一次）

语义是"批次配方+整数批次"：一批最少产`batchSize`份，当天产几批必须是整数。mod里配置的`targets`是
product type→每天批数。

1. **估算副产物**（`estimatedByproducts`）：假设每个配置的target原样满批达成，累加副产品量。只用来判断
   "副产物够覆盖几整批"，不是最终`byproducts`——否则会把"被别的target副产物覆盖、根本没独立生产"的那部分批次
   的副产品也算进去。
2. **覆盖抵消收敛成整数`targets`**：`coveredBatches=floor(覆盖量/batchSize)`，`activeN=max(0, N-coveredBatches)`，
   `activeN>0`才进`targets`。
3. **按收敛后的`targets`重算真正的`byproducts`**，同时记`byproductsByTarget`（各target各自贡献的副产品），
   供`WorkAccount()`按达成比例打折。
4. **递归展开原料**：原料本身也是本工坊的target就继续展开（自产自用），否则算外部原料，按绝对量累加。
5. **副产物再抵消外部原料**：绝对量直接相减（原料需求是连续的，不需要整数化）。得到`ingredients`。

随后设置容量：`inputCache`=净外部原料总量，`outputCache`=净活跃批次产出+全部副产品总量（按满负荷）。
`inputCache`设为只能放（需求方），精确product需求=各原料占单日总量的比例；`outputCache`只能取（供给方）。
物流完全靠这两个cache的权限与需求驱动（见`industry.md`）。`mod->targets`为空时两个cache容量为0。

多target互相覆盖/中间产品递归展开这几个分支目前只有逻辑，没有现成的mod案例覆盖（Basic里的农场/牧场/食品
加工厂都是单target、纯外部原料），改动前需要自己设计用例验证。

### 每日两步：`WorkAccount()` / `StartProduce()`

上下游对接由`Industry::Distribute()`负责，`Industry::Tick()`跨天时对**全体**工坊先全部`WorkAccount`、再
`Distribute`、再全部`StartProduce`，保证同一天内结算与调配互不干扰。

- **`WorkAccount()`**：把上一轮`StartProduce()`存的`pendingProduction`结算进`outputCache`。**按实际达成比例结算**：
  `actualRatio=pendingProduction[target]/targets[target]`，副产品=`byproductsByTarget[target]`×该比例，不按理论满批量。
  结果同时记入`lastProduced`（含副产品），供`Industry::GetDailyProduction`汇总报表。
- **`StartProduce(capacityRatio)`**：用`inputCache`存量和`outputCache`剩余空间算这一轮能产几批。
  `materialRatio`=所有原料里`现存量/(标准需求×capacityRatio)`最小的那个（上限1）；`spaceRatio`=
  `outputCache`剩余空间/(标准满负荷产出×capacityRatio)（限制在[0,1]）；`overallRatio=min`两者，
  每个target的批数=`floor(activeN×capacityRatio×overallRatio)`。最后按`overallRatio`**预扣**`inputCache`原料
  （让下一轮看到准确的当前存量，不等结算时才扣）。`capacityRatio`由`Industry::Tick`按到岗率算出，可以大于1。
- **`SeedOutputCache()`**：新开局给产出cache放半天产量（每个target：`floor(activeN×batchSize/2)`），读档不调用。

### 创建失败：`IsValid()`

`mod`为空（id没注册或没启用）时构造函数提前返回，此时`inputCache`/`outputCache`为nullptr；
`Industry::CreateManufacture`检查`IsValid()`后整个丢弃，析构对nullptr的delete是安全的。

## 依赖关系

- 依赖：`Dependence/industry/manufacture_mod.h`/`manufacture_factory.h`；`Industry::FindProduct()`（查配方/
  批量/副产品）；`Storage`（两个内部cache）。
- 被谁依赖：`Industry`（持有所有权并驱动每日流程）、`IndustrySaveCodec`（友元，存取两个cache库存和
  `pendingProduction`；`targets`/`ingredients`等结构数据由配方重新算出，不存档）。

## 待办/后续阶段

- `StartProduce()`的`overallRatio`按整个工坊统一限制，没有"按target分摊"——多target竞争同一种紧缺原料的场景
  需要重新设计这一步。
