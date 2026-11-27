# DeathWidget.h / DeathWidget.cpp

## 职责

`UDeathWidget`：玩家死亡界面(`WBP_Death`的C++基类)。由`AForeverPlayerController`常驻创建(`deathWidgetClass`，ZOrder 9，在暂停界面之下)，
流程完全由`UForeverDeathFrameworkComponent`驱动，这里只管显示和把两个按钮的点击转回去。

## 控件(WBP里按名字绑定，勾Is Variable)

| 名字 | 类型 | 必须 | 作用 |
|---|---|---|---|
| `ContinueButton` | Button | 是 | 继续游戏：跳15天后在复活点复活 |
| `ExitButton` | Button | 是 | 退出游戏：同暂停界面的退出，回主菜单 |
| `DimImage` | Image | 否 | 全屏黑色遮罩，慢镜头期间不透明度从0升到`dimOpacity`(默认0.75) |
| `TitleText` | TextBlock | 否 | 标题(比如"你死了")，跟着渐暗一起从透明渐显 |

## 关键设计

- `BeginDeath`：`HitTestInvisible`(盖在画面上不拦截鼠标)，按钮收起，进度0。`SetProgress(p)`：遮罩`RenderOpacity=p×dimOpacity`，标题`RenderOpacity=p`。
- `ShowMenu`：`Visible`、按钮出现、显示鼠标、UI Only输入并抢键盘焦点；`NativeOnKeyDown`吞掉所有按键(Esc/Tab不会漏出去开暂停)。
- `HideMenu`(点了继续)：按钮收起、交还Game Only输入，遮罩留着，随后被跳时间的黑屏盖住；黑屏盖住后`EndDeath`隐藏。
- 按钮回调经`AForeverFrameworkActor::GetDeathFramework()`调`Continue`/`ExitToMenu`，包在`CORE_GUARD`里。

## 依赖关系

- 依赖：`Framework/ForeverDeathFrameworkComponent`。
- 被谁依赖：`AForeverPlayerController`(创建/持有)、`UForeverDeathFrameworkComponent`(驱动)。
