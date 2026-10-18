# industry.h / industry.cpp

## 职责

`Industry`：工业系统域类，持有`Product`类型目录+`Storage`/`Manufacture`两张表，
驱动"结算→全城统一调配→算今天产量"这套每日三阶段——和`Traffic`持有`Vehicle`
同一个模式（见`traffic/traffic.h/.md`），不知道UE Actor的存在，和UE层没有任何
互相持有关系。

## 关键设计

- **"工厂"改叫"工坊"**：本项目里`XxxFactory`已经是"mod创建/销毁管理器"的专用
  术语（比如这个类自己持有的`ProductFactory`/`StorageFactory`/
  `ManufactureFactory`），如果继续把"实际生产东西的建筑/单位"也叫"工厂"会和这些
  类名混淆，所以这个概念（类名`Manufacture`不变）中文统一叫**工坊**。
- **全城统一调配**：没有静态的运输图——`GlobalAllocate()`每天在跨天时重新计算
  一遍全城"谁要收多少、谁要发多少"，完全不考虑物理距离（不绑房间，`Storage`/
  `Manufacture`都没有位置字段）。
- **批次配方**：配方是"一批最少产x份"（`Product::batchSize`），工坊的目标产量
  是"每天要产几批"（正整数），当天实际产量是"能凑出的最大整数批次数"，不是连续
  效率插值，具体算法见`manufacture.md`。

### 目录/表：`products` / `storages` / `manufactures`

- `products`：`unordered_map<string, Product*>`，**key是产品type，不是
  name**——这是目录性质，一个产品type全城只需要一份定义，`CreateProduct()`如果
  发现这个type已经创建过，直接返回已有的那份，不会重复`new`（和
  `storages`/`manufactures`按name索引、同名会先delete旧的这套"覆盖创建"逻辑不
  一样，`Product`是"按需创建一次，重复调用是查询"）。
- `storages`/`manufactures`：`unordered_map<string, T*>`，key是name，和
  `Traffic::vehicles`同一个模式——创建失败(`id`未注册/未在`config.json`对应
  `_mods`数组里启用)返回`nullptr`，不会把半成品塞进表；同名已存在会先delete旧的。

### 每天三阶段：`Tick()`

跨天(`crossedDay`)时按顺序对**全体**跑完一步再跑下一步，不是逐个工坊各自跑完
三步——保证同一天内所有结算/调配互不干扰（比如工坊A的`StartProduce()`不会读到
工坊B这一轮还没结算完的中间状态）：

```cpp
void Industry::Tick(const Time& currentTime, bool crossedDay, PostHandle* post) {
    if (!crossedDay) return;
    for (auto& [name, manufacture] : manufactures) manufacture->WorkAccount();
    GlobalAllocate();
    for (auto& [name, manufacture] : manufactures) manufacture->StartProduce();
}
```

非跨天的普通帧不做任何事——没有"每帧都要处理"的逻辑，不像`Populace`那样还有个
按秒触发的timer集合，`crossedDay`这个信号本身是`AForeverFrameworkActor::
Tick`已经算好透传进来的，`Industry`只需要判断这一个参数。

### 全城统一调配：`GlobalAllocate()`

核心规则是**"按固定顺序先到先得"**——不按比例分，也不贪心优先缺口最大的：

1. 收集系统里出现过的全部产品type（从`products`目录+每个工坊的净外部原料
   需求表里收集，`Storage`本身不暴露"当前有哪些type"的遍历接口，普通仓库不会
   凭空出现目录之外的type，不需要单独收集）。
2. 对每一种type分别做一轮分配：
   - `sources`：遍历`storages`收集当前存量`>0`的仓库，再遍历`manufactures`
     收集其`outputCache`存量`>0`的——直接遍历这两个`unordered_map`本身，顺序
     就是它们各自的遍历顺序。`unordered_map`的遍历顺序取决于哈希值和实现
     细节，不是按key的字典序排列，但在没有插入/删除的情况下，同一个容器实例
     在一次调配过程中的遍历顺序是固定的——规则需要的是"同一次调配里谁先谁后
     是确定的"，不要求"顺序本身要有实际意义"（不需要是创建顺序或字典序），所以
     直接用`unordered_map`自己的遍历顺序就够了，不需要额外维护一份插入顺序
     的`vector`。
   - `destinations`：普通仓库只要`AcceptsCategory()`为真且还有剩余空间就是
     需求方，缺口就是剩余空间本身（仓库对已接受的类型没有上限，能装多少要
     多少）；工坊的`inputCache`只有这个type在它自己的`GetIngredientNeeds()`
     表里才算需求方，缺口是"需求量-当前存量"。
   - 按`destinations`固定顺序，依次从`sources`固定顺序里拿货，直到这个需求方
     满足或者把当前能拿的`sources`都拿完（拿不完就换下一个需求方，不会回头
     再找）。
   - **拿货时跳过`src == dest`**：一个仓库自己当前的存量既会被记进`sources`
     （存量`>0`）又会被记进`destinations`（还有空间）。如果不跳过自己，
     `src->Output`再`dest->Input`到同一个对象上虽然物理存量净变化是0，但仍然
     会消耗掉这个`destination`记录的`remaining`额度——一旦"自己当前存量"达到
     "自己剩余空间"(即`stock >= capacity/2`)，这一次自我转账就会把
     `remaining`完全吃满，导致真正的外部来源（比如工坊的`outputCache`）永远
     排不上号，产物在`outputCache`里越攒越满、`StartProduce()`因为
     `outputCache`没有空间而永久停产——这是实测踩到的一个bug（`storages`
     两两互相不为空且刚好达到半满时，全系统就会卡死），不是理论上的边界情况,
     跳过自己能避免这个问题。

### 阶段占位：`ApplyChange()`

目前没有任何`Change`子类是`Industry`域自己认识、需要处理的，恒返回false(七个域都不认识时由
`AForeverFrameworkActor::ApplyChange`统一打"未实现"警告)。

### 不绑定房间/位置

仓库/工坊不需要绑`Room`，容量由mod自己声明的`capacity`常量决定
（`storage_mod.h`），不依赖任何房间数据。这意味着生成的仓库/工坊是"悬空"的，
没有对应的UE侧可视化Actor。`Storage`/`Manufacture`都没有位置字段，
`GlobalAllocate()`完全不考虑物理距离。

## 依赖关系

- 依赖：`Core/industry/product.h`/`storage.h`/`manufacture.h`（持有所有权）、
  `Dependence/industry/product_factory.h`/`storage_factory.h`/
  `manufacture_factory.h`（通过`Registry::Get()`拿引用）、`Core/common/
  registry.h`。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`
  （`Industry* industry`成员，`EnsureIndustryGenerated()`创建、`Tick`/
  `ApplyChange`已经接入主循环）。

## 待办/后续阶段

- 仓库/工坊目前没有对应的UE侧可视化Actor，纯粹靠日志验证系统运转。
