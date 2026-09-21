# ForeverBuildingFrameworkComponent.h / .cpp

## 职责（架构第2版：每栋building一个专属Actor）

这个组件现在**不直接持有任何per-building的渲染状态或组件**。真正的楼体footprint/楼层/LOD
渲染/电梯轿厢逻辑全部在`Source/Forever/Element/BuildingElement.h/.cpp`
（`ABuildingElement`）——`GenerateBuildings(Map* inMap)`遍历`map->GetBuildings()`
（`unordered_map<string,Building*>`，寻址用，见`Source/Core/map/map.md`"寻址"一节，遍历用
结构化绑定），给每栋building各`SpawnActor`一个专属的`ABuildingElement`并调用`Init()`——
Element自己同步建好远处灰色cube作为基线状态，之后的LOD状态机、近/远处组件创建销毁、电梯
轿厢动画全部由Element自己的`Tick`驱动。**独立建筑和园区内部建筑一视同仁**——不区分
`building->GetParentZone()`是否为空，统一走同一套楼体footprint/楼层/LOD渲染。

这个组件保留的职责：
1. 集中加载/缓存全地图共用的默认资产（`cubeMesh`/`defaultStairMesh`/`defaultRampMesh`/
   `defaultCabinMesh`/`defaultWallMaterial`等，构造函数`ConstructorHelpers`加载，
   `ResolveMaterial`/`ResolveMesh`按软路径缓存），Element通过公开的`Get*`/`Resolve*`方法
   拿这些——按软路径缓存的东西不适合每个Element各自维护一份，会互相拿不到对方缓存的结果。
2. 通过`TryConsumeLodOpBudget()`给所有Element提供一个全局共享的"每帧最多处理几条LOD操作"
   预算，避免大量building同时穿越距离阈值时所有Element在同一帧一起疯狂建组件（见下面
   "LOD切换"一节）。

**为什么要拆成每栋building一个Actor**：早期实现是所有building的近处LOD组件全部
`NewObject(owner=这个框架组件所在的AForeverFrameworkActor单例)`，PIE验证发现这样会让
UE的物理引擎对同一个Actor根组件下的大量Static简单碰撞子组件做"焊接"（weld），焊接开销随
这个owner身上**已有组件总数**线性增长——全地图共用一个owner，就是所有building互相拖累，
地图越大/玩家探索得越久，新建一层楼越卡。改成每栋building专属Actor之后，这个爆炸范围被
关进了"单栋楼自己的组件数"（几百到几千），不会波及其它building，详见下面"性能"一节的完整
排查记录。

近处LOD不是"填满楼体子矩形的一个扁cube"，而是`Building`真正的楼层内部结构——走廊、房间
隔墙、门/窗洞缺口（窗户资产有问题，这次删掉了窗户网格显示，门/窗都只是纯几何缺口）、楼梯/
坡道实体、电梯井墙体、地板/天花板两层独立薄slab，数据全部来自`Source/Core/map/building.h`
的`Floor`/`Room`（`Building::GetFloor(level)`/`GetRooms()`），几何生成逻辑照抄老工程
`ABuildingBase::ConstructBuilding`/`ConstructQuad`。**以下几节描述的算法这次会话没有变化，
只是实现代码从这个组件搬到了`ABuildingElement`里**（原来的`Building* building`/
`FBuildingRenderState& state`两个参数，现在是Element自己的成员，函数签名相应简化）。

## 两级LOD

- **近处（`EBuildingLod::Near`）**：完整楼层内部结构，见"近处楼层几何"一节。
- **远处（`ELod::Far`）**：整栋一个box，贴`ResolveLodMaterial(building)`解析出来的
  材质——`Building::GetLodMaterialPath()`（转发`BuildingMod::lodMaterial`）非空时按路径
  `LoadObject`+创建MID（框架组件缓存，避免重复创建）；留空时用框架组件的`defaultLodMaterial`
  （`Pure`+`SetVectorParameterValue("Color", FLinearColor(0.5,0.5,0.5))`染灰）。每个
  `ABuildingElement`有自己专属的`farMesh`（`UProceduralMeshComponent`，只有1个section，
  固定index 0）——架构第2版之前是全地图building共用一个PMC、每栋分配不同section，现在
  每栋building已经是独立Actor，远处box也顺手一起变成"这栋楼自己的组件"，不再需要
  `nextFarSectionIndex`这种跨building的索引分配。
- **切换距离，两个阈值形成迟滞区间**：`lodNearEnterDistance`（默认20，远→近的触发距离）/
  `lodFarExitDistance`（默认40，近→远的触发距离），都是`UPROPERTY(EditDefaultsOnly)`，
  地图单位。近→远的阈值故意比远→近的大——已经精细化的building要离得更远才会退化成单box，
  防止玩家在临界距离附近小范围来回走动时反复Near/Far抖动式切换（每次切换都要建/删一整层楼
  的组件，抖动等于反复触发这次会话花大力气排查的那类卡顿）。按viewer
  （`UGameplayStatics::GetPlayerPawn(GetWorld(),0)->GetActorLocation()/
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

## LOD切换：每个Element自己的队列 + 框架组件的全局共享预算

`Init()`只同步建远处cube，**不建任何近处楼层几何**——近处楼层完全交给`ABuildingElement`
自己的`Tick`按距离增量构建。每个Element自己就是渲染状态（`currentLod`/`transitionPending`/
`nearFloorCount`/`nearComponentsByFloor`都是Element的成员，不再需要一个`TMap<Building*,
...>`去存"谁的状态是什么"——Element本身就对应一个building）。`ABuildingElement::Tick`
每帧：

1. **入队自己的队列**：跳过`transitionPending`为true的情况，按水平距离和`currentLod`比较
   决定要不要切换：
   - 近→远（`wantNear=false`且`currentLod==Near`）：入队`BuildFarMesh`+
     `DeleteAllNearMeshes`两条。
   - 远→近（`wantNear=true`且`currentLod==Far`）：入队`nearFloorCount`条`BuildFloorMesh`
     （每层一条）+`BuildElevatorCabins`+最后一条`DeleteFarMesh`。
   - 入队后立刻把`transitionPending`置`true`。
2. **drain自己的`TQueue<FLodOp>`**：每次执行一条之前，先向框架组件的
   `TryConsumeLodOpBudget()`申请一份全局共享预算（框架组件的`TickComponent`每帧把这份预算
   重置成`maxLodOpsPerTick`，默认4，PIE验证发现8个/帧在建筑密集处仍然会卡顿后调低）——申请
   失败（这一帧全地图的预算已经被其它building的Element用完）就先停手，下一帧再继续。**必须
   先判断自己的队列是否为空，再申请预算**，不能反过来：如果反过来，每一栋"这一帧其实无事
   可做"的building也会白白申请（并消耗）一份全局预算，挤占真正需要建楼层的building的份额。
   **只有序列的最后一条**（`DeleteAllNearMeshes`/`DeleteFarMesh`）执行时才更新`currentLod`+
   清`transitionPending`，中间的`BuildFarMesh`/`BuildFloorMesh`只是建几何，不改状态。

这个设计相比"一个全局队列+框架组件自己drain"（架构第1版）的取舍：具体的LOD状态机/队列
下放到每个Element自己身上（每个building的近/远状态、当前排队的操作，天然就该是这栋楼自己
的状态，不需要框架组件维护一个`TMap<Building*, ...>`去间接查），但保留了框架组件作为
"全局节流总闸"的角色——如果完全不设共享预算、让每个Element各自决定自己的每帧处理量，大量
building同时穿越距离阈值时（比如玩家瞬移/沿着阈值边界走）依然会在同一帧集中爆发，回到
"每次LOD切换都巨卡"的老问题。

## 冻结世界直到LOD切换队列清空（`RequestFreezeUntilLodSettled`）

游戏刚开始/`ChangeControlChange`切换玩家控制权的那一刻，附近building的近处LOD（楼层/
房间细节）可能还没排队建完——玩家/市民会先掉到还没生成细节的地面上（物理碰撞体缺失），
等建筑加载完才落地，观感很差。`UForeverStoryFrameworkComponent::ApplyControlChange`
每次成功`Possess`之后都会调用一次`RequestFreezeUntilLodSettled()`：直接
`UGameplayStatics::SetGlobalTimeDilation(world, 0.f)`把整个世界的时间倍率归零（物理/
移动全部停摆，市民不会掉空），`TickComponent`每帧检查`pendingLodTransitionCount`是否
清零，清零后自动恢复成`1.f`。

这个机制之所以能生效，关键在于**时间倍率只缩放`Tick`函数收到的`DeltaTime`参数，不会
阻止`Tick`函数本身按正常帧率被调用**——`frameOpBudgetRemaining`的重置、
`ABuildingElement::Tick`里drain`lodOpQueue`的那段循环都不读`DeltaTime`（见上"LOD状态机
下放到每个`ABuildingElement`自己身上"一节），所以"世界静止"期间LOD队列依然能照常按
每帧预算排空，不会因为倍率归零而卡死。`pendingLodTransitionCount`是一个全局计数器，由
`ABuildingElement`自己在`transitionPending`置`true`/`false`时同步调用
`NotifyLodTransitionStarted`/`NotifyLodTransitionFinished`维护——框架组件不需要反过来
遍历全地图所有`ABuildingElement`逐个查询各自的`transitionPending`，O(1)。

## 依赖关系

- 依赖：`Source/Forever/Element/BuildingElement.h`（`GenerateBuildings()`
  `SpawnActor<ABuildingElement>()`）、`map/map.h`（`Map::GetBuildings()`）、
  `map/building.h`（`Building`，只用来取`GetLodMaterialPath()`）、
  `UObject/ConstructorHelpers.h`（默认资产加载）、`UMaterialInstanceDynamic`、
  `Engine/Engine.h`（`GEngine->ForceGarbageCollection`，临时排查用）。真正的楼体几何生成
  依赖（`map/room.h`/`map/geometry.h`/`ProceduralMeshComponent`/`Components/
  StaticMeshComponent.h`/`Components/BoxComponent.h`/`Kismet/GameplayStatics.h`/
  `Containers/Queue.h`）现在都在`BuildingElement.cpp`里，这个组件自己不再需要。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`
  （`EnsureMapGenerated()`在`GenerateZones`之后调用一次`GenerateBuildings`）；
  `Source/Forever/Element/BuildingElement.cpp`通过`TWeakObjectPtr<
  UForeverBuildingFrameworkComponent>`反向调用`ResolveMaterial`/`ResolveMesh`/
  `Get*默认资产`/`GetLodSwitchDistance`/`GetCabinCruiseSpeed`/`GetCabinEaseSeconds`/
  `TryConsumeLodOpBudget`/`NotifyLodTransitionStarted`/`NotifyLodTransitionFinished`；
  `Source/Forever/Framework/ForeverStoryFrameworkComponent.cpp`（`ApplyControlChange`
  切换玩家控制权后调`RequestFreezeUntilLodSettled`）。

## EndPlay：退出游戏/PIE停止时崩溃的修复

这个组件自己的`TickComponent`现在只重置`frameOpBudgetRemaining`和跑临时GC排查日志，不解
引用`map`拥有的任何数据，理论上不加`EndPlay`也不会崩——但保留`EndPlay`置空`map`是为了和
其它Framework组件的约定保持一致，防止以后有人往这个组件的`TickComponent`里加新逻辑时忘了
这茬。

真正需要小心的是`ABuildingElement`（独立Actor，见`BuildingElement.h`），它的`Tick`每帧都
解引用自己的`building`成员。**关键点**：`ABuildingElement`和`AForeverFrameworkActor`是
两个不同的Actor，`AForeverFrameworkActor::EndPlay`里"先`delete map`、再`Super::EndPlay()`
触发自己组件的`EndPlay`"这套顺序保证，只对它自己的`ActorComponent`成立，不能假设也对
其它独立Actor的`EndPlay`调用顺序生效——UE在关卡卸载/PIE停止时对不同Actor的`EndPlay`调用
顺序不保证谁先谁后。`ABuildingElement`的安全性不依赖这个顺序：它只需要保证"自己的`EndPlay`
一跑完，自己的`Tick`就再也不会被调用"（这是UE对同一个Actor自身生命周期的基本保证，和其它
Actor的`EndPlay`顺序无关），所以`ABuildingElement::EndPlay`只需要把自己的`building`置空
就足够安全，不需要关心`AForeverFrameworkActor`那边`delete map`发生在什么时候。

## 碰撞检测（Building进入/离开，第N轮迁移新增；代码现在在`ABuildingElement`里）

`ABuildingElement::Init()`里`BuildFarSection()`之后额外调一次`BuildCollisionBox()`——
真正的`UBoxComponent`+`OnComponentBeginOverlap`/
`OnComponentEndOverlap`（用户已确认的方案，不是手动Tick轮询），碰撞Profile用UE自带的
`"Trigger"`预设。水平=body矩形（`ComputeBodyWorldCenter`，和远处LOD灰box同一个框），
垂直=整栋楼Z范围（`ComputeFullZRange`，和远处LOD灰box同一个公式），三个方向各**+0.01**
（地图单位，`BUILDING_COLLISION_MARGIN`，和`BUILDING_WALL_THICKNESS`同一个量级）——和Room
碰撞盒的各`-0.01`（见`ForeverRoomFrameworkComponent.md`）配对，两者贴合的边界（比如Room贴
building外墙的那一侧）不会因为完全重合而在Overlap判定上抖动。这个碰撞盒**常驻，不随近/远
LOD切换增删**（`ABuildingElement::collisionBox`，和LOD状态本身无关），`Init()`时创建一次，
一直存在到这个Element被销毁。

Overlap回调（`OnOverlapBegin/End`）只使用生成时预先烘焙好的`FString`显示名
（`building->GetAddress()`，存进`ABuildingElement::collisionLabel`——现在一个Element只
对应一个碰撞盒，不再需要像架构第1版那样用`TMap<UPrimitiveComponent*, FString>`查表），绝不
解引用`Building*`/`Map*`——和上面`EndPlay`那条安全原则完全一致，这也是为什么这个碰撞盒不
需要跟着`EndPlay`做任何特殊清理。`OtherActor`过滤成只认`UGameplayStatics::
GetPlayerPawn(GetWorld(),0)`，和LOD距离判定同一个约定。测试阶段用
`GEngine->AddOnScreenDebugMessage`（`FColor::Cyan`）代替真实的Story事件分发（老工程
`ABuildingBase::EnterBuilding/LeaveBuilding`接的是Story系统，这次Story域还没迁移）。

## 电梯轿厢（第N轮迁移新增：mod声明+3D网格+占位动画；代码现在在`ABuildingElement`里）

`BuildingMod::cabins`（`ElevatorCabinSpec`列表，`shaftIndex`/`minFloor`/`maxFloor`/
`cabinMeshPath`，见`Source/Dependence/map/building_mod.md`）由mod显式声明，`Building`
（Core侧）不处理这份数据，完全是`ABuildingElement`自己读`building->GetMod()->cabins`。

- **和近处LOD楼层几何同一套增删节奏，但单独一个op**：远→近切换时，在所有`BuildFloorMesh`
  之后、`DeleteFarMesh`之前新增一条`BuildElevatorCabins`（`BuildElevatorCabinsForBuilding`），
  近→远切换的`DeleteAllNearMeshes`（即`ClearNearSections`）一并清空
  `ABuildingElement::cabins`——轿厢是纯视觉+动画，没必要像Building/Room碰撞盒那样常驻，
  跟着近处LOD一起增删更省资源(看不见的轿厢没必要一直播动画)。
- **`BuildElevatorCabinsForBuilding`**：对每个`ElevatorCabinSpec`，用
  `building->GetFloor(spec.minFloor)->GetElevators()[spec.shaftIndex]`取井道的
  `Elevator`几何（越界就跳过，不报错）。**轿厢mesh和楼梯/坡道同一个`[0,1]`单位包围盒约定，
  需要按实际尺寸缩放+按方向旋转**（用户明确纠正了第一版"不缩放，只贴building旋转"的错误
  实现）——直接复用`ComputeDirectionalMeshTransform(elevator.GetDirection(), ...)`，
  资产默认路径`/Game/Asset/Meshes/Elevator.Elevator`（用户已经把这个mesh放进了新工程的
  Content目录，不需要从老工程拷贝）。**Z缩放用轿厢"家"所在楼层(`minFloor`)的层高，不是
  `minFloor`~`maxFloor`的整个垂直跨度**——那个跨度是轿厢的移动范围（`FBuildingCabin::
  zBottom/zTop`），不是它自身的缩放尺寸，这是实现时最容易搞混的一点。
- **动画：匀速+两端缓入缓出（用户已确认的方案，一直在`minFloor`到`maxFloor`之间往返）**：
  `ABuildingElement::Tick`里对`currentLod==Near`遍历自己的`cabins`，用匿名namespace的
  `ComputeCabinZ(zBottom, zTop, cabinCruiseSpeed, cabinEaseSeconds, elapsedSeconds)`
  算当前Z（`cabinCruiseSpeed`/`cabinEaseSeconds`通过`framework->GetCabinCruiseSpeed()`/
  `GetCabinEaseSeconds()`拿框架组件的全地图共享配置）——用平滑Hermite曲线
  `smoothstep(u)=3u²-2u³`做速度曲线：一段缓入时间`te`内速度从0平滑升到`cabinCruiseSpeed`，
  这段位移的精确解析式是`cabinCruiseSpeed*te*(u³-u⁴/2)`（`u=s/te`，`u=1`时正好是
  `cabinCruiseSpeed*te*0.5`——smoothstep在`[0,1]`上的积分均值精确等于`0.5`，不是近似），
  缓出段由缓入段的公式按"距终点还有多久"对称复用（减速曲线是加速曲线的时间倒放）。`te`按
  `min(cabinEaseSeconds, distance/cabinCruiseSpeed)`夹到不超过半程距离对应的时间，矮建筑/
  短距离会退化成"没有匀速段、纯缓入接缓出"，不会算出负的匀速时间。`cabinCruiseSpeed`
  (默认150 UE单位/秒≈1.5m/s)/`cabinEaseSeconds`(默认1.5秒)是框架组件的`UPROPERTY`，
  每个`FCabin`有独立的`phaseOffset`（按世界坐标错开），避免同一栋楼/相邻楼的轿厢看起来
  同步摆动。全程只用`FCabin`里缓存的浮点数（`worldX/worldY/zBottom/zTop/phaseOffset`），
  不解引用`building`——`Tick`本身仍然在`if (!building) return;`保护之下，不需要额外处理。

## 性能：Mobility必须是Static，不能用引擎默认的Movable（PIE验证发现"每次LOD切换都巨卡"）

**现象**：加载/切换一层楼的近处LOD耗时特别长，而且感觉越用越卡（同样的操作，地图跑一段时间
之后比刚进游戏时明显更慢）。

**根因**：`SpawnCube`/`SpawnMesh`（墙体分段/地板天花板slab/楼梯坡道）、
`ForeverBuildingFrameworkComponent`/`ForeverZoneFrameworkComponent`/
`ForeverRoomFrameworkComponent`三个组件的碰撞盒，全部`NewObject`之后只
`SetupAttachment`+`RegisterComponent`，从来没有调用过`SetMobility`——而
`AForeverFrameworkActor`的根组件`sceneRoot`同样没有显式设置mobility，`USceneComponent`
不设置时的引擎默认值是**`Movable`**。这意味着：
1. 挂在`sceneRoot`下面的每一个子组件，不管这次会话有没有手动设成`Static`，只要父级是
   `Movable`，实际效果都会被当成"和父级一样活跃"（父级理论上随时可能移动，子级的世界坐标
   就没法当成固定值预先烘焙/缓存）。
2. **`ForeverRoomFrameworkComponent::GenerateRooms`在地图初始化时一次性给全地图**
   **每一个Room**各生成一个`UBoxComponent`——数量级是成千上万，这些全部是"事实上永远不会
   移动、但被引擎当成Movable"的图元，一次性全部塞进渲染器的动态图元八叉树/物理引擎的动态
   broadphase。
3. 这两套结构（渲染器的动态图元追踪、物理引擎的动态broadphase）对"新增一个动态图元"的
   开销，会随着场景里已有的动态图元总数增长而变差——地图初始化时Room碰撞盒已经把这个基数
   堆得很高，之后**每次**近处LOD整层增删墙体分段/地板天花板slab（同样被当成Movable的新
   图元）就会明显更慢，而且随着玩家探索的building/room越来越多、动态图元只增不减，会越用
   越卡。

**修复**：
- `AForeverFrameworkActor`构造函数里给`sceneRoot`显式`SetMobility(EComponentMobility::
  Static)`——这个Actor整个生命周期都不移动，根组件本来就该是Static，这样挂在它下面、
  自己也设成Static的子组件才能真正生效为Static（Static子组件可以挂在Static父级下；
  反过来Static父级下也允许挂Movable子级，比如电梯轿厢——UE的约束方向是"子级不能比父级
  更静态"，不是反过来）。
- `SpawnCube`/`SpawnMesh`/三个组件各自的`BuildCollisionBox`：`NewObject`之后、
  `RegisterComponent()`之前调用`SetMobility(Static)`（`SpawnMesh`新增`isMovable`参数，
  只有电梯轿厢那次调用传`true`保持`Movable`，因为轿厢每帧要`SetWorldLocation`播动画）。
  **`SetWorldLocation`/`SetWorldRotation`/`SetWorldScale3D`这几行也要跟着挪到
  `RegisterComponent()`之前**——`Static`组件注册之后引擎就不允许再"移动"它了（哪怕只是
  摆放阶段的最终定位），必须在还没注册、只是在设置"初始变换"的阶段就把这些都设好。
- `buildingLodMesh`（远处LOD的单个PMC）同样补了`SetMobility(Static)`——虽然只有一个
  实例，数量级不是问题，但顺手保持一致、避免以后有人照抄这个写法时漏掉。
- Zone的围墙`UInstancedStaticMeshComponent`（`wallMeshInstances`）、Roadnet/Terrain的
  既有mesh组件这次没有一起改——它们要么是"少量ISM组件、内部批量放很多instance"（ISM本身
  不受这个问题影响，因为组件数量本来就少），要么不在这次排查的"building近处LOD卡顿"这个
  具体症状路径上，为了控制这次修复的范围没有顺手一起碰，但同样的Mobility检查思路以后如果
  在别处遇到类似"越用越卡"的症状，应该优先怀疑这一点。

## 性能第2轮：Mobility修复之后仍然卡，真凶是"全地图共用一个owner Actor"

Mobility修复（上一节）让卡顿从~5秒缓解到~2秒，但用户反馈依然明显卡顿。追加`[BuildFloorSection
耗时排查]`诊断日志（打印`building`/`level`/`rooms`/`components`/`elapsed`）之后做了几轮
排查：

1. 怀疑`bGenerateOverlapEvents`触发的初始重叠扫描——给`SpawnCube`/`SpawnMesh`加
   `SetGenerateOverlapEvents(false)`，验证后**没有明显改善**。
2. 怀疑`GenerateRooms()`/Building/Zone的常驻碰撞盒总数拖慢后续碰撞图元注册——依次临时禁用
   Room/Building/Zone三级碰撞盒生成，有改善但**没有解决**。
3. 怀疑`DestroyComponent()`之后对象堆积在待GC队列——加一段每5秒强制`GEngine->
   ForceGarbageCollection(true)`的诊断代码（现在还在`TickComponent`里，标了TODO），实测
   **强制GC本身恒为0ms**（说明根本没有可回收对象），GC前后耗时曲线毫无变化，**排除**。
4. 关键突破：额外打印`GetOwner()->GetComponents().Num()`，发现单组件耗时和"owner身上组件
   总数"几乎精确线性相关（常数~5e-5 ms/组件/owner组件数），且**在同一栋楼连续建多层楼、
   完全没有发生任何销毁的情况下**这个比值也在爬升——排除了"跨LOD切换的销毁堆积"，坐实是
   "owner身上已有组件越多，新组件的`RegisterComponent()`越慢"这个纯粹和owner绑定的机制
   （大概率是物理引擎对同一个Actor根组件下的大量Static简单碰撞子组件做的"焊接"（weld），
   每新增一个都要重算已有shape数组，开销随已焊接的shape数线性增长）。
5. **验证性修复**：给每栋building临时`SpawnActor`一个独立的挂载Actor，近处LOD组件全部
   改attach到这个专属Actor而不是框架单例owner——重测后`frameworkOwnerComponents`全程
   稳定在个位数/几十，不再增长；单组件耗时不再随"游戏时间/全地图组件总数"爬升，而是精确
   跟着"这栋楼自己的组件数"走，**换一栋新楼立刻重置回~0.1-0.15ms的低位**，和这是全场第几
   栋楼、玩了多久完全无关。这确认了根因就是"全地图所有building共用一个owner Actor"，验证
   实验的临时挂载Actor随后被正式graduate成了`ABuildingElement`（见本文档顶部"职责"一节和
   `Source/Forever/Element/BuildingElement.md`）——**只是把临时实验代码里的"mountActor"
   概念，正式变成一个有名字、有生命周期管理、承担LOD状态机的正经`AActor`子类**，不是另起
   炉灶的新设计。

**三级碰撞盒已按结论恢复**：Building/Room两级挪进了`ABuildingElement`（跟着这栋楼自己的
其它组件一起attach到专属Actor，见`Source/Forever/Element/BuildingElement.md`；
`UForeverRoomFrameworkComponent`恢复回空骨架，见`ForeverRoomFrameworkComponent.md`）；
Zone一级数量级不大（远小于Room），直接留在`ForeverZoneFrameworkComponent`的单例owner上，
没有必要为了这么小的数量单独拆Actor。

**残留问题（这次会话没有解决，留给后续）**：单栋building自己内部，随着这栋楼自己的楼层
一层层建起来，"这栋楼自己的组件数"还是会在一次近LOD转场里从0涨到这栋楼全部楼层的总和，
耗时也跟着涨（比如一栋39房间的大楼，转场末尾单组件耗时可能是转场开始时的2-3倍）——只是
这个涨幅现在被关在单栋楼内部，不会传染给其它building。如果某栋特别大的楼转场卡顿感还是
明显，可以考虑把挂载粒度从"每栋building一个Actor"再细化成"每栋building的每一层楼一个
Actor"，彻底把焊接爆炸范围锁定在单层楼的量级，但目前(建筑规模)没有验证这一步是否真的
必要。

## 待办/后续阶段

- 电梯轿厢的真实调度逻辑（呼叫按钮、开关门、载客、`Script`挂钩）——这次只有mod声明+3D网格+
  占位性质的往返动画。
- 按mod自定义LOD切换距离/每帧队列上限——这次先用组件级`UPROPERTY`固定值(20/40/2)，不做到
  per-mod可配置。
- 3D(含高度)距离判定——这次用水平距离。
- 外墙/内墙材质区分——这次统一用一份`wallMaterial`，用户明确要求以后自己设计怎么分。
- 门/窗的实际mesh——门照抄老工程本来就没有；窗户第一版有`Window.Window`网格，但资产本身
  有问题，用户明确要求删掉了窗户显示逻辑，现在门/窗都只是纯几何缺口，等以后有能用的窗户
  资产再考虑加回`SpawnMesh`调用（`ComputeDirectionalMeshTransform`这类朝向/尺寸换算可以
  直接复用楼梯/坡道那一套）。
