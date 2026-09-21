# traffic.h / traffic.cpp

空骨架，构造/析构都是`= default`。这次新增纯粹是为了让`Core/common/implement.h`的
`PostImplement`能拿到7个域的真实指针，不是提前实现Traffic域——真正的业务聚合逻辑
（`Route`/`Station`/`Vehicle`三个Mod扩展点已经在`Dependence`/`Basic`层铺好）留到
`PHASE4_PLAN.md`阶段4-3再点名做。

和`Source/Forever/Framework/ForeverTrafficFrameworkComponent`（阶段2骨架，UE层Traffic域
组件）不是一回事，两者目前都是空的，没有互相持有关系。

## 持有方式

和`Map`/`Populace`/`Story`一样，直接挂在`AForeverFrameworkActor`身上（`Traffic* traffic`
成员，生命周期管理方式相同），不是被`UForeverTrafficFrameworkComponent`持有。

## `Tick`/`ApplyChange`：占位，保持"每个Core域都有这两个方法"的形状一致

这次重构`AForeverFrameworkActor::Tick`时新增的两个空实现方法。`AForeverFrameworkActor::
Tick`每帧会挨个调用`map`/`populace`/`society`/`industry`/`traffic`/`story`六个域各自的
`Tick`，`AForeverFrameworkActor::ApplyChange`（统一的Change消费入口）也会把没被它自己
处理掉的Change转发给这六个域各自的`ApplyChange`——目前没有任何Change子类是Traffic域
自己认识、需要处理的，`ApplyChange`因此是空实现，也**不打"未实现"警告**（只有
`Story::ApplyChange`保留那条兜底警告，避免六个域各打一遍重复日志）。等Traffic域真正
落地时再补内容，详见`Source/Forever/Framework/ForeverFrameworkActor.md`"统一的Change
消费入口：`ApplyChange`"一节。

## 依赖关系

- 依赖：`Core/common/class.h`（`Time`/`Change`）、`Dependence/common/handle.h`
  （`PostHandle`）。
- 被谁依赖：`Core/common/implement.h`（`PostImplement`构造函数）、
  `Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（持有+`Tick`驱动+
  `ApplyChange`转发）。
