# ForeverZoneFrameworkComponent.h / .cpp（`ForeverBuildingFrameworkComponent`同构，见文末）

## 职责

从空`UCLASS()`升级，`GenerateZones(Map* inMap)`遍历`map->GetZones()`，每个`Zone`（Core层，
继承`Quad`）按自己的`GetPosX/Y`/`GetSizeX/Y`画一个矮扁procedural mesh box，贴
`DefaultRoadnet`材质（已确认是黄色）。这是阶段4-1 Zone落地这次唯一的可视化手段——不做真实
建筑外观，人工看场景里的cube面积分配是否合理即可（要求9的验证方法），Zone内部再摆Building
的递归布局明确推迟，见`Source/Core/map/map.md`。

## 关键设计

- **box构建手法照抄`ForeverRoadnetFrameworkComponent.cpp`导航图debug可视化的`AppendNavBox`**
  （双面四边形拼六个面，任意朝向都不会因背面剔除消失），但那个helper是该文件的匿名namespace
  私有实现，这里独立复制一份`AppendFlatBox`，不是共享调用——两处box的语义/参数不完全一样
  （这里直接吃`Quad`，没有Z范围参数，固定用`ZONE_HEIGHT_EPSILON`~`+ZONE_BOX_HEIGHT`）。
- **`AppendFlatBox`按`Zone::GetRotation()`旋转box的四个角点**（局部坐标先按`rotation`旋转、
  再平移到世界坐标，和`Lot::GetPosition`同一套约定）——`Zone`/`Building`的`rotation`字段是
  自己新增的（不是继承自`Quad`，`Quad`本身没有旋转），来自`Map::InitZones`/`InitBuildings`
  落地时从对应顶层`Lot`的`GetRotation()`赋值，详见`Source/Core/map/zone.md`。PIE验证发现
  之前没传旋转时，斜向道路旁边的Zone/Building扁cube会显示成轴对齐、和实际地块朝向对不上。
- **`GenerateZones`必须在`Map::InitZones()`跑完之后调用**，和其余Framework组件的
  `GenerateXxx(Map*)`模式一致，`AForeverFrameworkActor::EnsureMapGenerated()`负责保证顺序。

## 依赖关系

- 依赖：`map/map.h`（`Map::GetZones()`）、`map/zone.h`（`Zone`）、`map/geometry.h`（`Quad`）、
  `ProceduralMeshComponent`、`UMaterialInstanceDynamic`。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`
  （`EnsureMapGenerated()`在`GenerateRoadnet`之后调用一次`GenerateZones`）。

## 待办/后续阶段

- 阶段4：Zone内部Building布局设计好之后，这里大概率需要新增"Zone footprint内部再画一批
  Building cube"的逻辑，目前只画Zone自己的外框。

---

# ForeverBuildingFrameworkComponent.h / .cpp

和`ForeverZoneFrameworkComponent`结构完全一样（`GenerateBuildings(Map*)`遍历
`map->GetBuildings()`，同款`AppendFlatBox`），唯一区别是贴`White`材质、box高度
（`BUILDING_BOX_HEIGHT`）和Zone（`ZONE_BOX_HEIGHT`）不一样，方便PIE里同时看到两种cube时能
区分层级——具体数值以代码为准，不是刻意的语义约定。`GenerateBuildings`必须在
`Map::InitBuildings()`跑完、且在`GenerateZones`之后调用。
