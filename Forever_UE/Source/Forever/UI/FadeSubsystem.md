# FadeSubsystem.h / FadeSubsystem.cpp

## 职责

`UFadeSubsystem`(`UGameInstanceSubsystem`)：**开局黑屏的持有者**，负责Menu->World切换期间一直盖着黑屏，World就绪后由`UFadeWidget`自己渐隐。

## 关键设计

- 黑屏必须在"按下开始/加载"的**那一帧**就出现，而`OpenLevel(World)`要同步加载很久、期间不渲染。所以控件由GameInstance持有(不属于任何关卡)，直接挂在`GameViewportClient`上，不随关卡销毁；`OpenWorldLevel`先变黑、等几帧让黑屏真正画出来，再`OpenLevel`。
- `WBP_Fade`的类由Menu/World两个Controller在`BeginPlay`里`RegisterWidgetClass`登记(类在各自BP里指定)；没登记过则`ShowBlack`打警告什么也不做。
- `EnsureBlack`：World关卡进场时调用，黑屏还没显示(比如编辑器里直接在World.umap上PIE)就补一个。
- 开始游戏和读档共用`OpenWorldLevel`。

## 依赖关系

- 依赖：`FadeWidget`。
- 被谁依赖：`ForeverConfigBridgeSubsystem`、`ForeverSaveSubsystem`、`Controller/ForeverMenuController`、`Controller/ForeverPlayerController`、`FadeWidget`。

## 待办/后续阶段

- 无。
