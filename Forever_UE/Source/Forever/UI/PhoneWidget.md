# PhoneWidget.h / PhoneWidget.cpp

## 职责

`UPhoneWidget`：**手机宿主**，把Core `Phone`跑起来、把它的画布贴到一张`Image`上，并转发UE键鼠事件。

## 关键设计

- 由`AForeverPlayerController`常驻持有一份实例，P键(`TogglePhone`)控制开关。
- 与`PuzzleWidget`的两点差异：
  1. 生命周期——Puzzle每次启动new/结束delete；Phone要跨"开->关->再开"保持多任务状态(桌面/运行中App/最近任务)，所以`phone`和`canvas`只在**第一次打开**时创建，`TogglePhone`之后只切`Visibility`+输入模式(`OpenPhone`=Visible+UIOnly，`ClosePhone`=Collapsed+GameOnly)，不delete/重建，只在`NativeDestruct`里释放。
  2. 画布分辨率——取`DisplayImage`子控件自己的屏幕像素尺寸(`GetAbsoluteSize`已含DPI缩放，1:1)，而不是整个视口；查不到(几何还没算出来)退回`kFallbackPhoneWidth/Height`(480x640)。
- `BlitCanvasToImage`照抄`PuzzleWidget`：`CreateTransient`+Lock/Memcpy/Unlock+`UpdateResource`。
- **手机关着时**(`Collapsed`的控件不Tick)：`AForeverPlayerController::Tick`每帧调`TickHidden(FApp::GetDeltaTime())`，手机建过且不可见时调`Phone::LoopHidden`，打开过的App在后台照样跑逻辑(不画)，见`Core/player/phone.md`。
- `NativeOnMouseWheel`把滚轮(向上为正)转给`Canvas::PushMouseWheel`，App的列表可以滚。
- **画布外的鼠标不发给手机**：位置按`DisplayImage`自己的几何换算(`UpdateMousePos`，按键/滚轮事件用自带的位置)，落在画布外的按下和滚轮直接丢掉。
  以前会带着画布外的坐标发进去：`Phone`把y超出画布的点当成底部导航栏(点手机下方=返回/桌面/任务)，App把它当成边上控件的点击。
  画布内按下、拖到画布外松开的抬起照发(`pressedInside`)，否则App里一直当按着；鼠标位置在画布外也照常更新，拖动能跟着走。关手机时清掉按下记录。
- 键鼠翻译用`CanvasInputTranslation`；`DisplayImage`命名避开Core层`Canvas`同名。

## 依赖关系

- 依赖：Core `Phone`、`Dependence/player/canvas.h`、`CanvasInputTranslation`。
- 被谁依赖：`Player/ForeverPlayerController`。
