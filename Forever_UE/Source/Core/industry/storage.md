# storage.h / storage.cpp

## 职责

`Storage`：一个仓库的实体，或者工坊(`Manufacture`)自带的输入/输出cache（见
`manufacture.md`）——只暴露"能装什么(`categories`)"+"当前装了多少(`stock`)"，
不知道自己的上下游、不知道位置。**运输(谁给谁多少)完全由`Industry`的全城统一
调配逻辑(`GlobalAllocate`，见`industry.md`)统一读写`Input`/`Output`来完成，
`Storage`自己不主动做任何事**，不持有任何"我认识谁"的信息，也不考虑物理距离。

## 关键设计

### 两种构造方式

- `Storage(StorageFactory* factory, const string& id, const string& name)`：
  **走mod系统**，`id`需要在`config.json`的`"storage_mods"`数组里启用（和
  `Vehicle`/`Product`同一个模式），供玩家可配置的"真实仓库"用。
- `Storage(const string& name, const vector<string>& categories, float
  capacity)`：**直接构造，不经过mod系统**，专供`Manufacture`创建自己的输入/
  输出cache用。**这里不走mod id**：`Factory::IsEnabled()`要求`registries`和
  `configuredArgs`两边都有，给一个纯内部实现细节（工坊的暂存中转，不是玩家能
  配置的内容）强加一层mod配置没有必要，所以直接构造，跳过整个mod系统。

`IsValid()`对两种构造方式的含义不同：mod驱动的构造里，`mod`为空（`id`未注册/
未启用）说明创建失败；直接构造的`Storage`没有`mod`这个概念，`valid`恒为
`true`。

### 容量模型：共享容量池，不分产品类型

`stock`是`unordered_map<string, float>`，`GetSpace() = capacity - 所有类型
存量之和`——不同产品类型共用同一个总容量池，不是每种产品类型各自独立配额。
`Input`/`Output`都是"按空间/存量截断+返回实际值"的模式（不会抛异常/断言
失败），调用方(`Industry::GlobalAllocate`)需要自己处理"没吃满/没吐够"的差额。

`SetCapacity()`供`Manufacture::SetProperty()`在跑完配方展开算法、算出"这个
工坊今天需要多少输入/输出空间"之后调整两个内部cache的容量——这个值会随配方/
目标产量变化而变化，不是构造时就能确定的常量。

### 分类匹配：`AcceptsCategory()`

和某个`Product::GetCategories()`比较，只要有任一字符串相等就返回`true`。
仓库对应的`categories`来自mod（多标签），产品的`categories`也来自mod——匹配
只在`Industry::GlobalAllocate()`阶段发生一次，`Storage`自己不缓存任何匹配
结果。

## 依赖关系

- 依赖：`Dependence/industry/storage_mod.h`/`storage_factory.h`
  （`StorageMod`/`StorageFactory`，仅mod驱动的构造需要）。
- 被谁依赖：`Industry`（持有所有权，按name索引普通仓库，`GlobalAllocate()`
  统一读写）、`Manufacture`（持有两个`Storage*`作为`inputCache`/
  `outputCache`，直接构造、不走mod系统）。
