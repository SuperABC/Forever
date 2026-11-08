# ACitizenController（市民 Controller）

`AAIController` 子类，指令用 `TCommandQueue<FCharacterCommand>`（和载具同一套队列，见 `VehicleController.md`）。

## 指令 `FCharacterCommand`
- `FollowPath(waypoints, room)`：沿 UE 世界坐标路径点逐点走，走完回调 `framework->NotifyArrived(citizen, room)`。市民自主上下班的异步 `FindPedestrianPath` 结果走这个（`ACitizenElement::WalkTo` 推一条 FollowPath）。
- `GoToRoom(room)`：Controller 自己向 `UForeverPopulaceFrameworkComponent::PlanWalkPath` 要从当前位置到 room 的行人路径再走（`Map::PlanPedestrianRoute`）。
- 公共字段：`speedScale`（走路速度倍率）、`bExemptDespawn`（执行期间不被流式销毁）。

## 生命周期
- 只在市民被 spawn 期间存在：`ACitizenElement::Init` 末尾 spawn 并 Possess，`EndPlay` 里 `ClearCommands` + Unpossess + Destroy。流式销毁时正在执行的指令/路径一并消失（不持久化）。
- 玩家占有市民时引擎让出这个 Controller（`OnUnPossess` 清空指令，还原速度/免销毁标记）；玩家放手后 `ACitizenElement::UnPossessed` 让它重新 Possess。`ClearCommands` 对每条未完成指令调 `EndCommand(false)`，被打断的指令不触发 `NotifyArrived`。
- 空闲（无指令）时关 Tick，不增加常驻开销。

## 移动
- 每帧朝当前路径点 `AddMovementInput`，只判水平距离 80uu（不用 `UPathFollowingComponent`，它的到达判定含竖直方向，行人图节点 Z 和脚底对不上会原地打转）。开始时 `MOVE_Walking`、`MaxWalkSpeed×speedScale`；结束还原，`MOVE_None`（玩家已接管则不动移动模式）。

## 注意
- 工程里"是不是玩家占有"一律用 `IsPlayerControlled()`，不要再用 `GetController()!=nullptr`（市民平时就有 AI Controller）。
- `EndCommand` 结束时：还原速度/免销毁标记、推进队列，`arrived && room` 才回调 `NotifyArrived`。

## 过马路（路口通行）

`GateCrosswalk`（Tick 里取到目标点之后、移动之前）：行人路径只是节点坐标串，不带边信息，所以靠 `JunctionTraffic::FindCrosswalk` 用"当前目标点→下一个点"的坐标去匹配人行横道连线（只管横道，转角不限行）。离横道起点 `crossingRequestDistance` 以内每 `crossingRequestInterval`（交通时钟秒）`Request`（行人 entity，长度 0）；放行后一边走一边续租，走过横道终点（`waypointIndex>crossingEndIndex`）`Leave`；没放行就走到离起点 `crossingWaitDistance` 处停下不再 `AddMovementInput`。`EndCommand`/`OnUnPossess` 会释放，销毁时来不及释放靠租约过期。Core 推算（未实例化）的行走不受路口限制，从横道中间交接给 Actor 的也不会被拦。

## 剩余路径
`GetRemainingWaypoints(out, outDestination)`：当前 FollowPath 指令的剩余路径点和目的地。第一个点是市民当前位置，**换算成脚底高度**（Actor 位置减胶囊半高；路径点本身是脚底高度，否则折线/交还 Core 的路径整体抬高）。市民被流式销毁时用它把行进交还 Core（`Citizen::ResumeWalk`），"你在去哪"折线也用它。`GetCurrentDestination()` 供存档把走路中的市民记成"已经到了"。
