# ForeverGameMode

## 职责
默认GameMode,指定`AForeverCharacter`/`AForeverPlayerController`/`AForeverPlayerState`为默认Pawn/Controller/PlayerState类。同时兜底解决"空场景无法测试"的问题:没有出生点。

对应旧蓝图`Blueprint/Player/MainGameMode`——用户确认该蓝图是纯数据(只设置Class Defaults,没有图表逻辑,因此没有dump文件)。这三个类默认值本身就是这份"纯数据"的等价物,阶段1核对blueprint内容时未发现需要补充的地方。

## 关键设计
- `FindPlayerStart_Implementation`:**先自己检查关卡里是不是真的有`APlayerStart`**（`TActorIterator<APlayerStart>`），有才交给引擎默认的`Super::FindPlayerStart_Implementation`处理；找不到才动态`SpawnActor<APlayerStart>`。**不能直接用`Super::FindPlayerStart_Implementation`的返回值是否为空来判断"有没有真正的出生点"**——这是阶段4-1实现时踩过的一个坑：引擎自己的`ChoosePlayerStart_Implementation`在`PlayerStarts`数组为空时会退化返回`AWorldSettings`（每个关卡都有、永远在原点）兜底，不会返回`nullptr`；一开始按"`if (Super::FindPlayerStart_Implementation(...))`为真就直接用"来写，结果哪怕关卡里一个`PlayerStart`都没有，这个分支也一直命中，地图正中心的计算代码从来没机会跑到，PIE验证时才发现出生点还在(0,0)。阶段4-1(Terrain落地,要求#5)之前这个兜底位置写死在原点上方(0,0,100),现在改成**地图正中心**:调用`EnsureFrameworkActorExists()`(幂等)确保地形已经生成,再从返回的`AForeverFrameworkActor::GetTerrainFramework()->GetMapCenterWorldLocation()`拿到含地形高度采样的世界坐标。如果因为某种原因拿不到`AForeverFrameworkActor`或地形还没生成,退回原来的`(0,0,100)`兜底(理论上不会走到,`EnsureFrameworkActorExists`保证地形已生成)。这样阶段0不需要打开编辑器往`World.umap`里放出生点,后续美术/关卡搭建时手动放置的`APlayerStart`会自动优先生效。
- **占位地板已经删除(阶段4-1,要求#5)**:阶段0在`BeginPlay`里检测场景中是否已有`AStaticMeshActor`、没有就用`/Engine/BasicShapes/Plane`铺一块占位地板的逻辑,连同`placeholderFloorMesh`属性,已经随Terrain落地整个移除——现在有真正的地形网格(`UForeverTerrainFrameworkComponent`),不再需要这个占位。
- `EnsureFrameworkActorExists()`(阶段2新增,阶段4-1改为返回`AForeverFrameworkActor*`而不是`void`):用`TActorIterator<AForeverFrameworkActor>`检测场景里是否已有实例,没有就在原点动态`SpawnActor`一个;找到/生成后都会调用该Actor的`EnsureTerrainGenerated()`(幂等)。`BeginPlay`和`FindPlayerStart_Implementation`都会调用这个函数——因为UE的玩家生成时机（`FindPlayerStart_Implementation`）相对`GameMode::BeginPlay`的实际先后顺序不是这份文档能确定死的事情，两处都调用、靠`EnsureFrameworkActorExists`/`EnsureTerrainGenerated`双重幂等，保证不管谁先跑，出生点计算时地形一定已经就绪。详见`Framework/ForeverFrameworkActor.md`。

## 依赖关系
- 依赖`AForeverCharacter`/`AForeverPlayerController`/`AForeverPlayerState`/`AForeverFrameworkActor`/`UForeverTerrainFrameworkComponent`。
- 被`Config/DefaultEngine.ini`的`GlobalDefaultGameMode=/Script/Forever.ForeverGameMode`引用。

## 待办/后续阶段
- 阶段4/关卡搭建阶段:一旦`World.umap`里手动放置了真实的`AForeverFrameworkActor`,删除`EnsureFrameworkActorExists`的动态兜底生成逻辑。
