# storage_mod.h / storage_mod.cpp

## 职责

`StorageMod`：一个仓库的类型定义——只声明"能装什么(`categories`)"+"能装多少(`capacity`)"。

## 关键设计

- 不知道自己的上下游/位置，这些由Industry的全城统一调配逻辑在运行时读写，见`Core/industry/storage.md`。
- `categories`和`ProductMod::categories`同一套字符串标签，任一交集即可存放。
- `capacity`是**共享容量池**(不同产品类型共用总容量)，不是每种产品类型各自独立配额。
- **`SetProperty()`两段式**：具体子类在这里填两个字段，不在构造函数里赋值。
- 带存档钩子，默认不存任何东西，见`common/mod_state.md`。
- `canTake`/`canPut`：只能放=需求方（商店仓库、工坊原料cache），只能取=供给方（工坊产出cache），可取可放=独立仓库。
- `serviceTypeDemand`（货物服务类型,比例）与`productDemand`（product id,比例）二选一，同时给出时Core只认`productDemand`；比例是占本仓库容量的比例[0,1]。仓库靠它自己驱动物流补货。

## 依赖关系

- 依赖：`common/mod_state.h`。
- 被谁依赖：`storage_factory.h`创建它；`Core/industry/storage.h`消费它；具体实现在`Source/Basic/industry/storage_basic.*`和外部mod里。

## 待办/后续阶段

- 无。
