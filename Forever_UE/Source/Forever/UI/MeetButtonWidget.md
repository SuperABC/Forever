# MeetButtonWidget.h / MeetButtonWidget.cpp

## 职责

`UMeetButtonWidget`：`MeetOptionWidget`滚动列表里的**一行交互选项**(对应老蓝图`MeetButton_C`)，点击转发给`UForeverStoryFrameworkComponent::OptionDialog`。

## 关键设计

- 持有三个字段：对话目标(市民/车辆/门的交互名)`targetName`、选项文本`option`、框架`framework`(弱引用)。
- `ClickButton()`给`MeetOptionWidget::ClickFocus`(键盘/手柄确认路径，不经真实鼠标点击)用，等价于`HandleClicked`，单独公开以和"Button.OnClicked回调"的身份分开。
- `SetRowHighlighted`切换高亮底色(当前Focus行黄色，其余白色)，把"改颜色"封装在本类，`MeetOptionWidget`不需要知道Button子控件。
- 与`SectionButtonWidget`是两套独立类(字段/回调目标不同)，不复用。
- Blueprint子类只摆命名`Button`+命名`Text`。

## 依赖关系

- 依赖：`ForeverFrameworkActor`、`ForeverStoryFrameworkComponent`。
- 被谁依赖：`MeetOptionWidget.cpp`。

## 待办/后续阶段

- 无。
