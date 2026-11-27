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

`GateCrosswalk`（Tick 里取到目标点之后、移动之前）：行人路径只是节点坐标串，不带边信息，所以靠 `JunctionTraffic::FindCrosswalk` 用"当前目标点→下一个点"的坐标去匹配人行横道连线（只管横道，转角不限行）。离横道起点 `crossingRequestDistance` 以内每 `crossingRequestInterval`（交通时钟秒）`Request`（行人 entity，长度 0）；放行后一边走一边续租，走过横道终点（`waypointIndex>crossingEndIndex`）`Leave`；没放行就走到离起点 `crossingWaitDistance` 处停下不再 `AddMovementInput`。有红绿灯的路口要等横穿的那条路变红灯才会放行（规则在 Core `junction.md`"红绿灯"，这里不用改），已经上了横道的续租照常走完。`EndCommand`/`OnUnPossess` 会释放，销毁时来不及释放靠租约过期。Core 推算（未实例化）的行走不受路口限制，从横道中间交接给 Actor 的也不会被拦。

## 剩余路径
`GetRemainingWaypoints(out, outDestination)`：当前 FollowPath 指令的剩余路径点和目的地。第一个点是市民当前位置，**换算成脚底高度**（Actor 位置减胶囊半高；路径点本身是脚底高度，否则折线/交还 Core 的路径整体抬高）。市民被流式销毁时用它把行进交还 Core（`HandBackCitizenToCore`里重新`Citizen::BeginWalk`/`BeginWalkToGreen`），"你在去哪"折线也用它。`GetCurrentDestination()` 供存档把走路中的市民记成"已经到了"。

## 逃跑指令 Flee（2026-11-19）
`FCharacterCommand::EType::Flee`(`fleeTarget`、`speedScale`=4×该市民扰动、`bExemptDespawn=true`)：没有路线序列，路径点边走边追加且始终比当前目标点多提前一个(给横道判定看下一点)。
`BeginCommand`：`framework->PlanFleeStart`取最近行人线并选背离端点，再`ExtendFlee`提前选下一个；走到节点后`ExtendFleeAhead`→`PlanFleeNext`。`TickFlee`每0.25秒检查：目标没了(`FindActiveCitizen`为空/不活跃)=lost，
`IsFleeRangeExceeded`(移动中销毁距离的9成)=far。路径选不下去(`EndCommand(true)`)=far；被清空/取代=interrupted。`EndCommand`里统一回调`NotifyFleeEnded(citizen, reason)`。
`TickFlee`同一个0.25秒检查里还测跑的方向：前进方向(指向下一个路点)和"市民→目标"连线夹角小于90度就立刻掉头——以当前位置重新`PlanFleeStart`(最近线、背离端点)并整个换掉路径点，掉头后冷却0.75秒，过横道时不掉头。逃跑速度倍率4。
`RetargetFlee`：已在逃时连续受伤只换目标。`GetRemainingWaypoints`对Flee返回false，存档时按"没路线"处理(位置照存、`fleeFrom`存档，读档后接着逃)。

## 绿化区目的地（2026-11-20）
`FCharacterCommand`加`green/greenCircle`(`MakeFollowPathToGreen`)：FollowPath的目的地是绿化区的圆时`room`为空，走完`EndCommand`回调`framework->NotifyArrivedGreen`而不是`NotifyArrived`。
`GetRemainingWaypoints(out, outRoom, outGreen*, outCircle*)`两种目的地都返回true(绿化目的地时`outRoom`为空)；`GetCurrentGreenDestination`单独问"正在去哪个绿化圆"。
