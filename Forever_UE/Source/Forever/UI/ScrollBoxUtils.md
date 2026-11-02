# ScrollBoxUtils.h

## 职责

头文件内联的`namespace ScrollBoxUtils`：配置界面几个`ScrollBox`的**统一item间距**。没有`.cpp`。

## 关键设计

- `AddChildWithGap(list, child)`添加子项并给其`ScrollBoxSlot`设底部Padding=`kItemGap`(4)。留白集中在一个常量，而不是让每个Row蓝图(`WBP_PathRow`/`WBP_ConceptButton`/`WBP_ModCheckRow`)自己在根节点加Margin，调整间距只改这一处。

## 依赖关系

- 依赖：UMG `UScrollBox`。
- 被谁依赖：`ModPanelWidget.cpp`、`ResourcePanelWidget.cpp`、`StoryPanelWidget.cpp`。

## 待办/后续阶段

- 无。
