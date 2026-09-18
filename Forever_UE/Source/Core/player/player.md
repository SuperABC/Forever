# player.h / player.cpp

**注意**：这是老工程的"物件/道具/手机"domain（`Asset`/`App`/`Puzzle`三个Mod扩展点），不是
UE的`AForeverPlayerController`——两者是完全不同的东西，动手前先确认没有认错文件，见
`PHASE4_PLAN.md`"关于player domain改名的说明"一节。

空骨架，构造/析构都是`= default`，没有任何字段/方法。这次新增纯粹是为了让`Core/common/
implement.h`的`PostImplement`能拿到7个域的真实指针，不是提前实现这个域——真正的业务聚合
逻辑留到`PHASE4_PLAN.md`阶段4-7再点名做。

## 持有方式

和`Map`/`Populace`/`Story`一样，直接挂在`AForeverFrameworkActor`身上（`Player* player`
成员，生命周期管理方式相同），不是被`UForeverAssetFrameworkComponent`持有。

## 依赖关系

- 依赖：无。
- 被谁依赖：`Core/common/implement.h`（`PostImplement`构造函数）、
  `Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`。
