# ForeverDoorFrameworkComponent.h / .cpp

## 职责

全图所有门（建筑门、房间门、园区大门，即`Map::GetAllDoors()`）的流式加载：按玩家距离生成/销毁
`UForeverDoorComponent`。门从建筑近/远LOD里独立出来，**不再随建筑LOD创建和清理**，也不再有门的专用LOD队列
和"楼层优先于门"的全局门槛。园区大门原来由`ForeverZoneFrameworkComponent`常驻生成，现在也由这里统一管理。

## 关键设计

- **空间索引**：`GenerateDoors(map)`（生成流程里"生成门"一步，在建筑模型之后）遍历`GetAllDoors()`，记每扇门的门洞中心
  （`Door::GetZ()`是门洞底边中点，取门高一半），放进边长`kGridCellSize`(40米)的格子表。`Door*`在Zone/Building/Room生成时就已创建，
  与渲染无关，所以读档和新建走同一条路。
- **距离跟随市民**：每帧从`UForeverPopulaceFrameworkComponent`读`GetCitizenSpawnDistance()`/`GetCitizenDespawnDistance()`，
  都乘`GetMovingDistanceScale()`——用的是**移动中市民**那一档（默认生成2×2=4、销毁4×2=8地图单位，即40米/80米）：走路的市民比静止的更早
  出现，门要在他们之前就位。度量用`StreamingDistanceBetween`，和市民扫描同一个公式（Z差放大5倍，所以头顶/脚下楼层的门不会因为
  三维距离近而被加载）。没有人口组件时退回`kFallbackSpawnDistance`/`kFallbackDespawnDistance`。
- **迟滞**：生成距离 < 销毁距离，临界距离附近不会来回生成/销毁。
- **每帧扫描**：销毁——遍历已生成的门（`activeEntries`），超出销毁距离的销毁；生成——只查生成半径覆盖的格子，收集未生成且在生成距离内的门，
  按距离从近到远排序后生成。门数量只在几格范围内，每帧全量比较不需要像市民那样分批轮询。
- **每帧限量**：`maxSpawnsPerFrame`(4)、`maxDespawnsPerFrame`(8)，均为`EditDefaultsOnly`。一扇门要建骨骼网格+动画实例+挡板+触发盒
  （4~6次`RegisterComponent`），传送、读档、切换控制权时一次进入范围的门可以有成百上千，不限量会一帧卡死；最近的先建，玩家身边的门最先就位。
  销毁同样要反注册组件，所以也限量（比生成宽松）。**这两个数是估计值，没有在PIE里量过**，按卡顿情况调。
- **销毁流程**：`ClearDoor()`（Tick/Overlap回调判空）→`RemoveInstanceComponent`→`DestroyComponent`，与原来建筑清理近景LOD门完全一致。
  销毁时门的开合状态和`passers`丢失，重新生成是全关状态。
- **等世界生成完**：和建筑一样，`IsInitialGenerationDone()`之前不流式（此时玩家只是过渡用的DefaultPawn）。

## 依赖关系

- 依赖：`Source/Core/map/map.h`/`door.h`、`Element/DoorComponent.h`、`ForeverPopulaceFrameworkComponent`（距离数值和度量）、`ForeverFrameworkActor`（`IsInitialGenerationDone`、取人口组件）。
- 被谁依赖：`AForeverFrameworkActor`创建并在"生成门"一步调用`GenerateDoors`。

## 待办/后续阶段

- 门的总数没有上限（只限每帧速度）：极端密集处同时在范围内的门可能很多，如需再加`maxActiveDoors`等PIE测完再说。
- 车辆的门（园区车行门）按玩家距离加载，不按车辆的流式距离；车辆在范围外撞门时门可能还没生成。
