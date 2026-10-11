# MainController 剩余热键

旧蓝图`Blueprint/Player/MainController`(dump文件`.dump/MainController.txt`)的热键已经全部迁移,只剩一项还没接回:

- **`Tab`(暂停菜单开关)**:对应旧逻辑`GlobalBase::GlobalPause`+`PausePanel`(UMG)。内核侧的暂停编排可以直接实现成`AForeverFrameworkActor`自己的方法,阻塞点是`PausePanel`对应的UMG Widget还没迁移。接回时复用已有的`UForeverKeyBindingSubsystem`配置化框架,不要重新写死按键。

其余热键(地图面板`M`、手机`P`、退出载具`Q`、背包`B`、对话选项上下移/确认)均已实现,参见`Source/Forever/Player/ForeverPlayerController.cpp`的`BindAction`列表和`Element/VehicleElement.h`的`ExitVehicle`。`N`键(导航调试可视化)在旧蓝图里是手动触发的开关,新实现里改成了`UForeverRoadnetFrameworkComponent`常驻生成的调试网格,不再需要按键触发,不算遗留项。
