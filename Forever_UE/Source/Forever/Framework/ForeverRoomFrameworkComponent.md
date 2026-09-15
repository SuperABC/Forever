# ForeverRoomFrameworkComponent.h / .cpp

## 职责

对应旧Framework Actor `Room`(C++ Base:RoomBase)。当前为空实现，职责说明见
`ForeverFrameworkComponent.md`域对照表。

## 历史：Room进入/离开检测碰撞盒一度在这里实现，后来挪走了

这个组件曾经短暂地填过内容——给全地图每个`Room`生成一个`UBoxComponent`+
`OnComponentBeginOverlap`/`OnComponentEndOverlap`检测碰撞盒。后来在排查"建筑近处LOD每次
整层增删都巨卡"时发现：全地图所有Room的碰撞盒全部`NewObject`在这个组件所在的单例
`AForeverFrameworkActor`上，会被物理引擎对同一个Actor根组件下大量Static简单碰撞子组件做的
"焊接"（weld）开销拖累——这和"building近处LOD组件全地图共用一个owner"是同一个根因（完整
排查过程见`Source/Forever/Framework/ForeverBuildingFrameworkComponent.md`"性能第2轮"
一节）。修复方式是把Room碰撞盒的生成挪进`ABuildingElement`（见
`Source/Forever/Element/BuildingElement.h/.md`）——每栋building自己的所有Room碰撞盒，
跟着这栋楼自己的近/远LOD组件、building自己的碰撞盒一起attach到这栋楼专属的Actor，不再
共用一个全地图规模的owner。

这个组件恢复回空骨架，`Room`这个域槽位保留给以后的阶段（比如Populace/Industry给Room关联
住户/库存这类和"Room进入/离开检测"完全不相关的逻辑）。

## 待办/后续阶段

- 阶段4逐个域落地后，把这个域真正需要的逻辑（和"Room进入/离开检测"无关的部分，那部分已经
  在`ABuildingElement`里）收进来。
