# ForeverBuildingFrameworkComponent.h / .cpp

## 职责

这个组件**不持有任何per-building的渲染状态或组件**。真正的楼体footprint/楼层/LOD渲染/
电梯轿厢逻辑全部在`Source/Forever/Element/BuildingElement.h/.cpp`
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

**为什么是每栋building一个专属Actor，不是全地图共用一个owner**：UE的物理引擎对"同一个
Actor根组件下挂大量Static简单碰撞子组件"会做"焊接"（weld），焊接开销随这个owner身上已有
组件总数线性增长——如果全地图所有building的近处LOD组件共用一个owner，就是所有building
互相拖累，地图越大/玩家探索得越久，新建一层楼越卡（完整排查过程见下面"性能排查"一节）。
拆成每栋building专属Actor之后，这个爆炸范围被关进了"单栋楼自己的组件数"（几百到几千），
不会波及其它building。

近处LOD不是"填满楼体子矩形的一个扁cube"，而是`Building`真正的楼层内部结构——走廊、房间
隔墙、门/窗洞缺口（门/窗都只是纯几何缺口，没有窗户网格——窗户资产有问题，已删掉窗户网格
显示）、楼梯/坡道实体、电梯井墙体、地板/天花板两层独立薄slab，数据全部来自
`Source/Core/map/building.h`的`Floor`/`Room`（`Building::GetFloor(level)`/`GetRooms()`），
几何生成逻辑照抄老工程`ABuildingBase::ConstructBuilding`/`ConstructQuad`。以下几节描述的是
算法本身；承载这些算法的代码在`ABuildingElement`里（原来的`Building*`/
`FBuildingRenderState&`两个参数，现在是Element自己的成员）。

- **0层且没有自定义模型的建筑不生成Element**——`GenerateBuildings`跳过
  `GetBasementCount()+GetLayerCount()==0`且`GetDecorations()`为空的building(例如只有植被的
  `ParkBuilding`)；0层但有自定义模型的(`MoharBuilding`)照常生成，模型靠Element的近处LOD加载，
  Element自己跳过远处方块和进出触发盒，见`BuildingElement.md`"自定义模型"一节。

## 两级LOD

- **近处（`EBuildingLod::Near`）**：完整楼层内部结构，见"近处楼层几何"一节。
- **远处（`ELod::Far`）**：整栋一个box，贴`ResolveLodMaterial(building)`解析出来的
  材质——`Building::GetLodMaterialPath()`（转发`BuildingMod::lodMaterial`）非空时按路径
  `LoadObject`+创建MID（框架组件缓存，避免重复创建）；留空时用框架组件的`defaultLodMaterial`
  （`Pure`+`SetVectorParameterValue("Color", FLinearColor(0.5,0.5,0.5))`染灰）。每个
  `ABuildingElement`有自己专属的`farMesh`（`UProceduralMeshComponent`，只有1个section，
  固定index 0）——每栋building已经是独立Actor，远处box也是"这栋楼自己的组件"，不需要跨
  building的section索引分配。
- **切换距离，两个阈值形成迟滞区间**：`lodNearEnterDistance`（默认40，远→近的触发距离）/
  `lodFarExitDistance`（默认80，近→远的触发距离），都是`UPROPERTY(EditDefaultsOnly)`，
  地图单位。近→远的阈值故意比远→近的大——已经精细化的building要离得更远才会退化成单box，
  防止玩家在临界距离附近小范围来回走动时反复Near/Far抖动式切换（每次切换都要建/删一整层楼
  的组件，抖动会反复触发下面"性能排查"一节的那类卡顿）。按viewer
  （`UGameplayStatics::GetPlayerPawn(GetWorld(),0)->GetActorLocation()/
  BUILDING_WORLD_SCALE`）到`building`世界中心的**水平（`FVector2D`）距离**判定，忽略高度差。

## 近处楼层几何：墙体/地板/天花板用ISM批量实例，楼梯/坡道/电梯轿厢仍是独立组件

墙体分段/地板slab/天花板slab按实际材质指针分组，每组一个常驻的
`UInstancedStaticMeshComponent`（`ABuildingElement::cubeInstancePools`，key是材质指针，
通常整栋楼只有墙/地/顶3个key，mod覆盖出不同材质时会多几个）。离开近处
(`ClearNearSections`)只是对每个ISM调`ClearInstances()`——ISM组件本身从`Init()`之后首次
用到某个材质时创建一次，常驻到这个`ABuildingElement`被销毁，近/远LOD反复切换不会重复
`RegisterComponent`/`DestroyComponent`。这个用法只对"整层/整栋同时批量增删"成立：建层
只追加实例（纯追加，不影响已有实例），清空用`ClearInstances()`（整体清空，不是逐个
`RemoveInstance`）——如果需要"单独删除/替换批量实例里的某一个"，`RemoveInstance`会导致
实例数组重排、牵连其它实例的索引，ISM在那种场景下不合适，但这里从不出现那种场景。碰撞显式
设成`"BlockAll"`（不依赖ISM默认碰撞值），墙体/地板/天花板只需要纯阻挡碰撞（挡住玩家走动、
挡住`ECC_Pawn`通道的开火线检测）。

**建层时`AddCubeInstance`不逐个调`AddInstance()`，攒够一层再批量提交**：计时诊断发现
单层墙体多的楼层（300+段）原来逐段调`UInstancedStaticMeshComponent::AddInstance()`要堆
到单帧90~120ms——有`BlockAll`碰撞的ISM每次单实例`AddInstance`都要同步建一次该实例的
碰撞体+更新渲染状态，这部分开销几乎线性于实例数（约0.14ms/实例）。现在`AddCubeInstance`
只是把world transform按材质分组暂存进`ABuildingElement::pendingCubeTransforms`
（`TMap<UMaterialInterface*, TArray<FTransform>>`），`BuildFloorSection`画完一层全部
墙体+地板天花板之后统一调`FlushCubeInstances()`，对每个材质各调一次批量API
`AddInstances(transforms, false, true)`，把实例缓冲区扩容/渲染状态更新/碰撞体创建合并
成一次，而不是逐个调用各付一次固定开销。

楼梯/坡道这类有真实3D资产的元素，和电梯轿厢(`isMovable=true`每帧播动画)、门
(`UForeverDoorComponent`，独立开合动画)不纳入ISM，用`SpawnMesh`各自生成独立
`UStaticMeshComponent`（不缩放，按网格自身大小摆放）——网格/材质/移动性各不相同、数量级
也小，继续走独立组件路径(`nearComponentsByFloor`现在只记录这部分)。

### 墙体开洞分段算法（`BuildWallsForElement`，照抄老工程`ConstructQuad`::`processFace`）

对`Corridor`/`Single`/`Row`（隐含4面都有墙）/`Stair`/`Elevator`/`Ramp`（按各自的
`GetWall(direction)`）的每一侧墙：把这一侧的门/窗开口（`WallHole`，沿墙方向ratio+offset）
按位置排序，在水平方向切分成若干段——没有开口就是1段整墙；有开口时，每个开口贡献最多3段
（开口前的墙段、开口上方过梁"仅当`y1>0`才有"、开口下方门槛/窗台"仅当开口没到地板才有"），
最后加一段trailing墙段。门/窗都只是缺口，不生成任何东西——窗户不摆网格，只保留开洞几何，
和门一样纯几何缺口；`FBuildingWallOpening`不区分门/窗，没有`isWindow`字段。墙体厚度固定
`BUILDING_WALL_THICKNESS`(0.01f，地图单位，照抄老工程写死值)。

**`y1`/`y2`是从天花板往下量的，不是从地板往上量**——完全照抄老工程`ConstructQuad::
processFace`的约定：`y1`是"开口顶到天花板"这段实心墙(过梁/门楣)的厚度，贴着天花板往下铺
（`if (y1>0)`才有）；`y2`是"开口底到天花板"的距离，`floorHeight-y2`才是"开口底到地板"这段
实心墙(门槛/窗台)的厚度，贴着地板往上铺（`if (floorHeight-y2>0)`才有）。两段的Z方向不能
搞反——门（`y1≈0`、`y2≈floorHeight`，开口几乎顶到天花板）如果把`y1`当成贴地板的门槛、
`floorHeight-y2`当成贴天花板的过梁，效果会和预期正好上下颠倒（对照老工程
`BuildingBase.cpp:400-404`：`op.y1>0`那段墙的Z中心是`center.Z + vertSpan/2 - op.y1/2`，
贴着天花板；`bH=vertSpan-op.y2`那段墙的Z中心是`center.Z - vertSpan/2 + bH/2`，贴着地板）。

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
组件默认，不分内外墙，统一一份默认墙体材质）+`stairMeshPath`/`rampMeshPath`
（`ResolveMesh`，留空用组件默认的`Stair.Stair`/`Ramp.Ramp`）。地板/天花板两层独立薄slab
（`kSlabThickness=0.02f`地图单位，照抄老工程），贴地坪往上一点点/贴楼层顶往下一点点，中间
留一条窄缝，和相邻楼层的对应slab不共面。

楼梯/坡道网格摆放时`SpawnMesh`按`.layout`模板里`Stair`/`Ramp`实际声明的footprint尺寸
（X/Y）+整层高度`floorHeight`（Z）缩放，和`SpawnCube`同一套"资产包围盒是边长
`BUILDING_CUBE_MESH_SIZE`(100)的正方体"约定（缩放系数=目标世界尺寸/这个边长）——这样不管
换成哪个美术资产，只要包围盒统一按这个单位大小做，就能直接互相替换，不用改代码。

**楼梯/坡道网格的朝向除了building自身的世界旋转，还要叠加这个楼梯/坡道自己的局部
`direction`**（`ComputeDirectionalMeshTransform`）——照抄老工程`ABuildingBase::
ConstructBuilding`里`GetRotation(stair.GetDirection())`那段：`NORTH=0°`、`EAST=90°`、
`SOUTH=180°`、`WEST=270°`，`WEST`/`EAST`方向还要交换`sizeX`/`sizeY`（网格局部坐标系默认
按`NORTH`朝向对齐世界X/Y，旋转90°/270°之后局部X/Y和世界X/Y互换，缩放必须按局部轴给，不然
会被拉伸成错误的长宽比）。这段角度相对building自身世界旋转是局部附加旋转，必须显式和
`building->GetRotation()`相加才等价——漏加这一步的话所有building的楼梯/坡道网格会只贴
building自身的世界旋转，看起来永远朝同一个方向，和building实际朝哪条路无关。

### Room墙体不用`Floor::GetSingles()`/`GetRows()`

`Single`/`Row`是"模板槽位"，槽位数量/大小和实际生成的`Room`数量不是一一对应（`ArrangeRow`
会把一个row槽位切成好几个`Room`），而且门/窗/朝向在实例化时已经转移到`Room`身上（见
`Building::AssignRoom`/`ArrangeRow`）——渲染时改成遍历`building->GetRooms()`按
`GetLayer()==level`筛选，画每个真正`Room`自己的墙体（隐含4面都有墙）。

## LOD切换：每个Element自己的队列 + 框架组件的全局共享预算

`Init()`只同步建远处cube，**不建任何近处楼层几何**——近处楼层完全交给`ABuildingElement`
自己的`Tick`按距离增量构建。每个Element自己就是渲染状态（`currentLod`/`transitionPending`/
`nearFloorCount`/`nearComponentsByFloor`都是Element的成员，不需要一个`TMap<Building*,
...>`去存"谁的状态是什么"——Element本身就对应一个building）。`ABuildingElement::Tick`
每帧：

1. **入队自己的队列**：跳过`transitionPending`为true的情况，按水平距离和`currentLod`比较
   决定要不要切换：
   - 近→远（`wantNear=false`且`currentLod==Near`）：往`floorTierOpQueue`入队`BuildFarMesh`+
     `DeleteAllNearMeshes`两条，`framework->NotifyFloorTierOpEnqueued(2)`。
   - 远→近（`wantNear=true`且`currentLod==Far`）：往`floorTierOpQueue`入队
     `nearFloorCount`条`BuildFloorMesh`（每层一条）+`BuildElevatorCabins`+最后一条
     `DeleteFarMesh`，`NotifyFloorTierOpEnqueued(nearFloorCount+2)`；紧接着调用
     `GatherPendingDoors()`收集这栋楼全部待建门，往`doorTierOpQueue`入队对应批数的
     `BuildDoorBatch`（门单独走低优先级队列，详见`BuildingElement.md`"门加载优先级"
     一节，以及下面"门和楼层分队列、分优先级"一节）。
   - 入队后立刻把`transitionPending`置`true`。
2. **drain自己的两个队列，楼层级永远先尝试**：每次执行一条之前，先向框架组件的
   `TryConsumeLodOpBudget(bool isDoorTierOp)`申请一份全局共享预算（框架组件的
   `TickComponent`每帧把这份预算重置成`maxLodOpsPerTick`，默认1——建筑密集处更高的值
   仍然会卡顿）——申请失败（这一帧全地图的预算已经被其它building的Element用完，或者
   申请的是门级操作但全地图还有楼层级操作未完成）就先停手，下一帧再继续。**必须先判断
   自己的队列是否为空，再申请预算**，不能反过来：如果反过来，每一栋"这一帧其实无事可做"
   的building也会白白申请（并消耗）一份全局预算，挤占真正需要建楼层的building的份额。
   **只有序列的最后一条**（`DeleteAllNearMeshes`/`DeleteFarMesh`）执行时才更新`currentLod`+
   清`transitionPending`，中间的`BuildFarMesh`/`BuildFloorMesh`只是建几何，不改状态。

这个设计的取舍：具体的LOD状态机/队列下放到每个Element自己身上（每个building的近/远状态、
当前排队的操作，天然就该是这栋楼自己的状态），但保留了框架组件作为"全局节流总闸"的角色——
如果完全不设共享预算、让每个Element各自决定自己的每帧处理量，大量building同时穿越距离
阈值时（比如玩家瞬移/沿着阈值边界走）依然会在同一帧集中爆发，回到"每次LOD切换都巨卡"的
老问题。

## 门和楼层分队列、分优先级

门系统上线后发现：每扇门的`UForeverDoorComponent::Init`要花4~6次`RegisterComponent()`，
如果跟墙体/楼梯/地板一起塞进同一个`BuildFloorMesh`操作单位，这部分开销会被悄悄摊进"建
一层楼"的预算里，加门之后楼层加载明显变卡。解法是把门拆成独立的`BuildDoorBatch`操作类型
（见`BuildingElement.h`的`ELodOpType`），并给它一套**全局优先级门槛**：

```cpp
bool TryConsumeLodOpBudget(bool isDoorTierOp) {
    if (isDoorTierOp && pendingFloorTierOps > 0) return false;
    if (frameOpBudgetRemaining <= 0) return false;
    frameOpBudgetRemaining--;
    return true;
}
```

`pendingFloorTierOps`是跨所有building共享的全局计数（不是per-building），由
`ABuildingElement`在排队/执行楼层级操作（`BuildFarMesh`/`DeleteAllNearMeshes`/
`BuildFloorMesh`/`BuildElevatorCabins`/`DeleteFarMesh`，不含`BuildDoorBatch`）时成对
调用`NotifyFloorTierOpEnqueued`/`NotifyFloorTierOpExecuted`维护。只要这个计数大于0
（即全地图任意一栋building还有楼层级操作排队未执行），所有building的门级请求都会被
拒绝——这正是"任意建筑的楼层加载插队到任意建筑的门加载前面"的实现关键：不需要
building互相感知彼此是谁、在干什么，只需要都查询这同一份全局计数就天然实现了跨建筑
优先级。

这套计数和`pendingLodTransitionCount`（见下面"冻结世界"一节）是两件不同的事，不要
混用：`pendingLodTransitionCount`按"转场"计数（一栋楼一次转场=1，不管排了几条操作），
给冻结世界用；`pendingFloorTierOps`按"操作条数"计数，只给门级操作的优先级门槛用。
`maxDoorsPerOp`（默认4，`EditDefaultsOnly`可调）控制每条`BuildDoorBatch`处理几扇门，
对应用户"先试一帧4扇"的原始诉求。

每个Element自己也要把`floorTierOpQueue`/`doorTierOpQueue`拆成两个独立的`TQueue`，不能
用一个混合队列靠`Peek()`队首类型判断——否则一栋楼自己排在队尾的楼层级操作会被排在队头、
永远等不到执行机会的自己的门操作挡住（队头的门操作被全局门槛拒绝出队，而解除这个门槛
恰恰需要先执行到排在它后面的那些楼层级操作），变成自己造成的死锁，而且因为门槛是全局的
会连带卡死全地图所有建筑的门加载。两个独立队列彼此不互相阻塞，不会有这个问题，详见
`BuildingElement.md`"门加载优先级"一节。

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
`ABuildingElement::Tick`里drain`floorTierOpQueue`/`doorTierOpQueue`的那两段循环都不读
`DeltaTime`，所以"世界静止"期间LOD队列依然能照常按每帧预算排空，不会因为倍率归零而
卡死。门级操作此时仍然受全局优先级门槛约束——如果恰好有building在冻结期间还有楼层级
操作排队，门会继续等，但这不影响冻结判定本身（判定只看楼层是否就位，见下面
`pendingLodTransitionCount`）。`pendingLodTransitionCount`
是一个全局计数器，由`ABuildingElement`自己在`transitionPending`置`true`/`false`时同步
调用`NotifyLodTransitionStarted`/`NotifyLodTransitionFinished`维护——框架组件不需要反过来
遍历全地图所有`ABuildingElement`逐个查询各自的`transitionPending`，O(1)。

## EndPlay安全性

这个组件自己的`TickComponent`只重置`frameOpBudgetRemaining`，不解引用`map`拥有的任何
数据，但仍保留`EndPlay`置空`map`，和其它Framework组件的约定保持一致，防止以后有人往
`TickComponent`里加新逻辑时忘了这茬。

真正需要小心的是`ABuildingElement`（独立Actor，见`BuildingElement.md`），它的`Tick`每帧
都解引用自己的`building`成员。**关键点**：`ABuildingElement`和`AForeverFrameworkActor`是
两个不同的Actor，`AForeverFrameworkActor::EndPlay`里"先`delete map`、再`Super::EndPlay()`
触发自己组件的`EndPlay`"这套顺序保证，只对它自己的`ActorComponent`成立，不能假设对其它
独立Actor的`EndPlay`调用顺序也生效——UE在关卡卸载/PIE停止时对不同Actor的`EndPlay`调用
顺序不保证谁先谁后。`ABuildingElement`的安全性不依赖这个顺序：它只需要保证"自己的
`EndPlay`一跑完，自己的`Tick`就再也不会被调用"（UE对同一个Actor自身生命周期的基本保证，
和其它Actor的`EndPlay`顺序无关），所以`ABuildingElement::EndPlay`只需要把自己的
`building`置空就足够安全。

## 碰撞检测（Building进入/离开）

`ABuildingElement::Init()`里`BuildFarSection()`之后额外调一次`BuildCollisionBox()`——
真正的`UBoxComponent`+`OnComponentBeginOverlap`/`OnComponentEndOverlap`（不是手动Tick
轮询），碰撞Profile用UE自带的`"Trigger"`预设。水平=body矩形（`ComputeBodyWorldCenter`，
和远处LOD灰box同一个框），垂直=整栋楼Z范围（`ComputeFullZRange`，和远处LOD灰box同一个
公式），三个方向各**+0.01**（地图单位，`BUILDING_COLLISION_MARGIN`，和
`BUILDING_WALL_THICKNESS`同一个量级）——和Room碰撞盒的各`-0.01`（见
`ForeverRoomFrameworkComponent.md`）配对，两者贴合的边界（比如Room贴building外墙的那一侧）
不会因为完全重合而在Overlap判定上抖动。这个碰撞盒**常驻，不随近/远LOD切换增删**
（`ABuildingElement::collisionBox`），`Init()`时创建一次，一直存在到这个Element被销毁。

Overlap回调（`OnOverlapBegin/End`）只使用生成时预先烘焙好的`FString`显示名
（`building->GetAddress()`，存进`ABuildingElement::collisionLabel`，一个Element只对应
一个碰撞盒），绝不解引用`Building*`/`Map*`——和上面`EndPlay`那条安全原则完全一致。
`OtherActor`过滤成只认`UGameplayStatics::GetPlayerPawn(GetWorld(),0)`，和LOD距离判定
同一个约定。测试阶段用`GEngine->AddOnScreenDebugMessage`（`FColor::Cyan`）代替真实的
Story事件分发。

## 电梯轿厢

电梯的状态(所在楼层、行程、Script、交互选项)在Core的`Cabin`里(`building->GetCabinEntities()`)，见
`Source/Core/map/cabin.md`。`ABuildingElement`只负责两件事：

- **推进行程(不管LOD)**：`Tick`最前面(玩家Pawn检查之前)调`UpdateCabins`，对每台`IsMoving()`的电梯
  `Advance(dt)`，用`StepCabin`按(高度, 速度)状态积分(加速度=`cabinCruiseSpeed/cabinEaseSeconds`，中途改目标速度连续)写回
  `SetCurrentZ`/`SetVelocity`，到站调`Arrive`。远景下电梯照样会到站。曲线是"匀速+两端缓入缓出"：smoothstep速度曲线，
  缓入段位移`cabinCruiseSpeed*te*(u³-u⁴/2)`，缓出段对称复用；`te`夹到不超过半程对应时间。
  `cabinCruiseSpeed`(默认300 UE单位/秒)/`cabinEaseSeconds`(默认1.5秒)是本组件的`UPROPERTY`。
- **近处LOD的表现**：远→近切换时在所有`BuildFloorMesh`之后跑`BuildElevatorCabins`
  (`BuildElevatorCabinsForBuilding`)，近→远的`ClearNearSections`一并清空。对每台Core电梯：
  - 轿厢网格：用`minFloor`那一层模板的这口井定位/定尺寸(`ComputeDirectionalMeshTransform`，资产默认
    `/Game/Asset/Meshes/Elevator.Elevator`，Z缩放用`minFloor`的层高)，**从Core记录的当前高度生成**；
    `UpdateCabins`在近处每帧按Core高度摆放。
  - 交互触发盒(`SpawnCabinTrigger`，Trigger碰撞预设)：轿厢内一个(宽/高为轿厢的`kElevatorInnerScale`=0.8，沿开口方向
    深度为轿厢进深的`kElevatorInnerDepthScale`=0.5、贴着轿厢门平面，Movable，跟着轿厢移动)；这台电梯负责的每一层电梯
    正面一个——用那一层模板自己的这口井定位，在开口(`Elevator::GetDirection()`那一侧)外放宽=开口宽×0.8、高=层高×0.8
    (`kElevatorMarkerScale`)、深`kElevatorFrontDepth`=1米且贴着开口平面的盒子，两个盒子之间不留缝，
    某层模板没有这口井就跳过。玩家进入任意一个弹出电梯选项(按计数防重复)，全部离开收起；
    `ClearNearSections`销毁前先解绑回调，玩家还在里面就显式收起。

## 性能排查：组件挂载粒度决定物理引擎碰撞焊接开销

**现象**：加载/切换一层楼的近处LOD耗时特别长，且随地图运行时间越用越卡。

**根因**：UE对"同一个Actor根组件下挂大量Static简单碰撞子组件"会做"焊接"（weld），新增
一个子组件的开销正比于这个owner身上已有组件总数。两个独立因素共同导致症状：

1. **Mobility没有显式设成Static**：`SpawnCube`/`SpawnMesh`、三个Framework组件的碰撞盒、
   `AForeverFrameworkActor`根组件`sceneRoot`，全部`NewObject`之后没有调用`SetMobility`，
   `USceneComponent`不设置时引擎默认值是`Movable`——父级`Movable`时子级即使手动设了
   `Static`也不会真正生效为Static（子级不能比父级更静态）。地图初始化时`Room`碰撞盒一次性
   生成成千上万个，全部被当成"事实上不动但引擎当成Movable"的动态图元，把渲染器/物理引擎的
   动态结构基数堆得很高，后续每次近处LOD整层增删都明显更慢，且随玩家探索范围扩大越用越卡。
2. **全地图所有building的近处LOD组件共用一个owner Actor**：即使修好Mobility，单组件耗时
   仍然和"owner身上组件总数"几乎精确线性相关，且在同一栋楼连续建多层楼、完全没有发生任何
   销毁的情况下这个比值也在爬升——排除了销毁堆积，坐实是物理引擎对同一个owner根组件下的
   大量Static碰撞子组件做焊接，每新增一个都要重算已有shape数组，开销随已焊接的shape数
   线性增长。

**修复**：
- `AForeverFrameworkActor`构造函数给`sceneRoot`显式`SetMobility(Static)`；
  `SpawnCube`/`SpawnMesh`/各组件`BuildCollisionBox`在`NewObject`之后、`RegisterComponent()`
  之前调用`SetMobility(Static)`（`SpawnMesh`的`isMovable`参数只有电梯轿厢那次调用传
  `true`，因为轿厢每帧要`SetWorldLocation`播动画）；`SetWorldLocation`/`Rotation`/
  `Scale3D`这几行也要跟着挪到`RegisterComponent()`之前——`Static`组件注册之后引擎不允许
  再"移动"它。Zone的围墙ISM/Roadnet/Terrain的既有mesh组件没有一起改——它们组件数量本来就
  少，不受这个问题影响；同样的Mobility检查思路以后遇到类似"越用越卡"的症状应该优先怀疑。
- **每栋building拆成专属`ABuildingElement`Actor**，近处LOD组件全部attach到这个专属Actor
  而不是框架单例owner——这样"owner身上组件总数"被关进单栋楼自己的量级，不再随全地图组件
  总数/游戏时间增长，换一栋新楼立刻重置回低位。Building/Room两级碰撞盒挪进了
  `ABuildingElement`（见`Source/Forever/Element/BuildingElement.md`），
  `UForeverRoomFrameworkComponent`恢复回空骨架（见`ForeverRoomFrameworkComponent.md`）；
  Zone一级数量级不大（远小于Room），直接留在`ForeverZoneFrameworkComponent`的单例owner上。
- **墙体/地板/天花板cube改成ISM**（见上"近处楼层几何"一节）：拆Actor之后残留的问题是单栋
  building内部——"这栋楼自己的组件数"仍然会随楼层累积，单组件注册耗时跟着涨（虽然不再传染
  给其它building）。改成按材质分组的常驻ISM之后，近/远LOD切换只是`AddInstance`/
  `ClearInstances()`，不再反复`RegisterComponent`/`DestroyComponent`，消除了这条残留曲线。

如果某栋特别大的楼转场卡顿感仍然明显，可以考虑把挂载粒度从"每栋building一个Actor"再
细化成"每栋building的每一层楼一个Actor"，但目前(建筑规模)没有验证这一步是否真的必要。

## 依赖关系

- 依赖：`Source/Forever/Element/BuildingElement.h`（`GenerateBuildings()`
  `SpawnActor<ABuildingElement>()`）、`map/map.h`（`Map::GetBuildings()`）、
  `map/building.h`（`Building`，只用来取`GetLodMaterialPath()`）、
  `UObject/ConstructorHelpers.h`（默认资产加载）、`UMaterialInstanceDynamic`、
  `Engine/Engine.h`。真正的楼体几何生成依赖（`map/room.h`/`map/general_geometry.h`/
  `ProceduralMeshComponent`/`Components/StaticMeshComponent.h`/`Components/
  BoxComponent.h`/`Kismet/GameplayStatics.h`/`Containers/Queue.h`）现在都在
  `BuildingElement.cpp`里，这个组件自己不再需要。
- 被谁依赖：`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`
  （`EnsureMapGenerated()`在`GenerateZones`之后调用一次`GenerateBuildings`）；
  `Source/Forever/Element/BuildingElement.cpp`通过`TWeakObjectPtr<
  UForeverBuildingFrameworkComponent>`反向调用`ResolveMaterial`/`ResolveMesh`/
  `Get*默认资产`/`GetLodSwitchDistance`/`GetCabinCruiseSpeed`/`GetCabinEaseSeconds`/
  `TryConsumeLodOpBudget`/`NotifyLodTransitionStarted`/`NotifyLodTransitionFinished`/
  `NotifyFloorTierOpEnqueued`/`NotifyFloorTierOpExecuted`/`GetMaxDoorsPerOp`；
  `Source/Forever/Framework/ForeverStoryFrameworkComponent.cpp`（`ApplyControlChange`
  切换玩家控制权后调`RequestFreezeUntilLodSettled`）。

## 待办/后续阶段

- 电梯轿厢的真实调度逻辑（呼叫按钮、开关门、载客、`Script`挂钩）——这次只有mod声明+3D网格+
  占位性质的往返动画。
- 按mod自定义LOD切换距离/每帧队列上限——这次先用组件级`UPROPERTY`固定值(40/80/4)，不做到
  per-mod可配置。
- 3D(含高度)距离判定——这次用水平距离。
- 外墙/内墙材质区分——这次统一用一份`wallMaterial`。
- 门/窗的实际mesh——门照抄老工程本来就没有；窗户资产有问题，已删掉窗户显示逻辑，现在门/窗
  都只是纯几何缺口，等以后有能用的窗户资产再考虑加回`SpawnMesh`调用（`
  ComputeDirectionalMeshTransform`这类朝向/尺寸换算可以直接复用楼梯/坡道那一套）。
