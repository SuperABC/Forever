# SectionOptionWidget.h / SectionOptionWidget.cpp

## 职责

`USectionOptionWidget`：剧情对话走到**分支选项段**(`Section::IsBranch()`)时弹出的选项列表。

## 关键设计

- `ShowOptions(framework, optionTexts)`：为每条已求值的文本创建一个`USectionButtonWidget`(类由`SectionButtonClass`指定)放进`OptionContainer`(ScrollBox)并设为可见，下标就是`SelectDialogOption`的index；`HideOptions`在轮到下一项时隐藏。
- 一次性弹出、选完即消失；与`MeetOptionWidget`(靠近交互对象的常驻选项列表)是两套UI。

## 依赖关系

- 依赖：`SectionButtonWidget`、`ForeverFrameworkActor`。
- 被谁依赖：`Framework/ForeverStoryFrameworkComponent`(`ShowCurrentItem`)、`Player/ForeverPlayerController`。

## 待办/后续阶段

- 无。
