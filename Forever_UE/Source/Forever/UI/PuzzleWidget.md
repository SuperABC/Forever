# PuzzleWidget.h / PuzzleWidget.cpp

## 职责

`UPuzzleWidget`：**小游戏宿主**，创建Core `Puzzle`实例、每帧`Loop`、把画布贴到`Image`上并转发键鼠事件，结束时收尾。

## 关键设计

- 入口`StartPuzzle(puzzleId, param)`：由`UForeverStoryFrameworkComponent::ApplyStartPuzzle`(剧情`start_puzzle`)调用；`param`是脚本传的动态启动参数，在`Init`前通过`PuzzleMod::SetParam`交给小游戏，与`config.json`里`puzzle_mods`的静态args不是一回事。
- **puzzle id 没注册/没在 config `puzzle_mods` 里启用时**：`Puzzle`构造抛`DataException`，`StartPuzzle`在动任何状态之前先创建新的这局并用`GuardData`接住(只接数据错误)，失败只打Warning日志并直接返回(不暂停世界、不显示界面，正在进行的上一局原样保留)，不会崩游戏。
- 输入模式用UIOnly：小游戏通过`NativeOnKeyDown`收键盘，UIOnly下Pawn的Enhanced Input收不到任何输入，不会和移动/视角等操作打架(同`SectionSpeakingWidget`)。
- 画布尺寸=真实视口像素(`GetViewportSize`，1:1填满)，`NativeTick`每帧比对是否resize并重建；查不到时用兜底默认值。
- `EndPuzzle(result)`(`Loop`返回非空时)：先把结果json展平进`PuzzleResultEvent`(delete后mod持有的json就没了)，再delete puzzle、隐藏自己、恢复游戏输入(`bShowMouseCursor=false`+`SetInputMode_GameOnly`+`SetFocusToGameViewport`)，与`ShowCurrentItem`队列清空分支一致。
- `puzzle`/`canvas`由本类持有。`UpdateMousePos`把鼠标位置换算成画布像素坐标。

- **不能让`Puzzle`活过一局**(2026-11-25)：`puzzle`包着mod dll里创建的实例，mod dll在一局结束时卸载。
  `AbortPuzzle()`丢弃正在进行的小游戏(不发结果事件)，`AForeverFrameworkActor::EndPlay`在卸载前调它，`NativeDestruct`里再兜底一次。
  规则见`CONVENTIONS.md`第13节。

## 依赖关系

- 依赖：Core `Puzzle`、`Dependence/player/canvas.h`、`CanvasInputTranslation`。
- 被谁依赖：`Framework/ForeverStoryFrameworkComponent`、`Mod/ForeverSaveSubsystem`、`Player/ForeverPlayerController`。

## 待办/后续阶段

- 无。

## 时间
开始谜题时记下当前全局时间缩放并设0，结束时还原成之前的值；重复启动(上一局还没结束)不会覆盖记下的值。
