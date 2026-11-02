# PhoneWidget.h / PhoneWidget.cpp

## 职责

`UPhoneWidget`：**手机宿主**，把Core `Phone`跑起来、把它的画布贴到一张`Image`上，并转发UE键鼠事件。

## 关键设计

- 由`AForeverPlayerController`常驻持有一份实例，P键(`TogglePhone`)控制开关。
- 与`PuzzleWidget`的两点差异：
  1. 生命周期——Puzzle每次启动new/结束delete；Phone要跨"开->关->再开"保持多任务状态(桌面/运行中App/最近任务)，所以`phone`和`canvas`只在**第一次打开**时创建，`TogglePhone`之后只切`Visibility`+输入模式(`OpenPhone`=Visible+UIOnly，`ClosePhone`=Collapsed+GameOnly)，不delete/重建，只在`NativeDestruct`里释放。
  2. 画布分辨率——取`DisplayImage`子控件自己的屏幕像素尺寸(`GetAbsoluteSize`已含DPI缩放，1:1)，而不是整个视口；查不到(几何还没算出来)退回`kFallbackPhoneWidth/Height`(480x640)。
- `BlitCanvasToImage`照抄`PuzzleWidget`：`CreateTransient`+Lock/Memcpy/Unlock+`UpdateResource`。
- 键鼠翻译用`CanvasInputTranslation`；`DisplayImage`命名避开Core层`Canvas`同名。

## 依赖关系

- 依赖：Core `Phone`、`Dependence/player/canvas.h`、`CanvasInputTranslation`。
- 被谁依赖：`Controller/ForeverPlayerController`。

## 待办/后续阶段

- 无。
