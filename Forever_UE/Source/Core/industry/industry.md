# industry.h / industry.cpp

空骨架，构造/析构都是`= default`，没有任何字段/方法。这次新增纯粹是为了让`Core/common/
implement.h`的`PostImplement`能拿到7个域的真实指针，不是提前实现Industry域——真正的业务
聚合逻辑（`Product`/`Storage`/`Manufacture`三个Mod扩展点已经在`Dependence`/`Basic`层铺好）
留到`PHASE4_PLAN.md`阶段4-5再点名做。

## 持有方式

和`Map`/`Populace`/`Story`一样，直接挂在`AForeverFrameworkActor`身上（`Industry* industry`
成员，生命周期管理方式相同），不是被某个FrameworkComponent持有。

## 依赖关系

- 依赖：无。
- 被谁依赖：`Core/common/implement.h`（`PostImplement`构造函数）、
  `Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`。
