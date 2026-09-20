# society.h / society.cpp

空骨架，构造/析构都是`= default`，没有任何字段/方法。这次新增纯粹是为了让`Core/common/
implement.h`的`PostImplement`（Story系统的`Post`查询门面，见`Core/story/script_mod.md`）能
拿到7个域的真实指针，不是提前实现Society域——真正的业务聚合逻辑（`Job`/`Organization`两个
Mod扩展点已经在`Dependence`/`Basic`层铺好）留到`PHASE4_PLAN.md`阶段4-4再点名做。

## 持有方式

和`Map`/`Populace`/`Story`一样，直接挂在`AForeverFrameworkActor`身上（`Society* society`
成员，`EnsureSocietyGenerated()`里`new`，`EndPlay`/析构函数里`delete`），不是被某个
FrameworkComponent持有——查过现有代码，`Map`/`Populace`/`Story`这三个已存在的Core聚合对象
全部是Actor自己持有，FrameworkComponent里出现的同类型指针都只是非持有的缓存引用。

## 依赖关系

- 依赖：无。
- 被谁依赖：`Core/common/implement.h`（`PostImplement`构造函数）、
  `Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`。
