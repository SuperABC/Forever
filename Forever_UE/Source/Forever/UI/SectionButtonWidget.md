# SectionButtonWidget.h / SectionButtonWidget.cpp

## 职责

`USectionButtonWidget`：`SectionOptionWidget`列表里的**一行剧情分支选项**(对应老蓝图`SectionButton_C`)，点击转发给`UForeverStoryFrameworkComponent::SelectDialogOption`。

## 关键设计

- `Setup(framework, index, text)`：`index`是该行在当前分支`Section::GetOptions()`里的下标；`text`已经求值好(`Option::GetOption()`是DSL原文，求值在`ShowDialog`里、`Section`还没离开原始`ScriptContext`时完成)。
- 与`MeetButtonWidget`是两套独立类，字段/回调目标不同。
- Blueprint子类只摆命名`Button`+命名`Text`。

## 依赖关系

- 依赖：`ForeverFrameworkActor`、`ForeverStoryFrameworkComponent`。
- 被谁依赖：`SectionOptionWidget.cpp`。

## 待办/后续阶段

- 无。
