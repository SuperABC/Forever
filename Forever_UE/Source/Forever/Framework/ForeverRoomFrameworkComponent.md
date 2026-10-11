# ForeverRoomFrameworkComponent.h / .cpp

## 职责

对应旧Framework Actor `Room`(C++ Base:`RoomBase`)。当前为空实现（`UCLASS()`无成员/方法），
`Room`这个域槽位留给以后的阶段（比如Populace/Industry给Room关联住户/库存这类逻辑）。

## 关键设计

### Room进入/离开检测碰撞盒不在这里，在`ABuildingElement`里

Room的进入/离开检测碰撞盒实际生成在每栋building专属的`ABuildingElement`里（见
`Source/Forever/Element/BuildingElement.h/.md`），跟着那栋楼自己的其它碰撞盒/近处LOD组件
一起attach到同一个Actor——**不能**给全地图每个Room各生成一个`UBoxComponent`挂在这个组件
所在的单例`AForeverFrameworkActor`上：Room数量级可能是成千上万，全部挂在同一个owner会被
物理引擎对"同一个Actor根组件下大量Static简单碰撞子组件"做的焊接（weld）开销拖累——和
building近处LOD组件共用一个owner是同一个根因，完整说明见
`ForeverBuildingFrameworkComponent.md`"性能排查"一节。

## 待办/后续阶段

- 这个域真正需要的逻辑（和"Room进入/离开检测"无关的部分，那部分已经在`ABuildingElement`
  里）后续按需求落地时收进来。
