# ForeverMenuGameMode

## 职责
主菜单/前端场景用的GameMode。

## 关键设计
- `PlayerControllerClass = AForeverMenuController`。
- `DefaultPawnClass = nullptr`:主菜单场景不需要可控制的Pawn。
- 不覆盖`PlayerStateClass`,沿用引擎默认`APlayerState`。

## 依赖关系
- 依赖`AForeverMenuController`。
- 其蓝图子类`BP_ForeverMenuGameMode`是`DefaultEngine.ini`的`GlobalDefaultGameMode`,菜单关卡(`Menu.umap`)沿用它。

## 待办/后续阶段
- 无。
