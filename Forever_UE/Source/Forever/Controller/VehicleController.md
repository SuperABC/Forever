# Controller体系总览（载具 / 公交 / 人物）

`Source/Forever/Controller/`：

- `CommandQueue.h`：指令队列模板`TCommandQueue<T>`，见`CommandQueue.md`。
- `VehicleCommandController`：载具基类(指令、路径跟随、物理/运动学双模式)，见`VehicleCommandController.md`。
- `BusController`：公交，加到站停靠+进出站事件，见`BusController.md`。
- `CitizenController`：市民自主行动，见`CitizenController.md`。

所有Controller共用同一思路：Core/UE只往Controller里塞指令，具体怎么执行全在Controller里。

## 公交事件
- 循环里到站：发`enter_stop`、停靠`dwellSeconds`、发`leave_stop`、去下一站。`NavigateToNode`路过站不发事件。`ClearCommands`/`StopNow`在停站中会补发`leave_stop`(让脚本撤掉上车/下车选项)。
- 事件经`UForeverTrafficFrameworkComponent::FireStopEvent(vehicle, stationNodeId, entering)`转成站名事件发给这辆车自己的脚本。

## QueryLoopProgress
`LoopNodes`执行中查询循环进度(所在的腿、这一腿已走比例；停靠中则是停靠已过比例)，线路公交从物理段交还Core时刻表前用它对齐进度(`Route::SyncVehicleProgress`)，避免位置瞬移。Controller自己的运动学模式本来就沿同一条路径接着推进，不需要这一步；市民从Actor交还Core时用剩余路径点接着推算，同理已经连续。

## 路径规划入口
`Map::PlanVehicleRouteToNode`(起点是车位置，带车头朝向避免选反向车道)/`PlanVehicleLeg`(节点到节点)；规划成功后车的`Vehicle::SetCurrentRoom(nullptr)`(已离开房间/停车位)。折线先做拐点圆角(`cornerRadius`，转角<8度不处理)，再按`pathSpacing`细分，距离<100uu的点合并，曲率取相邻三点最大值(偏保守)。

## 待办/后续阶段
- 未在PIE完整验证；重心下移和`maxLateralAccel`需要实测调。
