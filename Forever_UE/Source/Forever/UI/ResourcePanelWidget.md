# ResourcePanelWidget.h / ResourcePanelWidget.cpp

## 职责

`UResourcePanelWidget`：配置界面的**Resource screen**，管理资源文件夹列表(`Config::AddResourcePath`)。

## 关键设计

- 只管资源文件夹；`.script`列表在`StoryPanelWidget`。一个资源文件夹同时覆盖`.layout`和`.script`(见`config.md`)。
- `ScrollBox`+`PathRowWidget`模式：`RefreshRows`重建列表，`AddResourceButton`弹文件夹对话框添加；行间距用`ScrollBoxUtils`。

## 依赖关系

- 依赖：`PathRowWidget`、`ScrollBoxUtils`、`ForeverConfigBridgeSubsystem`。
- 被谁依赖：`ConfigMenuWidget.cpp`。

## 待办/后续阶段

- 无。
