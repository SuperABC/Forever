# ForeverMenuGameMode

## 职责
主菜单/前端场景用的GameMode。

## 关键设计
- `PlayerControllerClass = AForeverMenuController`。
- `DefaultPawnClass = nullptr`:主菜单场景不需要可控制的Pawn。
- 不覆盖`PlayerStateClass`,沿用引擎默认`APlayerState`。

## 依赖关系
- 依赖`AForeverMenuController`。
- 被`Config/DefaultEngine.ini`里主菜单关卡对应的GameMode override引用(尚未配置,需要在编辑器里给菜单关卡指定这个GameMode)。

## 待办/后续阶段
- 核对`DefaultPawnClass = nullptr`是否符合实际需求(目前基于"主菜单关卡通常不需要玩家出生/行走"的推断)。
- 主菜单关卡与UI资源就位后,在编辑器里把菜单地图的GameMode Override设置为这个类。
