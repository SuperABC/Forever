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

具体的墙体开洞分段算法/坐标转换/材质网格解析/电梯轿厢动画公式，和这个类从
`UForeverBuildingFrameworkComponent`搬过来之前完全一样，详细说明见
`ForeverBuildingFrameworkComponent.md`（那份文档描述的是算法本身，这次会话没有改动算法，
只是把承载这些算法的代码从框架组件搬到了这里，函数签名从"传入`Building*`/
`FBuildingRenderState&`两个参数"简化成"直接用Element自己的成员"）。这份文档只记录
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

### 全局LOD操作预算：`TryConsumeLodOpBudget()`

每个Element自己维护一个`TQueue<FLodOp>`（这栋楼自己排队要执行的LOD操作），但**执行速度**
受框架组件`TryConsumeLodOpBudget()`节流——框架组件的`TickComponent`每帧把一个共享计数器
`frameOpBudgetRemaining`重置成`maxLodOpsPerTick`，所有Element的`Tick`在真正执行一条操作
之前都要先来申请一份，申请失败就等下一帧再试。这是"状态机下放到每个Element"和"全局节流
不能丢"两个要求的折中：如果完全不设共享预算，大量building同时穿越距离阈值时（比如玩家
瞬移，或者沿着阈值边界走）依然会在同一帧集中爆发，重新变成"每次LOD切换都巨卡"。

`Tick`里申请预算的循环**必须先判断自己的队列是否为空，再申请预算**：
```cpp
while (!lodOpQueue.IsEmpty() && framework->TryConsumeLodOpBudget()) { ... }
```
不能反过来写成`framework->TryConsumeLodOpBudget() && lodOpQueue.Dequeue(op)`——那样即使
这一帧这栋building根本没有排队的操作，也会先申请（并消耗）一份全局预算，只是申请到了却没
东西可执行，白白挤占了其它真正需要建楼层的building的份额。

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

## 依赖关系

- 依赖：`Source/Forever/Framework/ForeverBuildingFrameworkComponent.h`（`framework`弱引用
  回调）、`map/map.h`/`map/building.h`/`map/room.h`/`map/geometry.h`（Core侧数据）、
  `ProceduralMeshComponent`（远处LOD）、`Components/StaticMeshComponent.h`（近处每段一个
  组件）、`Components/BoxComponent.h`（碰撞盒）、`UMaterialInstanceDynamic`、
  `Kismet/GameplayStatics.h`、`Engine/Engine.h`（`GEngine->AddOnScreenDebugMessage`）、
  `Containers/Queue.h`（`TQueue`）。
- 被谁依赖：`UForeverBuildingFrameworkComponent::GenerateBuildings()`
  （`SpawnActor<ABuildingElement>()`+`Init()`）。

## 待办/后续阶段

- `[BuildFloorSection耗时排查]`诊断日志（打印`elapsed`/`components`/`elementComponents`）
  和框架组件`TickComponent`里的`[强制GC排查]`诊断代码，在排查结论定型、每栋building独立
  Actor这个架构跑稳之后已经整段删除。
- 单栋building内部楼层特别多/特别大时，一次近LOD转场里"这栋楼自己的组件数"仍然会从0涨到
  全部楼层的总和，耗时跟着涨（只是不再波及其它building）——如果这点残留卡顿仍然明显，可以
  考虑把挂载粒度从"每栋building一个Actor"细化成"每层楼一个Actor"，见
  `ForeverBuildingFrameworkComponent.md`"性能第2轮"一节末尾。
