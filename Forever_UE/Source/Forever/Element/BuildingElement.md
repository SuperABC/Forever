# BuildingElement.h / .cpp

## 职责

每栋building一个专属的`AActor`，替代早期"全地图所有building共用
`UForeverBuildingFrameworkComponent`所在的单例`AForeverFrameworkActor`当owner"的做法——
后者会让物理引擎对同一个Actor根组件下的大量Static简单碰撞子组件做"焊接"（weld），焊接开销
随这个owner身上已有组件总数线性增长，导致地图越大/玩得越久，新建一层楼越卡（完整排查过程见
`Source/Forever/Framework/ForeverBuildingFrameworkComponent.md`"性能第2轮"一节）。

`UForeverBuildingFrameworkComponent::GenerateBuildings()`在`Map::InitBuildings()`跑完后
为每栋building各`SpawnActor`一个`ABuildingElement`并调用`Init(building, framework)`。
这个Actor自己承担：

- 两级LOD状态机（近处楼层内部结构 / 远处整栋一个灰box）+距离触发的近远切换。
- 近处所有独立组件（墙体分段/地板天花板slab/楼梯坡道网格）+远处LOD的`farMesh`（自己专属
  的`UProceduralMeshComponent`，只有1个section）+电梯轿厢mesh，全部attach到这个Actor
  自己的`elementRoot`，不再attach到框架组件的owner。
- 电梯轿厢往返动画。
- building自己的进入/离开检测碰撞盒（`BuildCollisionBox`）。
- 这栋楼所有Room各自的进入/离开检测碰撞盒（`BuildRoomCollisionBoxes`，原来在
  `UForeverRoomFrameworkComponent`里，同一套"全地图共用owner"问题挪过来的历史见下面
  "关键设计"一节，以及`ForeverRoomFrameworkComponent.md`）。

具体的墙体开洞分段算法/坐标转换/材质网格解析/电梯轿厢动画公式详见
`ForeverBuildingFrameworkComponent.md`（算法本身的说明放在那份文档，函数直接用Element
自己的成员，不传`Building*`/`FBuildingRenderState&`）。这份文档只记录
`ABuildingElement`这个类本身的设计取舍。

## 关键设计

### 为什么是独立Actor而不是继续用框架组件的owner

见`ForeverBuildingFrameworkComponent.md`"性能第2轮"一节的完整排查记录。简单说：UE对
"同一个Actor根组件下挂大量Static简单碰撞子组件"有一个已知的性能特性——新增子组件的开销
正比于这个Actor身上已有的组件总数。全地图所有building共用一个owner，等于所有building
互相拖累；拆成每栋building一个Actor之后，这个爆炸范围被关进了"单栋楼自己的组件数"（几百
到几千），不会波及其它building。

### 全地图共用的东西不下放，通过`framework`弱引用回调

材质/网格默认值（`cubeMesh`/`defaultWallMaterial`等）和它们按软路径的缓存
（`lodMaterialCache`/`meshCache`）、LOD切换距离/电梯速度等配置，都还留在
`UForeverBuildingFrameworkComponent`上，`ABuildingElement`只存一个
`TWeakObjectPtr<UForeverBuildingFrameworkComponent> framework`，需要的时候调用
`framework->ResolveMaterial(...)`/`framework->GetDefaultWallMaterial()`等。**这些东西
不适合让每个Element各自维护一份**——按软路径缓存的意义就是"同一个软路径只`LoadObject`一次"，
如果每个Element各自一份缓存，效果等于完全没缓存（每栋building第一次用到某个软路径都要
重新加载一次）。用`TWeakObjectPtr`而不是裸指针，是因为理论上框架组件可能先于某个Element
被销毁（虽然实际生命周期里不会发生，但`TWeakObjectPtr`几乎没有额外成本，比裸指针更安全）。

### 墙体/地板/天花板cube：批量提交，不逐个调`AddInstance()`

计时诊断发现：`BuildFloorSection`单层平均要花30ms、墙体分段多的楼层最高到120ms，和
这一层cube实例数几乎线性相关（约0.14ms/实例）——根因是`AddCubeInstance`原来每个cube
单独调一次`UInstancedStaticMeshComponent::AddInstance()`，而ISM设的是`BlockAll`碰撞，
每次单实例`AddInstance`都要同步建一次该实例的碰撞体+更新一次渲染状态，300+段的楼层
等于300多次独立的"同步建物理体"。

现在改成**收集-批量提交**两阶段：`AddCubeInstance`不再直接调`ism->AddInstance()`，
只是把world transform按材质分组追加进`pendingCubeTransforms`
（`TMap<UMaterialInterface*, TArray<FTransform>>`，`BuildingElement.h`新增成员）；
`BuildFloorSection`画完这一层全部墙体+地板天花板之后，调用`FlushCubeInstances()`——
对每个用到的材质各调一次`ism->AddInstances(transforms, false, true)`（批量API，
`Engine/Classes/Components/InstancedStaticMeshComponent.h`），把实例缓冲区扩容/渲染
状态更新/碰撞体创建合并成一次，而不是N次。`pendingCubeTransforms`只在一次
`BuildFloorSection`调用内部临时持有，提交完立刻清空，不跨帧/跨楼层残留。

### 全局LOD操作预算：`TryConsumeLodOpBudget(bool isDoorTierOp)`

每个Element自己维护**两个**`TQueue<FLodOp>`——`floorTierOpQueue`（墙体/楼梯/电梯井/
地板天花板/远近切换）和`doorTierOpQueue`（只放`BuildDoorBatch`）——但**执行速度**受框架
组件`TryConsumeLodOpBudget()`节流——框架组件的`TickComponent`每帧把一个共享计数器
`frameOpBudgetRemaining`重置成`maxLodOpsPerTick`，所有Element的`Tick`在真正执行一条操作
之前都要先来申请一份，申请失败就等下一帧再试。这是"状态机下放到每个Element"和"全局节流
不能丢"两个要求的折中：如果完全不设共享预算，大量building同时穿越距离阈值时（比如玩家
瞬移，或者沿着阈值边界走）依然会在同一帧集中爆发，重新变成"每次LOD切换都巨卡"。

`Tick`里申请预算的循环**必须先判断自己的队列是否为空，再申请预算**：
```cpp
while (!floorTierOpQueue.IsEmpty() && framework->TryConsumeLodOpBudget(false)) { ... }
while (!doorTierOpQueue.IsEmpty() && framework->TryConsumeLodOpBudget(true)) { ... }
```
不能反过来写成`framework->TryConsumeLodOpBudget(...) && queue.Dequeue(op)`——那样即使
这一帧这栋building根本没有排队的操作，也会先申请（并消耗）一份全局预算，只是申请到了却没
东西可执行，白白挤占了其它真正需要建楼层的building的份额。楼层级队列永远先尝试：只要
全地图任意一栋building还有楼层级操作排队未执行，门级请求一律被拒绝，见下面"门加载
优先级"一节。

### `EndPlay`安全性不依赖和框架Actor的调用顺序

`ABuildingElement`和`AForeverFrameworkActor`是两个独立的Actor。`AForeverFrameworkActor::
EndPlay`里"先`delete map`、再`Super::EndPlay()`触发自己组件的`EndPlay`"这套顺序保证，
只对它自己的`ActorComponent`（比如`UForeverBuildingFrameworkComponent`）成立——UE在关卡
卸载/PIE停止时，不同Actor之间`EndPlay`的调用顺序不保证谁先谁后。`ABuildingElement`的
安全性设计不依赖这个顺序：它只需要保证"自己的`EndPlay`一跑完，自己的`Tick`就再也不会被
调用"（这是UE对同一个Actor自身生命周期的基本保证，和其它Actor的`EndPlay`顺序无关），所以
`ABuildingElement::EndPlay`只把自己的`building`置空就足够安全。

### Room碰撞盒从`UForeverRoomFrameworkComponent`挪过来

原来Room的进入/离开检测碰撞盒在独立的`UForeverRoomFrameworkComponent::GenerateRooms`里
生成，全地图所有Room（数量级可能是成千上万）全部`NewObject`在那个组件所在的单例
`AForeverFrameworkActor`上——踩的是和building近处LOD组件完全一样的坑（全地图共用一个
owner，被物理引擎的碰撞体焊接开销拖累）。这次直接把这部分逻辑挪进
`BuildRoomCollisionBoxes()`，跟着这栋楼自己的其它组件一起attach到`elementRoot`：
`UForeverRoomFrameworkComponent`恢复回空骨架（见`ForeverRoomFrameworkComponent.md`），
不再有任何逻辑。用的换算公式和挪过来之前完全一样（Room局部矩形按`Building::LocalToWorld`
换算世界中心+building自身旋转，垂直范围是这个Room所在楼层的Z范围，三个方向各`-0.01`地图
单位，和building碰撞盒的`+0.01`配对）。

### Overlap回调只用烘焙好的字符串

和`ForeverZoneFrameworkComponent`同一套安全原则：`OnOverlapBegin/End`只使用`Init()`/
`BuildCollisionBox()`时预先烘焙好的`collisionLabel`（`FString`），`OnRoomOverlapBegin/End`
只使用`BuildRoomCollisionBoxes()`时按`room->GetAddress()`烘焙好、存进`roomBoxLabels`
（`TMap<UPrimitiveComponent*, FString>`，一栋楼有多个Room碰撞盒，不能像building自己那样
只用一个`FString`）的字符串——两组回调都绝不在回调里解引用`building`/`Room*`，关卡卸载
顺序不保证Core对象还活着，回调最坏情况只是打印一条字符串，不会有悬垂指针风险。

### 门加载优先级：从楼层构造里拆出来，单独按批次、低优先级处理

门最初是在`BuildFloorSection`里跟墙体/楼梯/地板一起同步建的，每扇门的
`UForeverDoorComponent::Init`要花4~6次`RegisterComponent()`，这部分开销被悄悄塞进了
"建一层楼"这一个LOD操作单位里，导致加门之后楼层加载明显变卡。现在拆成两步：

1. **收集**：`GatherPendingDoors()`在排队Far→Near转场的那一刻（不是等楼层建完）就把
   `building->GetDoorEntities()`和每个room的`GetDoorEntities()`全部收进`pendingDoors`
   （`TArray<Door*>`）——`Door*`（Core对象）生成时就和LOD状态无关，随时可读，不需要等
   楼层先建完才能发现有哪些门。
2. **按批构建**：`Tick()`按`FMath::DivideAndRoundUp(pendingDoors.Num(),
   framework->GetMaxDoorsPerOp())`往`doorTierOpQueue`里排入对应条数的`BuildDoorBatch`
   操作，每条执行时调用`BuildDoorBatch()`，从`pendingDoors`尾部弹出最多
   `GetMaxDoorsPerOp()`（默认4，`UForeverBuildingFrameworkComponent::maxDoorsPerOp`可
   调）个`Door*`，逐个生成`UForeverDoorComponent`（`NewObject`+
   `SetupAttachment(elementRoot)`+`RegisterComponent`+`Init(door)`+
   `AddInstanceComponent`，和`SpawnCube`/`SpawnMesh`同一套"动态创建近景组件"模式），
   `floorIndex`用`door->GetLevel() + building->GetBasementCount()`算出，push进
   `nearDoorsByFloor[floorIndex]`。

**门级操作全局低优先级**：`doorTierOpQueue`的请求要受
`UForeverBuildingFrameworkComponent::TryConsumeLodOpBudget(true)`的全局门槛——只要全
地图任意一栋building还有楼层级操作排队未执行，所有building的门级请求一律被拒绝（见
`ForeverBuildingFrameworkComponent.md`）。这样"任意建筑的楼层加载插队到任意建筑的门
加载前面"不需要building互相感知彼此，只靠这一份全局计数就能实现。

**为什么楼层/门必须分成两个独立队列，不能共用一个`TQueue`靠`Peek()`队首类型判断**：
如果只用一个混合队列，当一栋楼的门操作还没处理完、玩家又走远触发了Near→Far时，新的
`BuildFarMesh`/`DeleteAllNearMeshes`会排在剩余门操作**后面**——队首仍是门操作，会被
"全地图还有楼层级操作在等"的门槛挡住而无法出队，而挡住它的楼层级操作恰恰就是排在它
后面、永远等不到执行机会的那两个操作，变成这栋楼自己造成的死锁，且因为门槛是全局的，
会连带卡死全地图所有建筑的门加载。拆成`floorTierOpQueue`/`doorTierOpQueue`两个独立
队列后，一栋楼自己的楼层级操作永远不会被自己的门操作挡住，不会发生这个死锁。

`ClearNearSections()`对称清理门组件(`ClearDoor()`+`RemoveInstanceComponent`+
`DestroyComponent`)，并额外清空`pendingDoors`，避免这个Actor被反复Far↔Near循环使用
时，上一轮没排完的门残留污染下一轮`GatherPendingDoors`的收集结果——即便门还没建完就被
提前Near→Far打断，已经建出的门组件依然会被这里正确销毁。门洞本身不受影响——
`BuildWallsForElement`挖洞逻辑不知道、也不需要知道这个洞有没有真正的门扇，见
`Source/Core/map/door.md`/`Source/Forever/Element/DoorComponent.md`。

### 自定义模型：楼层级队列里单独一条`BuildDecorations`操作，只在近处显示

mod用`BuildingMod::AddDecoration`指定的StaticMesh(`Building::GetDecorations()`，世界摆位Core已经按
建筑朝向换算好，见`Source/Core/map/building.md`)不跟楼层/门一起建：远->近时排一条单独的
`ELodOpType::BuildDecorations`，放在所有`BuildFloorMesh`之后、`BuildElevatorCabins`之前，进
`floorTierOpQueue`——和楼层同一个优先级，高于门(`NotifyFloorTierOpEnqueued`的计数相应+1)，
没有自定义模型的建筑不排这一条。每个模型一个Static的`UStaticMeshComponent`
(`framework->ResolveMesh`全局按软路径缓存)，Z和楼层同一个地面基准`BUILDING_HEIGHT_EPSILON`，缩放
是资产原尺寸的倍数(不按单位立方体拟合，`SpawnMesh`的约定不适用)，碰撞用资产自带的。存在
`nearDecorations`里，`ClearNearSections`(近->远)一并销毁——只在近处显示。

0层建筑(`nearFloorCount==0`，比如`MoharBuilding`)：`Init`不建远处方块和进出触发盒(高度都是0)，
`BuildFarSection`也直接跳过，远处LOD什么都不显示。


- 依赖：`Source/Forever/Framework/ForeverBuildingFrameworkComponent.h`（`framework`弱引用
  回调）、`map/map.h`/`map/building.h`/`map/room.h`/`map/door.h`/`map/general_geometry.h`
  （Core侧数据）、`Source/Forever/Element/DoorComponent.h`（门）、
  `ProceduralMeshComponent`（远处LOD）、`Components/StaticMeshComponent.h`（近处每段一个
  组件）、`Components/BoxComponent.h`（碰撞盒）、`UMaterialInstanceDynamic`、
  `Kismet/GameplayStatics.h`、`Engine/Engine.h`（`GEngine->AddOnScreenDebugMessage`）、
  `Containers/Queue.h`（`TQueue`）。
- 被谁依赖：`UForeverBuildingFrameworkComponent::GenerateBuildings()`
  （`SpawnActor<ABuildingElement>()`+`Init()`）。

## 待办/后续阶段

- 单栋building内部楼层特别多/特别大时，一次近LOD转场里"这栋楼自己的组件数"仍然会从0涨到
  全部楼层的总和，耗时跟着涨（只是不再波及其它building）——如果这点残留卡顿仍然明显，可以
  考虑把挂载粒度从"每栋building一个Actor"细化成"每层楼一个Actor"，见
  `ForeverBuildingFrameworkComponent.md`"性能排查"一节末尾。
