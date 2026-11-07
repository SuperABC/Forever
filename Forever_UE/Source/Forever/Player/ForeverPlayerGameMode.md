# ForeverPlayerGameMode

## 职责
游玩关卡用的默认GameMode。指定`AForeverPlayerController`/`AForeverPlayerState`为默认Controller/PlayerState类,`HUDClass`为`AForeverHUD`。开局`DefaultPawnClass`是引擎自带的`ADefaultPawn`,真正的玩家角色由Story系统在`game_start`时通过`ChangeControlChange`把控制权切换到一个随机市民(`ACitizenElement`,见`ForeverStoryFrameworkComponent.cpp`)。没有`APlayerStart`时提供出生点兜底。

## 关键设计
- `DefaultPawnClass = ADefaultPawn::StaticClass()`:不在开局生成一个独立于Populace之外的"玩家角色"——"玩家是谁"完全交给Story系统决定,`ADefaultPawn`只是过渡用的默认键鼠飞相机(legacy input,见`Config/DefaultInput.ini`)。
- `FindPlayerStart_Implementation`:先用`TActorIterator<APlayerStart>`检查关卡里是否存在真正的`APlayerStart`,有才交给`Super::FindPlayerStart_Implementation`;没有才动态`SpawnActor<APlayerStart>`。**不能只看`Super::FindPlayerStart_Implementation`的返回值是否为空来判断"有没有真正的出生点"**——`PlayerStarts`数组为空时引擎会退化返回`AWorldSettings`(永远存在、永远在原点)兜底,不会返回`nullptr`。
- 找不到`PlayerStart`时的兜底位置是`AForeverFrameworkActor::GetDefaultSpawnLocation()`(地图中心的常量位置):此时世界生成还没跑完(分阶段生成,见`Framework/ForeverFrameworkActor.md`),地形不可用。玩家真正的位置由`game_start`的`change_control`/读档的`RestoreControlledPawn`决定。
- `EnsureFrameworkActorExists()`:用`TActorIterator<AForeverFrameworkActor>`检测场景中是否已有实例,没有就在原点动态`SpawnActor`一个;`BeginPlay`调用它。不再调用`EnsureMapGenerated()`,地图生成由该Actor自己分阶段推进。

## 依赖关系
- 依赖`AForeverPlayerController`/`AForeverPlayerState`/`AForeverHUD`/`AForeverFrameworkActor`、引擎`ADefaultPawn`。
- 是`Content/Blueprint/Player/BP_ForeverPlayerGameMode`的父类,`World.umap`的World Settings用这个蓝图作GameMode(菜单关卡用`BP_ForeverMenuGameMode`,即`DefaultEngine.ini`的`GlobalDefaultGameMode`)。

## 待办/后续阶段
- 一旦`World.umap`里手动放置了真实的`AForeverFrameworkActor`,删除`EnsureFrameworkActorExists`的动态兜底生成逻辑。
