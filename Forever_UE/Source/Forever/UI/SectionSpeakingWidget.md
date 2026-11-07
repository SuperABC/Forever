# SectionSpeakingWidget.h / SectionSpeakingWidget.cpp

## 职责

`USectionSpeakingWidget`：剧情对话里**一条普通台词**(发言者+内容)的展示框，点击或按空格推进到下一条。

## 关键设计

- `ShowLine(framework, speaker, content)`/`HideLine()`；`speaker`为空时隐藏发言者文本行(只显示content)。点击铺满整个控件的`ClickCatcher`按钮，或按空格，都转发给`UForeverStoryFrameworkComponent::AdvanceDialog`。
- **空格键走`NativeOnKeyDown`而不是Enhanced Input**：对话展示期间`ShowCurrentItem`把输入模式切成UIOnly(WidgetToFocus指定为本Widget)，UIOnly下Pawn的Enhanced Input完全收不到输入——这样从根上不会把按键路由给Pawn，不需要逐个判断"对话是否在展示"；所以空格只能在拿到键盘焦点的本Widget上接。只处理SpaceBar，其他键交给基类。
- 点击没有"最短展示时长"防误触（未实现，见待办）。

## 依赖关系

- 依赖：`ForeverFrameworkActor`、`ForeverStoryFrameworkComponent`。
- 被谁依赖：`Framework/ForeverStoryFrameworkComponent`、`Input/ForeverKeyBindingSubsystem`、`Player/ForeverPlayerController`、`PuzzleWidget.h`。

## 待办/后续阶段

- 补上点击防误触的最短展示时长(需要时再加)。
