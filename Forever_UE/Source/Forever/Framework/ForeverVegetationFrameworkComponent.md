# ForeverVegetationFrameworkComponent

## 职责

全图植被的统一渲染。Core的`Map::GetVegetation()`里存着已经展开好的每一棵树（见
`Source/Core/map/vegetation.md`），这个组件把它们按mesh分组，每个StaticMesh一个
`UHierarchicalInstancedStaticMeshComponent`，所有树都是HISM的实例。

## 关键设计

- **加载放在最开始，运行时只增删实例**——`GenerateVegetation(map)`先对`GetMeshPaths()`逐个
  `LoadObject<UStaticMesh>`并建好HISM(加载失败打Warning，那个mesh的树全部跳过)，再按mesh分组
  每个HISM只调一次`AddInstances`批量添加(只触发一次树重建)。之后`AddTree`/`RemoveTree`只增删
  实例；`AddTree`遇到没预加载过的mesh会现场加载一次并打Warning，正常情况下不应该发生。
- **只支持StaticMesh**——HISM只能实例化StaticMesh，蓝图Actor不能这样批量渲染。
- **树id和实例下标的映射**——HISM开了`SetRemoveSwap()`，删除时最后一个实例会被挪到被删的位置；
  `treeSlots`(treeId→mesh+实例下标)和`instanceTreeIds`(每个HISM的实例下标→treeId)两张表在
  `RemoveTree`里一起修正。初始树沿用Core的`TreeInstance::id`，运行时新增的id接着往后数。
- **Transform**——位置=地图element坐标×1000，Z=`Map::SampleHeight`(含细化高度)×1000，
  `FRotator(0, yaw°, 0)`，等比缩放直接用Core算好的最终scale(随机已经在Core里叠加过)。
- **剔除距离**——`cullDistanceStart/End`(UU，默认0~60000)是`EditDefaultsOnly`，HISM按
  cluster剔除远处的树。mobility设成Static，和FrameworkActor的sceneRoot一致。
- 调用时机：`AForeverFrameworkActor::EnsureMapGenerated`在`GenerateBuildings`之后调用。

## 依赖关系

- 依赖：`map/map.h`(`Map::GetVegetation`/`Map::SampleHeight`)、UE `HierarchicalInstancedStaticMeshComponent`。
- 被谁依赖：`AForeverFrameworkActor`。

## 待办/后续阶段

- 行道树的位置/间距/缩放都是粗估，还没有避开建筑出入口(RoadOpening)。
- 运行时增删目前只有组件接口，还没有接到Story/Change之类的调用方。
