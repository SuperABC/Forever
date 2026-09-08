# ForeverMenuGameMode

## 职责
主菜单/前端场景用的GameMode。对应旧蓝图`Blueprint/Player/MenuGameMode`——用户确认该蓝图是纯数据(只设置Class Defaults,没有图表逻辑),因此没有dump文件,直接按数据语义重建。

## 关键设计
- `PlayerControllerClass = AForeverMenuController`。
- `DefaultPawnClass = nullptr`:主菜单场景不需要可控制的Pawn,这是基于"纯数据蓝图"场景的合理推断(前端菜单地图通常不需要玩家出生/行走),**未经用户逐项核对旧蓝图Class Defaults面板确认**,如果实际旧蓝图里`DefaultPawnClass`另有所指,需要回来改这里。
- 不覆盖`PlayerStateClass`,沿用引擎默认`APlayerState`——旧蓝图既然没有图表逻辑,也没有迹象表明需要自定义PlayerState。

## 依赖关系
- 依赖`AForeverMenuController`。
- 被`Config/DefaultEngine.ini`里主菜单关卡对应的GameMode override引用(尚未配置,需要在编辑器里给菜单关卡指定这个GameMode)。

## 待办/后续阶段
- 阶段5:主菜单关卡本身、`StartMenu`等UI资源就位后,需要在编辑器里把菜单地图的GameMode Override设置为这个类,并核对上面"关键设计"里标注的推断是否与旧蓝图Class Defaults一致。
