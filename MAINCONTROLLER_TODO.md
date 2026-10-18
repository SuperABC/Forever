# MainController 剩余热键

旧蓝图`Blueprint/Player/MainController`(dump文件`.dump/MainController.txt`)的热键已经全部迁移,没有遗留项。

最后接回的是**`Tab`(暂停菜单开关)**:新实现是`UI/ForeverPausePanelWidget`(保存/加载两个按钮),由`AForeverPlayerController::TogglePause`打开,按键走`UForeverKeyBindingSubsystem`的`TogglePause`配置项。

其余热键(地图面板`M`、手机`P`、退出载具`Q`、背包`B`、对话选项上下移/确认)均已实现,参见`Source/Forever/Player/ForeverPlayerController.cpp`的`BindAction`列表和`Element/VehicleElement.h`的`ExitVehicle`。`N`键(导航调试可视化)在旧蓝图里是手动触发的开关,新实现里改成了`UForeverRoadnetFrameworkComponent`常驻生成的调试网格,不再需要按键触发,不算遗留项。
