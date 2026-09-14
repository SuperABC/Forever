# ForeverBuildingFrameworkComponent.h / .cpp

## 职责

`GenerateBuildings(Map* inMap)`遍历`map->GetBuildings()`（`unordered_map<string,Building*>`，
寻址用，见`Source/Core/map/map.md`"寻址"一节，遍历用结构化绑定），给每栋building分配LOD状态，
同步建好远处灰色cube作为基线状态。**独立建筑和园区内部建筑一视同仁**——不区分
`building->GetParentZone()`是否为空，统一走同一套楼体footprint/楼层/LOD渲染。

近处LOD这次不再是"填满楼体子矩形的一个扁cube"，而是`Building`真正的楼层内部结构——走廊、
房间隔墙、门/窗洞缺口（窗户资产有问题，这次删掉了窗户网格显示，门/窗都只是纯几何缺口）、
楼梯/坡道实体、电梯井墙体、地板/天花板两层独立薄slab，数据
全部来自`Source/Core/map/building.h`的`Floor`/`Room`（`Building::GetFloor(level)`/
`GetRooms()`），几何生成逻辑照抄老工程`ABuildingBase::ConstructBuilding`/`ConstructQuad`。

## 两级LOD

- **近处（`EBuildingLod::Near`）**：完整楼层内部结构，见"近处楼层几何"一节。
- **远处（`EBuildingLod::Far`）**：整栋一个box，贴`ResolveLodMaterial(building)`解析出来的
  材质——`Building::GetLodMaterialPath()`（转发`BuildingMod::lodMaterial`）非空时按路径
  `LoadObject`+创建MID（缓存避免重复创建）；留空时用组件级的`defaultLodMaterial`（`Pure`+
  `SetVectorParameterValue("Color", FLinearColor(0.5,0.5,0.5))`染灰）。继续用
  `buildingLodMesh`这一个`UProceduralMeshComponent`+每栋building一个section——远处只有
  一个box，没有"频繁增删细节"的问题，不需要跟着近处一起换成独立组件。
- **切换距离**：`lodSwitchDistance`（`UPROPERTY(EditDefaultsOnly)`，默认20，地图单位），按
  viewer（`UGameplayStatics::GetPlayerPawn(GetWorld(),0)->GetActorLocation()/
  BUILDING_WORLD_SCALE`）到`building`世界中心的**水平（`FVector2D`）距离**判定，忽略高度差。

## 近处楼层几何：不用PMC section也不用ISM，每段一个独立`UStaticMeshComponent`

**楼体要频繁整层增删（LOD切换/以后建筑增删），PMC虽然可以`ClearMeshSection`单独清空一个
section，但所有building共享同一个PMC对象、共享同一份vertex/index buffer，一整层几十个墙体
分段全部要挤进"这栋building自己的一段连续section区间"这个设计已经不匹配"一层楼有几十个独立
墙体+地板+天花板+楼梯/电梯井墙"这个复杂度；ISM更不合适（同一个mesh资产的批量实例，索引数组
增删会牵连其它实例）。改成老工程本来的路数**：每个墙体分段/地板slab/天花板slab都是一个独立的
`UStaticMeshComponent`（`SpawnCube`：复用一个通用的`/Game/Asset/Meshes/Cube.Cube`单位立方体
网格，从老工程`Content/Asset/Meshes/Cube.uasset`直接拷贝过来(和`Stair.uasset`/`Ramp.uasset`
一起，纯文件复制——两边引擎版本同为5.7，不需要重新导出；`Window.uasset`当时也一起拷贝过来，
但窗户资产本身有问题，后来直接删掉了窗户网格显示逻辑，这个文件现在没有任何代码引用它，留在
`Content/Asset/Meshes/`下不影响什么，没有顺手清理)，`SetWorldScale3D`缩放到目标尺寸），
楼梯/坡道这类有真实3D资产的元素用`SpawnMesh`
（不缩放，按网格自身大小摆放）。`FBuildingRenderState::nearComponentsByFloor`（按floorIndex
分组的`TArray<UStaticMeshComponent*>`）记录这栋building当前占用的所有独立组件，近处LOD
整层增删就是创建/销毁一批组件，不涉及任何共享索引结构。

### 墙体开洞分段算法（`BuildWallsForElement`，照抄老工程`ConstructQuad`::`processFace`）

对`Corridor`/`Single`/`Row`（隐含4面都有墙）/`Stair`/`Elevator`/`Ramp`（按各自的
`GetWall(direction)`）的每一侧墙：把这一侧的门/窗开口（`WallHole`，沿墙方向ratio+offset）
按位置排序，在水平方向切分成若干段——没有开口就是1段整墙；有开口时，每个开口贡献最多3段
（开口前的墙段、开口上方过梁"仅当`y1>0`才有"、开口下方门槛/窗台"仅当开口没到地板才有"），
最后加一段trailing墙段。门/窗都只是缺口，不生成任何东西——**窗户这次不摆网格**（窗户资产
有问题，用户明确要求直接删掉窗户显示逻辑，只保留开洞几何，和门一样纯几何缺口；`windowMesh`
成员/`Window.Window`加载也一并删掉，不再区分门/窗类型，`FBuildingWallOpening`不需要
`isWindow`字段）。墙体厚度固定`BUILDING_WALL_THICKNESS`(0.01f，地图单位，照抄老工程写死值)。

**`y1`/`y2`是从天花板往下量的，不是从地板往上量**——完全照抄老工程`ConstructQuad::
processFace`的约定：`y1`是"开口顶到天花板"这段实心墙(过梁/门楣)的厚度，贴着天花板往下铺
（`if (y1>0)`才有）；`y2`是"开口底到天花板"的距离，`floorHeight-y2`才是"开口底到地板"这段
实心墙(门槛/窗台)的厚度，贴着地板往上铺（`if (floorHeight-y2>0)`才有）。**第一版实现把这
两段的Z方向搞反了**（把`y1`当成贴地板的门槛、把`floorHeight-y2`当成贴天花板的过梁），门
（`y1≈0`、`y2≈floorHeight`，开口几乎顶到天花板）因此被画成"底部一段很高的实心墙+顶部完全
打通"，效果和预期正好上下颠倒——PIE验证发现，对照老工程`BuildingBase.cpp:400-404`
（`op.y1>0`那段墙的Z中心是`center.Z + vertSpan/2 - op.y1/2`，明显贴着天花板；
`bH=vertSpan-op.y2`那段墙的Z中心是`center.Z - vertSpan/2 + bH/2`，明显贴着地板）才定位到
问题所在，修复后两段位置对调（窗户网格当时也同样从`floorTopWorldZ`往下减，现在窗户网格这段
代码整个删掉了，见上一节）。

### 楼层局部坐标 → 世界坐标（`ComputeWorldPosition`）

`Floor`/`Room`等Core对象的坐标都是"楼体局部坐标"（原点在`Building`楼体子矩形的左下角，未
旋转），和`Building::LocalToWorld`（Core侧，纯map单位）同一套约定，这里是它的UE世界单位
版本：先减去半楼体尺寸、加上`bodyOffsetX/Y`换算成"相对Building自身中心的局部偏移"，再按
`building.GetRotation()`旋转、加上`GetPosX/Y()*SCALE`得到世界坐标。

### 楼层Z范围

楼层底部Z用`Building::GetFloorBaseZ(level)`（Core侧现成方法，grade-相对、地图单位，不含
`BUILDING_HEIGHT_EPSILON`这个纯渲染层的"避免和地形共面z-fighting"偏移），渲染层自己加回
这个偏移换算世界Z——`basements`部分往地坪以下堆叠，地上部分从地坪往上堆叠，和远处LOD
`ComputeFullZRange`（最深地下室的底到最高楼层的顶）用同一套grade约定。

### 每层材质/网格解析

按`building->GetMod()->floors[level].assets`（`FloorAssetSpec`，`AssignFloor`时mod声明的）
解析这一层的`wallMaterial`/`floorMaterial`/`ceilingMaterial`（`ResolveMaterial`，留空用
组件默认，这次**不分内外墙**，统一一份默认墙体材质）+`stairMeshPath`/`rampMeshPath`
（`ResolveMesh`，留空用组件默认的`Stair.Stair`/`Ramp.Ramp`）。地板/天花板两层独立薄slab
（`kSlabThickness=0.02f`地图单位，照抄老工程），贴地坪往上一点点/贴楼层顶往下一点点，中间
留一条窄缝，和相邻楼层的对应slab不共面。

楼梯/坡道网格摆放时`SpawnMesh`要按`.layout`模板里`Stair`/`Ramp`实际声明的footprint尺寸
（X/Y）+整层高度`floorHeight`（Z）缩放，和`SpawnCube`同一套"资产包围盒是边长
`BUILDING_CUBE_MESH_SIZE`(100)的正方体"约定（缩放系数=目标世界尺寸/这个边长）——这样不管
换成哪个美术资产，只要包围盒统一按这个单位大小做，就能直接互相替换，不用改代码。**第一版
实现完全没有调用`SetWorldScale3D`**，楼梯/坡道网格一直按资产原始大小摆放，和模板里
`Stair`/`Ramp`实际声明的尺寸对不上——PIE验证发现后补上，`SpawnMesh`签名新增`sizeX/sizeY/sizeZ`
三个参数。

**楼梯/坡道网格的朝向除了building自身的世界旋转，还要叠加这个楼梯/坡道自己的局部
`direction`**（`ComputeDirectionalMeshTransform`）——照抄老工程`ABuildingBase::
ConstructBuilding`里`GetRotation(stair.GetDirection())`那段：`NORTH=0°`、`EAST=90°`、
`SOUTH=180°`、`WEST=270°`，`WEST`/`EAST`方向还要交换`sizeX`/`sizeY`（网格局部坐标系默认
按`NORTH`朝向对齐世界X/Y，旋转90°/270°之后局部X/Y和世界X/Y互换，缩放必须按局部轴给，不然
会被拉伸成错误的长宽比）。老工程里这段角度是相对building自己`Actor`(已经摆好世界旋转)的
局部附加旋转，这次没有per-building的`Actor`，必须显式把这段角度和`building->GetRotation()`
相加才等价——**第一版实现完全没做这一步，所有building的楼梯/坡道网格只贴了building自身的
世界旋转，看起来永远朝同一个方向，和building实际朝哪条路完全无关**，PIE验证发现后补上。

### Room墙体不用`Floor::GetSingles()`/`GetRows()`

`Single`/`Row`是"模板槽位"，槽位数量/大小和实际生成的`Room`数量不是一一对应（`ArrangeRow`
会把一个row槽位切成好几个`Room`），而且门/窗/朝向在实例化时已经转移到`Room`身上（见
`Building::AssignRoom`/`ArrangeRow`）——渲染时改成遍历`building->GetRooms()`按
`GetLayer()==level`筛选，画每个真正`Room`自己的墙体（隐含4面都有墙）。

## LOD切换：一个节流队列，`TickComponent`每帧最多处理`maxLodOpsPerTick`条

`GenerateBuildings()`只同步建远处cube，**不建任何近处楼层几何**——近处楼层完全交给
`TickComponent`按距离增量构建。每个building维护一个`FBuildingRenderState`（`currentLod`/
`transitionPending`/`nearFloorCount`/`farSectionIndex`/`nearComponentsByFloor`，存在
`renderStates`这个`TMap<Building*,...>`里）。`TickComponent`每帧：

1. **入队**：遍历`renderStates`，跳过`transitionPending`为true的，按水平距离和`currentLod`
   比较决定要不要切换：
   - 近→远（`wantNear=false`且`currentLod==Near`）：入队`BuildFarMesh`+
     `DeleteAllNearMeshes`两条。
   - 远→近（`wantNear=true`且`currentLod==Far`）：入队`nearFloorCount`条`BuildFloorMesh`
     （每层一条）+最后一条`DeleteFarMesh`。
   - 入队后立刻把这栋building的`transitionPending`置`true`。
2. **drain**：从一个全局`TQueue<FBuildingLodOp>`最多取`maxLodOpsPerTick`（默认4，PIE验证
   发现8个/帧在建筑密集处仍然会卡顿后调低）条
   执行——**只有序列的最后一条**（`DeleteAllNearMeshes`/`DeleteFarMesh`）执行时才更新
   `currentLod`+清`transitionPending`，中间的`BuildFarMesh`/`BuildFloorMesh`只是建几何，
   不改状态。`BuildFloorMesh`这个op现在内部工作量比早期版本大得多（一整层的墙/地板/天花板/
   楼梯电梯井墙），但节流机制本身不用改。

## 依赖关系

- 依赖：`map/map.h`（`Map::GetBuildings()`）、`map/building.h`（`Building`/`Floor`/
  `Stair`/`Elevator`/`Ramp`/`Ceiling`/`Ground`/`Corridor`）、`map/room.h`（`Room`）、
  `map/geometry.h`、`ProceduralMeshComponent`（远处LOD）、`Components/
  StaticMeshComponent.h`（近处每段一个组件）、`UMaterialInstanceDynamic`、
  `Kismet/GameplayStatics.h`、`Containers/Queue.h`（`TQueue`）。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`
  （`EnsureMapGenerated()`在`GenerateZones`之后调用一次`GenerateBuildings`）。

## EndPlay：退出游戏/PIE停止时崩溃的修复

`AForeverFrameworkActor::EndPlay`会同步`delete map`(Core侧的`Map`，连带它拥有的所有
`Building`/`Room`/`Component`)，但那是Actor自己的`Map*`字段，和这个组件`GenerateBuildings()`
时缓存下来的`map`是两个不同的变量——Actor那边`delete`并不会连带把这个组件里的`map`也清空。
这个组件的`TickComponent`每帧都会解引用`renderStates`里存的`Building*`(`building->
GetPosX()`等)，如果不清空，`AForeverFrameworkActor::EndPlay`里`delete map`之后、这个组件
真正被引擎销毁之前万一还漏进来一帧Tick，就是野指针——PIE验证发现的"退出游戏崩溃"的根因。
修复：`UForeverBuildingFrameworkComponent`新增`EndPlay`重写，只把`map`置空（`TickComponent`
顶部`if (!map) return;`这一行本身就足够挡住后续所有解引用），不用手动清空`renderStates`/
`lodOpQueue`或`DestroyComponent`那些near-LOD组件——它们跟着owner一起被引擎正常GC掉即可。
`AActor::EndPlay`会自动分发调用每个`ActorComponent`自己的`EndPlay`，且分发发生在
`AForeverFrameworkActor::EndPlay`里`delete map`那两行之后（`Super::EndPlay(...)`那一行），
所以这里置空的时候`map`已经不是野指针，可以放心比较/赋值，只是不能再解引用——这也是为什么
只需要在这一个组件上加`EndPlay`：`Source/Forever/Framework`下其它会tick的组件
（`ForeverTerrainFrameworkComponent`）目前的`TickComponent`没有实现任何函数体，不会解引用
`map`拥有的任何数据，没有同样的风险，不需要一起改。

## 待办/后续阶段

- 电梯轿厢——这次只做井道墙体几何，不摆任何"轿厢"实体。
- 按mod自定义LOD切换距离/每帧队列上限——这次先用组件级`UPROPERTY`固定值(20/8)，不做到
  per-mod可配置。
- 3D(含高度)距离判定——这次用水平距离。
- 外墙/内墙材质区分——这次统一用一份`wallMaterial`，用户明确要求以后自己设计怎么分。
- 门的实际mesh——照抄老工程，纯几何缺口，不生成任何东西。
