# ForeverThrowListWidget

## 职责
长按数字键3弹出的投掷物选择列表：每组可投掷的物品一个按钮(名称+数量)，点击按钮激活该投掷物并关闭列表；松开3键时列表还在(没点任何按钮)就只关闭，什么都不发生。打开期间世界慢放到0.1倍。逻辑全在C++，外观由Blueprint子类设计(和项目里其它控件同一模式：C++基类 + `BindWidget` + 手工WBP)。

## Blueprint 需要的控件
**`WBP_ThrowList`**(父类 `UForeverThrowListWidget`)：
- `ListContainer`(`ScrollBox`，必须，名字一字不差)：放按钮行，行多了自己滚动。
- `EmptyText`(`TextBlock`，可选)：没有投掷物时显示，比如"没有投掷物"；不摆则列表为空时什么都不显示。
- 类默认值里把 `buttonClass` 设成 `WBP_ThrowListButton`。
- 按钮行之间的间距由C++设(每行上下各 `Row Vertical Padding`，默认6)，在类默认值里调，不需要在蓝图里给行加边距。
- 其余装饰(底板、标题、`ScrollBox` 的最大高度/位置)随便设计。

**`WBP_ThrowListButton`**(父类 `UForeverThrowListButtonWidget`)：
- `Button`(`Button`，必须)、`Label`(`TextBlock`，必须)：文字由C++设成 `资产id  x数量`。
- 当前激活项的按钮底色由C++设置，在类默认值里调 `Active Color` / `Inactive Color` 即可，不需要写蓝图逻辑。

**`BP_ForeverPlayerController`**：类默认值里把 `throwListWidgetClass` 设成 `WBP_ThrowList`。没设会在日志里警告，长按3不会弹出列表。

## 关键设计
- **慢放用全局时间膨胀**：`Open()`存下当前`GetGlobalTimeDilation`，设为0.1；`Close()`还原。因为暂停面板/谜题/建筑LOD冻结也用时间膨胀：(1) 打开前膨胀`<0.01`(世界被别人冻结)就不打开；(2) 关闭时只在当前值仍是0.1时才还原，中途被别的系统改走(比如谜题冻结了)就不动；(3) `TogglePause`在列表打开时拒绝。世界慢放时游戏时钟流速也跟着变成0.1倍。
- **输入**：沿用其它面板的惯例，打开时`SetInputMode_UIOnlyEx`+显示光标+`SetKeyboardFocus`，所以视角和移动自然被挡住。UI Only下Enhanced Input收不到按键，松开3键要控件自己在`NativeOnKeyUp`里拦(按`UForeverKeyBindingSubsystem::GetKey("ThrowItem")`比较)，`Close()`幂等。
- **点击按钮后输入模式留在UI Only，直到3键真正松开**：点击时3通常还按着，此时切回游戏会清空按键状态，系统的按键重复随后让Pawn收到假的"按下"，又触发长按、再弹出列表。所以`Select`只隐藏列表(透明度0+`HitTestInvisible`，不能设Collapsed——Collapsed的控件拿不到键盘焦点)、还原时间膨胀并激活投掷物，输入仍留在UI Only，等`NativeOnKeyUp`收到3键松开才`Close()`切回游戏；这期间按键重复都被控件吃掉。代价是点击后到松开3之前不能移动/转视角。`NativeTick`里有10秒保险，一直收不到松开就强行切回，免得卡死。`IsOpen()`在这两个阶段都为true。
- **切输入模式会清空按键状态**：Pawn会收到假的"松开"，键还按着时系统的按键重复又会让它收到假的"按下"。所以`AForeverCharacter::ThrowKeyReleased`和`ThrowKeyPressed`在`IsOpen()`时都忽略，否则会反复触发长按、列表闪烁。不用"关闭后一段时间内忽略按下"的时间窗口：会让松开3后紧接着再按3按不出来。
- 行的点击回调是C++原生委托(`FOnThrowListButtonSelected`，带type)，列表控件在创建每一行时绑定到`Select(type)`；不需要给蓝图暴露。
- 名称先显示资产id(`asset_grenade  x10`)，以后加displayName时和HUD左上角一起换。

## 依赖关系
- 依赖：`UForeverThrowableComponent`(`ActivateByType`，列表数据`FThrowListItem`)、`UForeverKeyBindingSubsystem`、`UForeverThrowListButtonWidget`。
- 被谁依赖：`AForeverPlayerController`(创建并常驻，ZOrder 5)、`AForeverCharacter`(长按时`Open`、松开/换人时`Close`)。

## 待办/后续
- 键盘/手柄选择(目前只能鼠标点击)。
- 中文显示名。
