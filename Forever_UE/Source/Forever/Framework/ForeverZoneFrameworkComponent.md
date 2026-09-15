# ForeverZoneFrameworkComponent.h / .cpp

## 职责

`GenerateZones(Map* inMap)`遍历`map->GetZones()`（`unordered_map<string,Zone*>`，寻址用，见
`Source/Core/map/map.md`"寻址"一节，遍历用结构化绑定`for (auto& [name, zone] : ...)`），对
每个`Zone`的`GetWalls()`（`vector<ZoneWallSpec>`，字段语义见
`Source/Dependence/map/zone_mod.md`）逐条调用`BuildWallSegment`铺围墙ISM实例。**这次会话
不再画园区内部建筑**——原来这个组件单独用`DefaultRoad`材质画一份园区内部建筑的扁box
（`internalBuildingMesh`/`internalBuildingMaterial`/`internalBuildingBaseMaterial`成员+
`ZoneAppendFlatBox`/`ZoneAppendQuadDoubleSided`两个helper），现在改由
`ForeverBuildingFrameworkComponent`统一渲染`map->GetBuildings()`里的所有building（天然
包含园区内部建筑，不再按`GetParentZone()`是否为空分流成两个组件各画一份），这几个成员/
helper函数已经整体删除。围墙渲染不受影响。大门（`ZoneGateSpec`）这次没有资产，只在`Zone`上
存数据，这个组件不消费它、不生成任何几何。

## 关键设计

- **`GetOrCreateWallISM`/围墙分段铺设算法照抄`ForeverRoadnetFrameworkComponent.cpp`的
  `GetOrCreateRoadISM`/`BuildRoadInstances`里的`tileRange`**：同一个mesh路径复用同一个
  `UInstancedStaticMeshComponent`（`wallMeshInstances`，`TMap<FString,...>`缓存，逻辑完全
  照抄）；分段数量算法同样是"scaled unit落在`[0.8,1.2]*unit`区间内，取最接近`rangeLen/unit`
  的合法整数`n`"，每个实例沿长度轴缩放`actualSegLen/unit`倍。和Road的区别是围墙是直线段（不是
  Bezier曲线），不需要`Connection::GetPoint`/`GetTangent`的弧长参数化，直接线性插值端点即可；
  `n<=0`（这段长度连一节unit都铺不出来）这次先跳过，不做退化兜底渲染（标了TODO）。
- **`BuildWallSegment`自己重新实现了一遍"zone局部坐标(原点在矩形中心)转世界坐标"的旋转公式**
  （和`Map::ZoneLocalToWorld`同一套cos/sin写法），而不是调用`Map`的方法——`ZoneLocalToWorld`
  是`Map`的私有成员，Forever层拿不到，只能照抄公式（两边各自维护一份，改动时要注意同步）。
  `ZoneWallSpec`的局部坐标语义（参考边+margin+depth，进深`depthInward`控制往内/往外量）见
  `zone_mod.md`；"内"的具体朝向（WEST边的内是+X等）目前是先按几何直觉随便定的一个约定，效果
  和围墙资产的实际朝向对不上时，翻转`ZoneWallSpec::depthInward`就行，不需要改这里的代码。
  **围墙进深的中心线偏移必须用完整的`wall.depth`，不是`depth*0.5f`**——沿边方向的
  `marginStart`/`marginEnd`收缩量用的是完整的`depth`（贴墙角的一端缩进`WALL_DEPTH`，四个
  方向统一，不区分方向），如果进深方向的偏移只用一半，两个轴描述的"整体往里缩进depth"就对不
  齐，相邻两条墙的中心线端点会停在两个不同的点上，墙角会露出缺口（这是PIE验证反复调试才定下
  来的：margin=0时缩进不够会crossing成"井"字，margin/depth各自缩一半会在墙角露出缺口，只有
  两个轴都用完整的`depth`才能让两条墙的中心线端点精确重合）。
- **`GenerateZones`必须在`Map::InitZones()`跑完之后调用**，和其余Framework组件的
  `GenerateXxx(Map*)`模式一致，`AForeverFrameworkActor::EnsureMapGenerated()`负责保证顺序。
- **进入/离开检测碰撞盒（第N轮迁移新增）**：`GenerateZones`遍历完`GetWalls()`之后额外调一次
  `BuildCollisionBox(zone)`——真正的`UBoxComponent`+`OnComponentBeginOverlap`/
  `OnComponentEndOverlap`（用户已确认的方案，不是手动Tick轮询），碰撞Profile用UE自带的
  `"Trigger"`预设。水平=Zone自己的Quad（不做尺寸调整，只有Room碰撞盒缩小0.01/Building
  碰撞盒放大0.01，Zone维持原样）；垂直=zone内部所有building的Z范围并集——遍历
  `zone->GetInternalBuildings()`，每个building按和`ForeverBuildingFrameworkComponent.cpp`
  的`ComputeFullZRange`同一个公式（不同翻译单元不能共用匿名namespace函数，这里用本文件
  专属的`ComputeBuildingFullZRange`重算一遍）算出Z范围，取全局min/max。**没有内部building
  的zone跳过、不创建碰撞盒**——没有意义的"内部"体验。Overlap回调只使用生成时预先烘焙好的
  `FString`显示名（`zone->GetAddress()`），绝不解引用`Zone*`/`Building*`/`Map*`（和
  `ForeverBuildingFrameworkComponent`的`EndPlay`野指针教训同一套安全原则），测试阶段用
  `GEngine->AddOnScreenDebugMessage`（`FColor::Yellow`）代替真实的Story事件分发。

## 依赖关系

- 依赖：`map/map.h`（`Map::GetZones()`）、`map/zone.h`（`Zone`，`GetWalls()`/
  `GetInternalBuildings()`/`GetAddress()`）、`map/building.h`（`Building`，算碰撞盒Z范围
  用）、`map/geometry.h`（`FACE_DIRECTION`）、`Components/InstancedStaticMeshComponent.h`、
  `Components/BoxComponent.h`、`Kismet/GameplayStatics.h`、`Engine/Engine.h`。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`
  （`EnsureMapGenerated()`在`GenerateRoadnet`之后调用一次`GenerateZones`）。

## 待办/后续阶段

- 围墙"贴合不满整数unit时"的退化兜底渲染（这次先跳过）。
- 大门资产/渲染（`ZoneGateSpec`这次只存位置数据，没有mesh字段）。
- Zone内部道路目前不在这个组件渲染范围内——`mesh=""`本来就不画，等以后有园区内道路模型
  再补。
