# BackgroundSpeakingWidget

## 职责
后台对话的展示框，显示在屏幕上方：一条台词的发言者+内容。和`SectionSpeakingWidget`(前台，屏幕下方)是两个独立控件，可以同时显示。

## 关键设计
- 没有点击/键盘推进，只靠`UForeverStoryFrameworkComponent`的计时器结束，见`ForeverStoryFrameworkComponent.md`"后台对话"一节。
- 显示时整个控件是`HitTestInvisible`、不可聚焦：不挡鼠标、不抢键盘焦点，不切输入模式，玩家照常操控角色，MeetOption照常可用。
- Blueprint子类`Content/Blueprint/UI/WBP_BackgroundSpeaking`只摆`Speaker`/`Content`两个TextBlock(BindWidget)，位置/样式在蓝图里调。
- `AForeverPlayerController::backgroundSpeakingWidgetClass`指定蓝图类，没指定时只打警告，后台对话照样计时推进(只是看不见)。

## 依赖关系
- 被`AForeverPlayerController`创建持有，被`UForeverStoryFrameworkComponent::ShowCurrentBackground`调用。
