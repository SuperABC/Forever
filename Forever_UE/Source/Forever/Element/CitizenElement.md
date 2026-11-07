# CitizenElement.h / .cpp

## 职责

一个 citizen 在场景里显形时对应的 `ACitizenElement`，基类是 `AForeverCharacter`(和玩家角色共用摄像机/移动参数/Enhanced Input 绑定)，所以市民既能被 AI 带着走，也能被玩家 `Possess` 后直接用同一套输入/移动逻辑操控。和 `ABuildingElement` 同一套"每个 Core 实例对应一个强类型 Actor 子类"模式(Actor 持有指向 Core 对象的裸指针 + 指向共享框架组件的 `TWeakObjectPtr`)，区别是不常驻：由 `UForeverPopulaceFrameworkComponent::TickComponent` 按玩家距离流式 `SpawnActor` + `Init(citizen, this)` / 写回当前位置后 `Destroy()`(人口规模通常远大于建筑数量)，映射表 `activeInstances` 在框架组件里，见 `Framework/ForeverPopulaceFrameworkComponent.md`。

## 关键设计

- **位置"留空/首次随机/此后复用"在这里落地，不在 `Citizen`**：`Citizen` 只存 `hasPosition`+`posX/Y/Z`(地图单位)。`Init()` 里 `HasPosition()` 为 true 直接读记录位置(`*CITIZEN_WORLD_SCALE`)；否则 `ComputeRoomLandingSpot(room)`：房间中心(`Room::GetPosX/PosY`，经本文件私有的 `ComputeCitizenWorldPosition` 换算)±0.2 地图单位随机抖动，Z = `GetFloorBaseZ(layer)` + 地坪偏移 10 + **地板 slab 厚度 20**(Ground slab 中心在楼层底部+半个厚度，站人表面是 slab 顶面，不加会陷进地板) + 胶囊半高(actor location 是胶囊中心)，算出后**立刻** `citizen->SetPosition` 写回。流式销毁时由 Populace 读 `GetActorLocation()` 写回 `Citizen`，所以不管销毁多少次，重新生成都在最后的真实位置。`building` 必须从 `room->GetParentBuilding()` 反查，不能用 `citizen->GetBuilding()`(那是"家"，换房间后不一定同一栋楼)。
- **匿名 namespace helper 命名坑**：Forever 模块是 UBT unity build，多个 `.cpp` 会被拼进同一个翻译单元，各文件各自维护的匿名 namespace helper **函数名必须在整个模块内唯一**(本文件叫 `ComputeCitizenWorldPosition`，不能叫 `ComputeWorldPosition`)，否则重定义编译错误，且触发与最近改动无关。
- **移动模式跟着占有状态切换**：构造函数 `MOVE_None`(避免重力/地面检测挪走 `Init()` 摆好的位置)、默认关 Tick、`bRunPhysicsWithNoController=true`；`PossessedBy` 只对玩家 Controller 响应：切 `MOVE_Walking`、**重新打开 Tick**(`AForeverCharacter::Tick` 要每帧跑才有瞄准摄像机插值，否则右键瞄准事件触发但镜头纹丝不动)、禁掉自己的 `proximityBox`(自己就是玩家时盒与胶囊常驻重叠)；`UnPossessed` 切回 `MOVE_None`、恢复盒的 `Trigger` 预设、关 Tick，玩家放手时 `RepossessAIController()` 在**下一帧**让自己的 AI Controller 重新 Possess(UnPossessed 调用栈里 `Controller` 还没清空)。Input Mapping Context 的增删交给 `Super`。`Tick()` 在非玩家控制时自己关掉。
- **自己的 `ACitizenController`**：`Init()` 末尾创建并 Possess，`EndPlay` 里 `ClearCommands`+`UnPossess`+`Destroy`。`WalkTo(waypoints, destination)` 先 `ClearCommands`(新路线取代旧路线) 再推一条 `FollowPath`；走路/到达判定/`NotifyArrived` 全在 Controller，见 `Controller/CitizenController.md`。`GetWalkDestination()` 转问 Controller 当前指令(存档把走路中的市民记成"已经到了")。"玩家占有"判断一律用 `IsPlayerControlled()`。
- **`TeleportToRoom`**：`RequestWalk` 寻路失败(起点/终点没有导航节点，或图不连通)但该市民有 Actor 时，Actor 也必须跟着挪(`ComputeRoomLandingSpot`+`SetActorLocation`+`SetCurrentRoom`+`SetPosition`)，并先 `ClearCommands`(否则走完会回调 `NotifyArrived` 把房间改回去)；不挪的话 Core 已"到家"而可见 Actor 冻结在原地。
- **靠近检测碰撞盒**(`proximityBox`，半宽 120/半高 90)：纯 UE 层的装饰性判定，与流式生成/销毁的距离无关。只有有 Scheduler 脚本或有选项时才建，之后选项出现由 `RefreshOptions` 补建。姓名和选项在建盒时烘焙成快照(`citizenNameOnly`/`cachedOptions`)，Overlap 回调**绝不解引用 `citizen`**；必须排除 `OtherActor==this`，并要求 `OtherActor` 是玩家当前 pawn 且为 `AForeverCharacter`(否则开车靠近也会弹)。`add_option`/`remove_option` 后由 `AForeverFrameworkActor::RefreshInteractOptions` 调 `RefreshOptions()` 重新烘焙，并用 `UMeetOptionWidget::ReplaceOptions` 整组换掉屏幕上的选项。
- **外观**：身体网格按性别由 `AForeverCharacter::ApplyBodyMesh` 选(男 Manny、女 Quinn，同骨架)；头像由 `UForeverAvatarSubsystem::GetCitizenAvatar(citizen, 当前游戏年份)` 提供后 `SetAvatarTexture`，没有头像文件保留默认，见 `Player/ForeverAvatarSubsystem.md`。
- **`EndPlay` 安全性**：只保证自己的 `EndPlay` 之后不再解引用 `citizen`。只有 `EEndPlayReason::Destroyed` 才通知框架 `HandleElementEndPlay`(摘掉 `activeInstances` 里的悬空键)；退出游戏/切关卡/PIE 结束时 Core 的 Populace 可能已先释放，碰它们会崩。位置写回只在受控的运行时销毁路径里由 Populace 做。
- `SetDespawnExempt(true)`：上车(`ToggleVehicle`)时标记不参与距离销毁(Actor 隐藏在原地，玩家开车走远会被销毁，下车时 `previousPawn` 悬空)，下车传 false。`GetCitizen()` 供武器命中后反查 `Citizen*`。

## 依赖关系

- 依赖：`Player/ForeverCharacter.h`(基类)、`Controller/CitizenController.h`、`Framework/ForeverPopulaceFrameworkComponent.h`(`framework` 弱引用回调)、`Player/ForeverAvatarSubsystem`、`UI/MeetOptionWidget`、`Core/populace/citizen.h`、`map/building.h`/`room.h`。
- 被谁依赖：`UForeverPopulaceFrameworkComponent`(`SpawnActor`/`Init`/`Destroy`/`FindOrSpawnCitizenByName`)、`UForeverStoryFrameworkComponent::ApplyControlChange`(剧情 `change_control` 占有市民)、`UForeverWeaponComponent`(命中反查)。

## 待办/后续阶段

- 没有 Job/没有调度的市民仍静止，通用的"没事做时到处逛逛"自由游走 AI 还没有。
- 真实资产替换占位小白人；靠近盒尺寸是经验值，可能需按真实资产微调。
- Controller 化后的走路未在 PIE 完整验证。
- `bRunPhysicsWithNoController=true`(构造函数里)可能是 Controller 化前的遗留，未确认是否仍需要；涉及行为风险，暂未改。
