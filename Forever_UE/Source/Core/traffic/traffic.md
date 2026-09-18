# traffic.h / traffic.cpp

空骨架，构造/析构都是`= default`，没有任何字段/方法。这次新增纯粹是为了让`Core/common/
implement.h`的`PostImplement`能拿到7个域的真实指针，不是提前实现Traffic域——真正的业务
聚合逻辑（`Route`/`Station`/`Vehicle`三个Mod扩展点已经在`Dependence`/`Basic`层铺好）留到
`PHASE4_PLAN.md`阶段4-3再点名做。

和`Source/Forever/Framework/ForeverTrafficFrameworkComponent`（阶段2骨架，UE层Traffic域
组件）不是一回事，两者目前都是空的，没有互相持有关系。

## 持有方式

和`Map`/`Populace`/`Story`一样，直接挂在`AForeverFrameworkActor`身上（`Traffic* traffic`
成员，生命周期管理方式相同），不是被`UForeverTrafficFrameworkComponent`持有。

## 依赖关系

- 依赖：无。
- 被谁依赖：`Core/common/implement.h`（`PostImplement`构造函数）、
  `Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`。
