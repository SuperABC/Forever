# ForeverPausePanelWidget

## 职责
游戏内暂停界面：`Tab`(按键绑定`TogglePause`，可在`KeyBindings.ini`里改)打开/关闭，打开时把全局时间缩放设成0暂停世界，提供"保存""加载"两个按钮，各自弹出文件对话框，实际读写交给`UForeverSaveSubsystem`。存档系统总说明见`Source/Core/common/save.md`。

## 关键设计
- 由`AForeverPlayerController::BeginPlay`常驻创建(`pauseWidgetClass`，`AddToViewport(10)`盖在其它常驻Widget上面)，`NativeConstruct`里默认`Collapsed`。
- 暂停用全局时间缩放(打开设0，关闭设1)。建筑LOD冻结(`ForeverBuildingFrameworkComponent`)和谜题(`PuzzleWidget`)也用时间缩放=0，并且结束时会改回1，所以**时间缩放已经是0时不允许打开暂停界面**——否则它们结束时会意外解除暂停，关暂停界面时也会解除它们的冻结。UE会把时间缩放夹到WorldSettings的最小值(默认0.0001)，所以按`< 0.01`判断。
- 输入模式切换照抄`UMapWidget`：打开时UI Only+显示鼠标+`SetKeyboardFocus`，关闭时Game Only+`SetFocusToGameViewport`。UI Only下Enhanced Input收不到按键，关闭键要在`NativeOnKeyDown`里自己拦：比对`UForeverKeyBindingSubsystem::GetKey("TogglePause")`(按配置，不写死Tab)，`Esc`固定也能关。
- 文件对话框关掉后键盘焦点不一定回到界面上，`RefocusSelf`重新抢一次，Tab/Esc才能继续关。
- 加载成功时`RequestLoad`已经发起`OpenLevel`，关卡连同暂停状态和这个Widget一起销毁，新关卡的`AForeverPlayerController::BeginPlay`重置输入模式，这里不收尾。
- 打开前由Controller检查地图/手机/背包/谜题/剧情对话都没开着(见`AForeverPlayerController::TogglePause`)，避免两个UI Only界面抢焦点。

## 蓝图约定
- `WBP_PausePanel`，父类`UForeverPausePanelWidget`。
- 必须有：`SaveButton`、`LoadButton`、`CloseButton`(`UButton`)。继续游戏有三种方式：`Tab`、`Esc`、点`CloseButton`。
- 可选：`MessageText`(`UTextBlock`，显示保存成功/失败原因；没有时结果打在屏幕左上角的调试信息里)。
- 在`BP_ForeverPlayerController`的Class Defaults里把`pauseWidgetClass`设成它。

## 依赖关系
- 依赖：`Mod/ForeverSaveSubsystem`(`SaveWithDialog`/`LoadWithDialog`)、`Input/ForeverKeyBindingSubsystem`(`GetKey`)。
- 被谁依赖：`Player/ForeverPlayerController`。

## 待办/后续阶段
- 无。保存前的UE侧状态同步、对话/谜题进行中拒绝保存已经在`UForeverSaveSubsystem::SaveToFile`里做了，见`ForeverSaveSubsystem.md`。
