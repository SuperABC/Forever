# ModCheckRowWidget.h / ModCheckRowWidget.cpp

## 职责

`UModCheckRowWidget`：Mod screen右列的**一行=一个mod id的勾选框**。

## 关键设计

- 状态：被依赖(`bLocked`)且已勾选 -> 勾选框禁用不能取消、名字半透明(整行保持可用，参数控件仍可编辑)；被依赖但还没勾选 -> 文字标红、勾选框保持未勾选且可点。标红后靠首次`SetRowData`记下的`defaultLabelColor`还原。
- 悬停提示：被依赖时，鼠标悬停该行(含勾选框)用提示框(`BuildDependentsToolTip`，深色背景+放大字号)列出依赖它的mod；没被依赖则不显示。
- 切换时调`ForeverConfigBridgeSubsystem::SetModEnabled`，再回调`onChanged`(即`ModPanelWidget::RefreshModList`)：启用/禁用任一id都可能改变别的id的锁定状态，必须整列刷新而不是只刷自己。

## 依赖关系

- 依赖：`ForeverConfigBridgeSubsystem`。
- 被谁依赖：`ModPanelWidget.cpp`。

## 单选行模式
`SetSelectRowData(modId, bSelected, onSelect)`：主线剧情列表用。选中行勾选框禁用，勾上别的行调`onSelect(modId)`，不走`Bridge::SetModEnabled`。

## 参数编辑
可选控件(`BindWidgetOptional`，WBP里按名字摆，没有就不生效)：`ArgsBox`(`UEditableTextBox`，整串参数，回车/失焦提交到`Bridge::SetModArgs`)、`ArgsDetailButton`(`UButton`，点击弹出`UModArgsDialogWidget`，确定后回写`ArgsBox`并提交)。`SetRowData`从`Bridge::GetModArgs`读入当前参数；未勾选的mod也可编辑(暂存内存)。单选行(`SetSelectRowData`)会把这两个控件折叠。被依赖且已勾选的行只锁勾选框(名字半透明)，参数输入框和详情按钮仍可用。
