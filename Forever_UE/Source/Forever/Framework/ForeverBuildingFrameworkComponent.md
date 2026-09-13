# ForeverBuildingFrameworkComponent.h / .cpp

## 职责

`GenerateBuildings(Map* inMap)`遍历`map->GetBuildings()`（`unordered_map<string,Building*>`，
寻址用，见`Source/Core/map/map.md`"寻址"一节，遍历用结构化绑定），给每栋building分配LOD状态+
mesh section下标，同步建好远处灰色cube作为基线状态。**独立建筑和园区内部建筑一视同仁**——
不再像早前那样跳过`building->GetParentZone()`非空的building（那批以前由
`ForeverZoneFrameworkComponent`用`DefaultRoad`材质单独画一份），这次统一走同一套楼体
footprint/楼层/LOD渲染。

建筑可视化cube不再填满整个`Building`落地的`Quad`，而是收缩到`BuildingMod`声明的楼体子矩形
（`Building::GetBodyOffsetX/Y()`/`GetBodySizeX/Y()`，见`Source/Core/map/building.md`），并且
按`basements`/`layers`/`floorHeights`（`Building::GetBasementCount/LayerCount/
GetFloorHeights()`）逐层堆叠——不再是过去那种"整栋建筑一个矮扁box"。

## 两级LOD

- **近处（`EBuildingLod::Near`）**：每层楼一个procedural mesh box，贴固定的白色
  `buildingMaterial`（`Pure`材质，默认色）。
- **远处（`EBuildingLod::Far`）**：整栋一个box，贴`ResolveLodMaterial(building)`解析出来的
  材质——`Building::GetLodMaterialPath()`（转发`BuildingMod::lodMaterial`）非空时按路径
  `LoadObject`+创建MID（`lodMaterialCache`按路径缓存，避免重复创建）；留空时用组件级的
  `defaultLodMaterial`（`Pure`+`SetVectorParameterValue("Color", FLinearColor(0.5,0.5,0.5))`
  染灰，照抄`ForeverRoadnetFrameworkComponent.cpp`给`pedestrianNavMaterial`染黑的同一手法，
  不强制要求每个mod都提供专门的灰色资产）。
- **切换距离**：`lodSwitchDistance`（`UPROPERTY(EditDefaultsOnly)`，默认20，地图单位），按
  viewer（`UGameplayStatics::GetPlayerPawn(GetWorld(),0)->GetActorLocation()/
  BUILDING_WORLD_SCALE`，和`ForeverTerrainFrameworkComponent`取viewer位置同一个写法）到
  `building`世界中心的**水平（`FVector2D`）距离**判定，忽略高度差——以后如果发现有问题（比如
  摄像机长期悬空很高导致误判为"远"）容易改成3D距离，这次没有这个需求。

## 两个独立的`UProceduralMeshComponent`，section按building分区

- **`buildingMesh`**（近处/楼层）：每栋building独占一段连续的section区间
  `[nearSectionBase, nearSectionBase+nearFloorCount)`，`nearFloorCount=
  GetBasementCount()+GetLayerCount()`，下标分配在`GenerateBuildings()`里一次性做完（一个
  全局递增计数器`nextNearSectionIndex`），不会重复分配——`GenerateBuildings()`只会跑一次
  （`AForeverFrameworkActor::EnsureMapGenerated()`用`if(map)return;`幂等保护）。
- **`buildingLodMesh`**（远处）：每栋building一个section（`farSectionIndex`，另一个独立
  递增计数器`nextFarSectionIndex`，和近处的下标空间互不干扰，因为是两个不同的PMC）。
- 用两个独立PMC而不是共享一个PMC靠index range区分，照抄`ForeverTerrainFrameworkComponent`
  已有的"一个组件持有多个PMC"先例，逻辑更简单——近/远切换时只需要`ClearMeshSection`/
  `CreateMeshSection`对应PMC的对应section，不会互相影响。

## LOD切换：一个节流队列，`TickComponent`每帧最多处理`maxLodOpsPerTick`条

`GenerateBuildings()`只同步建远处cube，**不建任何近处楼层mesh**——近处楼层完全交给
`TickComponent`按距离增量构建。每个building维护一个`FBuildingRenderState`（`currentLod`/
`transitionPending`/`nearSectionBase`/`nearFloorCount`/`farSectionIndex`，存在
`renderStates`这个`TMap<Building*,...>`里）。`TickComponent`每帧：

1. **入队**：遍历`renderStates`，跳过`transitionPending`为true的（一栋building的操作序列
   没drain完之前不会重复判定/重复入队），按水平距离和`currentLod`比较决定要不要切换：
   - 近→远（`wantNear=false`且`currentLod==Near`）：入队`BuildFarMesh`+
     `DeleteAllNearMeshes`两条。
   - 远→近（`wantNear=true`且`currentLod==Far`）：入队`nearFloorCount`条`BuildFloorMesh`
     （每层一条，`floorIndex`从0到`nearFloorCount-1`）+最后一条`DeleteFarMesh`。
   - 入队后立刻把这栋building的`transitionPending`置`true`。
2. **drain**：从一个全局`TQueue<FBuildingLodOp>`（不是按building分开的队列，跨building的
   操作在同一个FIFO里交替执行也没关系，因为每栋building自己的操作序列内部严格按入队顺序
   drain，不会被打乱）最多取`maxLodOpsPerTick`（`UPROPERTY(EditDefaultsOnly)`，默认8）条
   执行——**只有序列的最后一条**（`DeleteAllNearMeshes`/`DeleteFarMesh`）执行时才更新
   `currentLod`+清`transitionPending`，中间的`BuildFarMesh`/`BuildFloorMesh`只是建mesh，
   不改状态。这样保证一栋building的操作序列不会被提前判定"已完成"从而重复入队，也保证
   跨越阈值的建筑不会因为一次性建太多mesh造成卡顿。

## 楼层Z范围计算

- `ComputeBodyWorldCenter`：楼体子矩形的世界中心——`Building::GetPosX/PosY()`是整个`Quad`
  的世界中心，楼体中心相对这个点有一个未旋转局部坐标系下的偏移(`GetBodyOffsetX/Y()`)，这个
  偏移要跟着building自己的`GetRotation()`一起转，才能得到楼体真正的世界中心，和`Building`
  本身"局部坐标先旋转再平移"的约定一致。
- `ComputeFloorZRange`/`ComputeFullZRange`：`basements`部分往`BUILDING_HEIGHT_EPSILON`
  （地坪高度，避免和地形共面z-fighting）以下堆叠，地上部分从地坪往上堆叠——
  `floorHeights[basements-1]`是最接近地坪的地下室（如果有），`floorHeights[basements]`是
  1楼。远处LOD的cube跨越整栋建筑（最深地下室的底到最高楼层的顶）。

## 依赖关系

- 依赖：`map/map.h`（`Map::GetBuildings()`）、`map/building.h`（`Building`，楼体/楼层/LOD
  的getter）、`map/geometry.h`、`ProceduralMeshComponent`、`UMaterialInstanceDynamic`、
  `Kismet/GameplayStatics.h`（`UGameplayStatics::GetPlayerPawn`）、`Containers/Queue.h`
  （`TQueue`）。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`
  （`EnsureMapGenerated()`在`GenerateZones`之后调用一次`GenerateBuildings`）。

## 待办/后续阶段

- 楼层内部布局（Room/Component）——一整层仍然只是一个cube。
- 按mod自定义LOD切换距离/每帧队列上限——这次先用组件级`UPROPERTY`固定值(20/8)，不做到
  per-mod可配置。
- 3D(含高度)距离判定——这次用水平距离。
- 按`AREA_TYPE`细化权重——`RoadnetMod`已经会给每个lot标好实际分区类型，但`BuildingBasic`
  这个通用占位类型目前对所有`area`一视同仁返回同一个权重，等以后设计具体建筑类型时再细化。
