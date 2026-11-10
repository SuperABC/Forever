# StoryPanelWidget.h / StoryPanelWidget.cpp

## 职责

`UStoryPanelWidget`：配置界面的**Story screen**，添加/删除`.script`文件，全部一起加载(不是多选一激活)。

## 关键设计

- 结构同`ResourcePanelWidget`的`ScrollBox`+`PathRowWidget`模式；`RefreshRows`重建列表，`AddScriptButton`弹文件对话框添加。行间距用`ScrollBoxUtils`。

## 依赖关系

- 依赖：`PathRowWidget`、`ScrollBoxUtils`、`ForeverConfigBridgeSubsystem`。
- 被谁依赖：`ConfigMenuWidget.cpp`。

## 待办/后续阶段

- 无。

## 主线剧情mod单选列表
WBP里加一个叫`MainStoryList`的`ScrollBox`（`BindWidgetOptional`）并在`ModCheckRowClass`里指定`WBP_ModCheckRow`；缺任何一个就不显示。
列表列出`script_mods`里已启用的id，用`UModCheckRowWidget::SetSelectRowData`做单选行：选中的那行勾选框禁用（不能取消），勾别的行就改选，写进`main_story`
（`Bridge::SetMainStory`，只接受已启用的script mod）。切到Story标签和脚本/mod变化后`RefreshRows`会重建它。开局校验见`ForeverConfigBridgeSubsystem.md`。
