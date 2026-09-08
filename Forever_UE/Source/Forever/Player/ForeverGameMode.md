# ForeverGameMode

## 职责
默认GameMode,指定`AForeverCharacter`/`AForeverPlayerController`/`AForeverPlayerState`为默认Pawn/Controller/PlayerState类。同时兜底解决"空场景无法测试"的两个问题:没有出生点、没有地面。

对应旧蓝图`Blueprint/Player/MainGameMode`——用户确认该蓝图是纯数据(只设置Class Defaults,没有图表逻辑,因此没有dump文件)。这三个类默认值本身就是这份"纯数据"的等价物,阶段1核对blueprint内容时未发现需要补充的地方。

## 关键设计
- `FindPlayerStart_Implementation`:先走引擎默认逻辑找场景里手放的`APlayerStart`;找不到才动态`SpawnActor<APlayerStart>`在原点上方(0,0,100)。这样阶段0不需要打开编辑器往`World.umap`里放出生点,后续美术/关卡搭建时手动放置的`APlayerStart`会自动优先生效。
- `BeginPlay`里检测场景中是否已有`AStaticMeshActor`,一个都没有的话才用`/Engine/BasicShapes/Plane`铺一块100x100倍缩放的占位地板(带碰撞)。这是阶段0专属的临时占位,**阶段4引入Map/Terrain系统后应移除这段逻辑**,不要误以为是正式地形方案。

## 依赖关系
- 依赖`AForeverCharacter`/`AForeverPlayerController`/`AForeverPlayerState`。
- 依赖引擎自带资产`/Engine/BasicShapes/Plane`(引擎内容,无需拷贝)。
- 被`Config/DefaultEngine.ini`的`GlobalDefaultGameMode=/Script/Forever.ForeverGameMode`引用。

## 待办/后续阶段
- 阶段4(Map/Terrain系统落地后):删除`BeginPlay`里的占位地板生成逻辑,以及`placeholderFloorMesh`属性。
