# ACitizenController（市民 Controller）

`AAIController` 子类，指令用 `TCommandQueue<FCharacterCommand>`（和载具同一套队列，见 `VehicleController.md`）。

## 指令 `FCharacterCommand`
- `FollowPath(waypoints, room)`：沿 UE 世界坐标路径点逐点走，走完回调 `framework->NotifyArrived(citizen, room)`。市民自主上下班的异步 `FindPedestrianPath` 结果走这个（`ACitizenElement::WalkTo` 现在只是推一条 FollowPath）。
- `GoToRoom(room)`：Controller 自己向 `UForeverPopulaceFrameworkComponent::PlanWalkPath` 要从当前位置到 room 的行人路径再走（`Map::PlanPedestrianRoute`）。
- 公共字段：`speedScale`（走路速度倍率）、`bExemptDespawn`（执行期间不被流式销毁）、`bTestNavigation`（结束/被打断回调 `FinishTestNavigation` 清调试线；导航测试已停用，代码保留）。

## 生命周期
- 只在市民被 spawn 期间存在：`ACitizenElement::Init` 末尾 spawn 并 Possess，`EndPlay` 里 `ClearCommands` + Unpossess + Destroy。流式销毁时正在执行的指令/路径一并消失（不持久化）。
- 玩家占有市民时引擎让出这个 Controller（`OnUnPossess` 清空指令，还原速度/免销毁标记）；玩家放手后 `ACitizenElement::UnPossessed` 让它重新 Possess。
- 空闲（无指令）时关 Tick，不增加常驻开销。

## 移动
- 每帧朝当前路径点 `AddMovementInput`，只判水平距离 80uu（不用 `UPathFollowingComponent`，它的到达判定含竖直方向，行人图节点 Z 和脚底对不上会原地打转）。开始时 `MOVE_Walking`、`MaxWalkSpeed×speedScale`；结束还原，`MOVE_None`（玩家已接管则不动移动模式）。

## 注意
- 工程里"是不是玩家占有"一律用 `IsPlayerControlled()`，不要再用 `GetController()!=nullptr`（市民平时就有 AI Controller）。
- 未在 PIE 验证。
