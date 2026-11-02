# MeetOptionWidget.h / MeetOptionWidget.cpp

## 职责

`UMeetOptionWidget`：靠近市民/载具/门等可交互对象时弹出的**交互选项列表**(对应老蓝图`MeetOption_C`)，支持键盘/滚轮移动高亮、确认选中。

## 关键设计

- 列表本体是`OptionContainer`(ScrollBox)，每个选项是一个`UMeetButtonWidget`(类由`MeetButtonClass`指定)；`Focus`记录当前高亮行下标；`OptionMap`按"目标名+下标"复合key(`MakeKey`)记录每行控件，供按目标名批量移除。
- 接口：`AddOption`(玩家靠近时每项调一次)、`RemoveName`(离开时移除该目标的全部选项)、`HasName`(玩家是否正站在它旁边)、`ReplaceOptions`(运行中`add_option/remove_option`改了选项：名字在列表里则整组替换；不在则什么都不做，下次靠近时Actor用自己刷新过的快照)、`FocusUp/FocusDown/ClickFocus`。
- `NativeTick`只在Focus实际变化时才重刷颜色，并把高亮行滚入可视区域。
- 与`SectionOptionWidget`是两套UI：这里管"靠近能问什么"(随时多个，滚轮/F操作)，那个管"剧情对话走到分支时怎么选"(一次性弹出)。

## 依赖关系

- 依赖：`MeetButtonWidget`、`ForeverFrameworkActor`。
- 被谁依赖：`Element/AssetElement`、`BuildingElement`、`CitizenElement`、`DoorComponent`、`TransitVehicleElement`、`VehicleElement`，`Framework/ForeverStoryFrameworkComponent`，`Player/ForeverCharacter`、`Controller/ForeverPlayerController`。

## 待办/后续阶段

- 无。
