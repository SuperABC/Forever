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
