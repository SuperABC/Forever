# ForeverZoneFrameworkComponent.h / .cpp（`ForeverBuildingFrameworkComponent`同构，见文末）

## 职责

`GenerateZones(Map* inMap)`遍历`map->GetZones()`做两件事：①对每个`Zone`的`GetWalls()`
（`vector<ZoneWallSpec>`，字段语义见`Source/Dependence/map/zone_mod.md`）逐条调用
`BuildWallSegment`铺围墙ISM实例；②对每个`Zone`的`GetInternalBuildings()`逐个画一个扁box
（`ZoneAppendFlatBox`，贴`DefaultRoad`材质）。**第十五轮迁移不再画Zone自己的扁box占位**——
围墙本身已经足够表达zone范围，原来的`zoneMesh`/`zoneMaterial`/`zoneBaseMaterial`/
`AppendFlatBox`/`AppendQuadDoubleSided`都已删除；腾出来的`DefaultRoadnet`材质这次改贴给
园区内部建筑的扁box（和顶层Building贴`Pure`材质区分开，PIE里一眼能看出哪些building是园区
内部的）。大门（`ZoneGateSpec`）这次没有资产，只在`Zone`上存数据，这个组件不消费它、不生成
任何几何。

## 关键设计

- **`GetOrCreateWallISM`/围墙分段铺设算法照抄`ForeverRoadnetFrameworkComponent.cpp`的
  `GetOrCreateRoadISM`/`BuildRoadInstances`里的`tileRange`**：同一个mesh路径复用同一个
  `UInstancedStaticMeshComponent`（`wallMeshInstances`，`TMap<FString,...>`缓存，逻辑完全
  照抄）；分段数量算法同样是"scaled unit落在`[0.8,1.2]*unit`区间内，取最接近`rangeLen/unit`
  的合法整数`n`"，每个实例沿长度轴缩放`actualSegLen/unit`倍。和Road的区别是围墙是直线段（不是
  Bezier曲线），不需要`Connection::GetPoint`/`GetTangent`的弧长参数化，直接线性插值端点即可；
  `n<=0`（这段长度连一节unit都铺不出来）这次先跳过，不做`AppendFlatRoadCube`那样的退化兜底
  渲染（标了TODO，见`zone_mod.md`"待办"）。
- **`BuildWallSegment`自己重新实现了一遍"zone局部坐标(原点在矩形中心)转世界坐标"的旋转公式**
  （和`Map::ZoneLocalToWorld`同一套cos/sin写法），而不是调用`Map`的方法——`ZoneLocalToWorld`
  是`Map`的私有成员，Forever层拿不到，只能照抄公式（两边各自维护一份，改动时要注意同步）。
  `ZoneWallSpec`的局部坐标语义（参考边+margin+depth，进深`depthInward`控制往内/往外量）见
  `zone_mod.md`；"内"的具体朝向（WEST边的内是+X等）目前是先按几何直觉随便定的一个约定，效果
  和围墙资产的实际朝向对不上时，翻转`ZoneWallSpec::depthInward`就行，不需要改这里的代码。
- **园区内部建筑的扁box(`ZoneAppendFlatBox`/`ZoneAppendQuadDoubleSided`)是独立复制的一份**，
  手法和`ForeverBuildingFrameworkComponent.cpp`的`BuildingAppendFlatBox`完全一样（双面四边形
  拼六个面+按`Building::GetRotation()`旋转），只是贴的材质（`DefaultRoad`而不是`Pure`）和
  高度常量（`ZONE_INTERNAL_BUILDING_HEIGHT_EPSILON`/`ZONE_INTERNAL_BUILDING_BOX_HEIGHT`，
  数值和`ForeverBuildingFrameworkComponent.cpp`的`BUILDING_HEIGHT_EPSILON`/
  `BUILDING_BOX_HEIGHT`一样，只是不同材质需要各自的`#define`）不同，用独立的
  `internalBuildingMesh`/`internalBuildingMaterial` ProceduralMeshComponent，不和围墙的
  ISM混在一起。**`ForeverBuildingFrameworkComponent::GenerateBuildings`会跳过
  `building->GetParentZone()`非空的Building**（见文末），两个组件对`Map::buildings`里的
  同一批Building按`GetParentZone()`是否为空分流，不会重复画。
- **`GenerateZones`必须在`Map::InitZones()`跑完之后调用**，和其余Framework组件的
  `GenerateXxx(Map*)`模式一致，`AForeverFrameworkActor::EnsureMapGenerated()`负责保证顺序。

## 依赖关系

- 依赖：`map/map.h`（`Map::GetZones()`）、`map/zone.h`（`Zone`，`GetWalls()`/
  `GetInternalBuildings()`）、`map/building.h`（`Building`，`GetRotation()`）、
  `map/geometry.h`（`FACE_DIRECTION`）、`Components/InstancedStaticMeshComponent.h`、
  `ProceduralMeshComponent`、`UMaterialInstanceDynamic`。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`
  （`EnsureMapGenerated()`在`GenerateRoadnet`之后调用一次`GenerateZones`）。

## 待办/后续阶段

- 围墙"贴合不满整数unit时"的退化兜底渲染（这次先跳过）。
- 大门资产/渲染（`ZoneGateSpec`这次只存位置数据，没有mesh字段）。
- Zone内部道路目前不在这个组件渲染范围内——`mesh=""`本来就不画，等以后有园区内道路模型
  再补。
- 阶段4：Zone内部Building自动填充剩余空间的递归布局（用户明确推迟，`internalBuildings`是
  `ZoneMod`显式指定的固定列表，不是自动填充，两者不是一回事）设计好之后，这里可能需要新增
  额外的可视化。

---

# ForeverBuildingFrameworkComponent.h / .cpp

`Building`仍然只画一个矮扁procedural mesh box占位（`GenerateBuildings(Map*)`遍历
`map->GetBuildings()`，自己的`AppendFlatBox`实现：双面四边形拼六个面+按
`Building::GetRotation()`旋转），贴`Pure`材质（原名`White`，现在带一个`Color`参数，默认
白色——building这边保持默认白色不用改），box高度`BUILDING_BOX_HEIGHT`。**第十五轮迁移新增
一处跳过逻辑**：`GenerateBuildings`遍历`map->GetBuildings()`时，如果
`building->GetParentZone()`非空（说明是Zone内部建筑），直接`continue`不画——这批Building
改由`ForeverZoneFrameworkComponent`用`DefaultRoad`材质单独画一份（见上），避免同一个
Building被两个组件各画一次。`GenerateBuildings`必须在`Map::InitBuildings()`跑完、且在
`GenerateZones`之后调用。
