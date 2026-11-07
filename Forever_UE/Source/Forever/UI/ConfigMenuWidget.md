# ConfigMenuWidget.h / ConfigMenuWidget.cpp

## 职责

`UConfigMenuWidget`：**配置界面顶层**。包含Story/Mod/Resource三个screen的切换、开始游戏、加载存档、保存/读取配置。

## 关键设计

- 三个screen用`SetVisibility`切换(sibling panel手法)，不是tab控件；`ShowOnly(ETab)`保证同时只显示一个。
- `StartButton`调`UForeverConfigBridgeSubsystem::ValidateAndStartGame`，失败时在`FailureMessage`(左上角，初始Collapsed)显示拒绝原因；`LoadGameButton`交给`UForeverSaveSubsystem::LoadWithDialog`，失败原因同样显示在`FailureMessage`。
- `LoadGameButton`/`SaveConfigButton`/`LoadConfigButton`用`BindWidgetOptional`：WBP里还没放这些按钮时不影响蓝图编译；保存/读取配置弹json文件对话框，见`UForeverConfigBridgeSubsystem::Save/LoadConfigWithDialog`。
- 由`AForeverMenuController::BeginPlay`经`startMenuWidgetClass`(蓝图里指定为本类的子类)创建并`AddToViewport`。

## 依赖关系

- 依赖：`StoryPanelWidget`、`ModPanelWidget`、`ResourcePanelWidget`、`ForeverConfigBridgeSubsystem`、`ForeverSaveSubsystem`。
- 被谁依赖：`Player/ForeverMenuController`(经蓝图类指定)。

## 待办/后续阶段

- 无。
