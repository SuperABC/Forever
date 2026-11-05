# PuzzleWidget.h / PuzzleWidget.cpp

## 职责

`UPuzzleWidget`：**小游戏宿主**，创建Core `Puzzle`实例、每帧`Loop`、把画布贴到`Image`上并转发键鼠事件，结束时收尾。

## 关键设计

- 入口`StartPuzzle(puzzleId, param)`：由`UForeverStoryFrameworkComponent::ApplyStartPuzzle`(剧情`start_puzzle`)调用；`param`是脚本传的动态启动参数，在`Init`前通过`PuzzleMod::SetParam`交给小游戏，与`config.json`里`puzzle_mods`的静态args不是一回事。
- 输入模式用UIOnly：小游戏通过`NativeOnKeyDown`收键盘，UIOnly下Pawn的Enhanced Input收不到任何输入，不会和移动/视角等操作打架(同`SectionSpeakingWidget`)。
- 画布尺寸=真实视口像素(`GetViewportSize`，1:1填满)，`NativeTick`每帧比对是否resize并重建；查不到时用兜底默认值。
- `EndPuzzle(result)`(`Loop`返回非空时)：先把结果json展平进`PuzzleResultEvent`(delete后mod持有的json就没了)，再delete puzzle、隐藏自己、恢复游戏输入(`bShowMouseCursor=false`+`SetInputMode_GameOnly`+`SetFocusToGameViewport`)，与`ShowCurrentItem`队列清空分支一致。
- `puzzle`/`canvas`由本类持有。`UpdateMousePos`把鼠标位置换算成画布像素坐标。

## 依赖关系

- 依赖：Core `Puzzle`、`Dependence/player/canvas.h`、`CanvasInputTranslation`。
- 被谁依赖：`Framework/ForeverStoryFrameworkComponent`、`Framework/ForeverSaveSubsystem`、`Controller/ForeverPlayerController`。

## 待办/后续阶段

- 无。
