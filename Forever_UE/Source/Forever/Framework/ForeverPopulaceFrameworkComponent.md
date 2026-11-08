# ForeverPopulaceFrameworkComponent.h / .cpp

## 职责

对应旧Framework Actor `Populace`（C++ Base:`PopulaceBase`）。**按玩家距离流式生成/销毁
`ACitizenElement`**，并负责市民走路（异步寻路）、瞬移、到达通知、存档位置同步。不像
`ABuildingElement`那样"开局全建好、常驻到关卡结束"——人口规模比建筑数量大得多。

`AForeverFrameworkActor::EnsurePopulaceGenerated()`在`map->Checkin(*populace)`之后调用一次
`GenerateCitizens(map, populace)`：只把`populace->GetCitizens()`缓存成`allCitizens`
（`TArray<Citizen*>`，不涉及所有权），不`SpawnActor`；真正的生成/销毁全在`TickComponent`。
世界还在分阶段生成（`IsInitialGenerationDone()`为假）时`TickComponent`直接返回，因为
`game_start`广播（给市民加交互选项）之前生成的市民，烘焙的选项快照是空的。

## 关键设计

### 流式生成/销毁

- **轮询分片**：`streamingBatchStride`（20）决定每帧处理`allCitizens.Num()/stride`个市民，
  `pollCursor`循环推进，避免总人口大时每帧全量距离检测。每帧最多生成`maxSpawnsPerFrame`(1)个
  Actor（一次`SpawnActor`约5ms），到上限就停止本帧扫描，`pollCursor`只前进到已处理的位置。
- **距离度量**：`StreamingDistance`把Z差放大`POPULACE_VERTICAL_DISTANCE_WEIGHT`(5)倍，
  不同楼层的人一般互相看不到；生成和销毁用同一个度量，迟滞区间才一致。
- **迟滞**：`citizenSpawnDistance`(2.0地图单位) < `citizenDespawnDistance`(4.0)，避免临界距离来回生成/销毁。
- **静止/移动分开**：静止的（Actor没有走路命令）用上述两个距离，最多`maxStationaryCitizens`(16)个；
  移动中的（Actor有走路命令，或Core正在推算路线）距离乘`movingDistanceScale`(2)，最多
  `maxMovingCitizens`(32)个。免销毁的（`IsDespawnExempt()`，如被玩家操控）不计数。名额满了只是不再生成新的，
  已存在的Actor只按距离销毁，不因类别超限被销毁。
- **未生成时的位置**：`ComputeLogicalPosition`——`HasPosition()`用记录值，否则用`GetCurrentRoom()`中心，
  **不加随机抖动**（抖动是`ACitizenElement::Init()`生成时的一次性效果）。building必须从
  `room->GetParentBuilding()`反查，**不能用`citizen->GetBuilding()`**（那是"家"所在的楼，
  `currentRoom`换到工作单位后两者不同栋，混用会算出错误世界坐标，症状是上班时间商店里看不到店员）。
- **销毁**：先把Actor位置转回地图单位`SetPosition`回`Citizen`（下次直接在记录位置出现），
  若还在走路，用控制器`GetRemainingWaypoints`的剩余路径`BeginWalk`交还Core，不丢目的地，再`Destroy()`并摘出`activeInstances`。
- **死人和离城的人**：流式循环、`FindOrSpawnCitizenByName`、`RequestWalk`/`ApplyWalkResult`/`NotifyArrived`
  都跳过`!IsActive()`（死亡或离城）的市民，`TeleportCitizen`打警告后返回；`HandleCitizenDeath`销毁其Actor，死亡和离城（`AForeverFrameworkActor`收到`DailyReport::departed`后逐个调用）共用它。复活（`npc_revive`）后再teleport才恢复，离城不可逆。

### `activeInstances`

`TMap<Citizen*, TObjectPtr<ACitizenElement>>`是本组件自己维护的映射（Core的`Citizen`不知道自己有没有Actor）。
`Citizen*`键没法`UPROPERTY`，GC不会置空，所以`ACitizenElement::EndPlay`回调`HandleElementEndPlay`：
Actor以任何方式结束（流式销毁、掉出世界被引擎销毁）都从映射摘掉，避免悬空指针。
`EndPlay`只把`map`置空，不主动`Destroy()`场上的Actor（关卡卸载会清理）。

### `FindOrSpawnCitizenByName` / `SpawnCitizen`

剧情`ChangeControlChange`、`StartTestNavigation`等指定的市民不一定在玩家附近，
`FindOrSpawnCitizenByName`按`Citizen::GetName()`（UTF-8转`FString`）在`allCitizens`里线性查找，
已有Actor直接返回，否则`SpawnCitizen`强制生成，不看距离、不受数量上限限制。找不到同名/已死亡/生成失败返回`nullptr`。
`SpawnCitizen`是流式生成和强制生成共用的私有函数，调用方保证`citizen`非空且不在`activeInstances`里。

### 走路：`RequestWalk` + 异步寻路

`RequestWalk(citizen, destination)`（Job产出`NPCNavigateChange`时由`AForeverFrameworkActor::Tick`的回调转发）：

- 被玩家占有（`IsPlayerControlled()`）的市民整次调度不生效（不走路也不瞬移，被占有期间的调度不补）。
- 先`++latestWalkRequest[citizen]`得到请求序号；`DrainPathResults`丢弃序号过期的异步结果，
  避免旧导航的结果晚到把新路线盖掉。`TeleportCitizen`也递增序号来作废在途结果。
- **起点**：在路上（`GetTransitPosition`：Actor控制器有指令，或Core在推算）从**当前位置**起算，
  后台用`Map::PlanPedestrianRoute(nullptr, x, y, dest)`接到最近的行人图节点，路径第一个点补上当前位置；
  不在路上用`currentRoom`的`GetNavigationNode()`，已经在目的地房间直接返回。
  新路线取代旧路线（`ACitizenElement::WalkTo`先`ClearCommands`），中途目标从未"到达"过，不会误发`npc_arrive`。
- **没有可用导航节点**：没有Dijkstra可算，同步`ApplyWalkResult(..., 空路径)`。
- **否则**：`pendingPathfindingTasks`+1，`Async(EAsyncExecution::ThreadPool, ...)`跑`map->FindPedestrianPath`
  （纯读，地图生成后导航图不再被写，多线程并发只读安全），结果推进`pathResultQueue`（`TQueue<..., Mpsc>`）后计数-1。
  每天整点所有有工作的市民同时触发调度，Dijkstra在车道级导航图上不便宜，放游戏线程会持续卡顿。
- `TickComponent`每帧`DrainPathResults`，对每条调`ApplyWalkResult`。它碰`activeInstances`/`ACitizenElement`，必须在游戏线程；
  且**按落地那一刻的状态重新判断**（发起到落地隔了若干帧，市民可能已被流式销毁/重新生成或刚被占有）。

#### `ApplyWalkResult`分流

1. 被玩家占有：不处理。
2. 路径非空 + 有Actor：`WalkTo(waypoints, destination)`，`currentRoom`置空（人在路上不属于任何房间），走完由`NotifyArrived`换成目的地并发`npc_arrive`。
3. 有Actor但路径为空（寻路失败）：`TeleportToRoom(destination)`，不能只改Core状态让可见Actor冻结在原地。
4. 无Actor、路径点>1：`Citizen::BeginWalk`交给Core按时间推算位置（速度取`ACitizenElement`默认`MaxWalkSpeed`乘`kUnspawnedWalkSpeedScale`(10)，
   省得大批通勤市民很久才到），`currentRoom`置空。流式扫描里这类市民每次先`AdvanceWalk`（走完换房间并`ReportArrival`），
   位置进入生成距离就生成Actor（抬一个胶囊半高），用`GetRemainingWalk`的剩余路径`WalkTo`接着走并`EndWalk`（之后按正常速度，进度被拖慢）。
5. 无Actor且路径为空：`EndWalk` + `SetCurrentRoom(destination)` + `ClearPosition()`（位置重置，下次生成按新房间重算）。

`TeleportCitizen`：有Actor就`TeleportToRoom`，否则`EndWalk`+`SetCurrentRoom`+`ClearPosition`；被玩家占有的不动。

不使用`AIController`/`NavMesh`：`ACitizenElement::WalkTo`沿`Map::FindPedestrianPath`算好的路径点逐点前进，见`Element/CitizenElement.md`。

#### 关闭时的跨线程安全

后台线程捕获`Map*`/`Citizen*`/`Room*`裸指针和`TWeakObjectPtr`（只用来判断组件是否还在，决定要不要入队）。
`AForeverFrameworkActor::EndPlay`/析构要`delete map`/`delete populace`，此时若还有后台线程在读就是use-after-free，因此：

- `pendingPathfindingTasks`和`bCancelPathfinding`是**static**：组件在EndPlay/析构期间随时可能被回收，实例成员活不过等待窗口。
- 两处在`delete`前调`WaitForPendingPathfinding()`：先置`bCancelPathfinding`让排队中还没开始的任务直接跳过（只递减计数），
  再`Sleep(0.001f)`忙等计数归零。`GenerateCitizens`把`bCancelPathfinding`复位（PIE可能连续开）。
- **不要改成"派发GameThread任务再等它跑完"**：EndPlay本身跑在游戏线程上，会死锁。
- `pathResultQueue`里没drain的残留结果只含纯数据指针值和`TArray<FVector>`，随组件销毁即可。
- 屏幕左上角青色文字显示`pendingPathfindingTasks`（`bShowPathfindingQueue`，调试用，默认开）。

### 到达通知

`ReportArrival(citizen, previousRoom)`在真正到达处调用（有Actor的`NotifyArrived`、无Actor的`AdvanceWalk`返回true、
寻路失败/`TeleportCitizen`落地），房间真的变了才转给`UForeverStoryFrameworkComponent::BroadcastNpcArrive`；存档同步/恢复不发。

### 存档：`SyncPositionsToCore` / `RestoreAfterSave`

存档前由`UForeverSaveSubsystem::SaveToFile`调用：先`DrainPathResults`，把场景里每个Actor的位置写回`Citizen::SetPosition`
（平时只在走远被销毁时才写回）。同时给每个市民记下"是否已实例化"（`Citizen::SetWasSpawned`）。正在走路的市民存即时状态：Actor在走的，把控制器`GetRemainingWaypoints`的剩余路径（首点=当前位置）临时交给Core行进模拟（`BeginWalk`，速度取Actor的`MaxWalkSpeed`，不乘无Actor的加速系数）；Core在推算的，`RebaseWalk`改写成从当前位置起的剩余路径。临时交给Core的那些存完`RestoreAfterSave`立刻`EndWalk`并还原，不影响正在进行的游戏；指令还没展开成路径的少数情况退回按"已经走到了"记。

读档后`GenerateCitizens`对每个市民`RestartWalk(now)`对齐行进时钟，并把`WasSpawned`的市民放进`pendingRestoreSpawns`。`TickComponent`在流式扫描之前先逐个强制生成它们（不看距离、不受数量上限，每帧`maxSpawnsPerFrame`个，在Core里行进的接着交给Actor走），队列清空才恢复正常流式扫描，所以读档后看到的人和存档前是同一批（可能更多，不会更少）。
返回的`FWalkingSnapshot`保存临时改动前的状态，存完`RestoreAfterSave`立刻还原，不影响正在进行的游戏。

### 其它

- `ForEachActiveCitizen` / `FindActiveCitizen`：识别模式（`UForeverIdentifyComponent`）等枚举/查询已生成Actor，前者包含被玩家操控的那个，不触发生成。
- `PlanWalkPath`：`ACitizenController`的`GoToRoom`指令用，同步规划`from`到`destination`房间的行人路径点（`Map::PlanPedestrianRoute`）。
- `StartTestNavigation(name)`（script的`test_navigate_citizen` Change）：`FindOrSpawnCitizenByName`后用
  `UForeverTrafficFrameworkComponent::TrackTestNavigationLine`持续显示该市民剩余路线的橙色折线（无计时，走完才清），
  数据来自`ACitizenController::GetRemainingWaypoints`。

## 依赖关系

- 依赖：`Element/CitizenElement.h`、`Controller/CitizenController.h`、`Source/Core`的`populace/populace.h`/`citizen.h`、
  `map/map.h`/`building.h`/`room.h`/`general_geometry.h`、`UForeverTrafficFrameworkComponent`（测试折线）、
  `UForeverStoryFrameworkComponent`（`BroadcastNpcArrive`）、`Async/Async.h`、`Containers/Queue.h`、`HAL/PlatformProcess.h`。
- 被依赖：`AForeverFrameworkActor`（`EnsurePopulaceGenerated`、`Tick`里转发`RequestWalk`/`TeleportCitizen`/`AddCitizen`/`StartTestNavigation`、`EndPlay`/析构里`WaitForPendingPathfinding`）、
  `UForeverStoryFrameworkComponent`（`FindOrSpawnCitizenByName`）、武器系统（`HandleCitizenDeath`）、
  存档子系统、`ACitizenElement`/`ACitizenController`（`NotifyArrived`、`HandleElementEndPlay`、`PlanWalkPath`）。

## 待办/后续阶段

- `streamingBatchStride`/距离阈值/数量上限是经验初始值，人口规模和地图尺寸确定后按PIE结果调整。
- 全员随机走动之类的压力测试不放C++，在script里用`npc_navigate`等Change实现。

## 行走实例化概率
每次行走的寻路结果落地时(`ApplyWalkResult`)掷一次骰子(`Citizen::RollWalkInstantiable(全局概率)`)，决定这次行走是否参与流式实例化。
不参与的市民在 `TickComponent` 里一直由 Core 按时间推算位置，走完为止，不占 `maxMovingCitizens` 名额，也不会在视口中间凭空出现。

全局概率 `walkInstantiateProbability` 是动态的：初始1；每扫完一整轮 `allCitizens`，统计进入生成距离(×`movingDistanceScale`)的行走者数
(含已实例化的和被排除的)，取历史峰值x，概率 = min(当前, 1.5*maxMovingCitizens/x)，只减不增——第一次高峰期只是在测x，从第二次高峰起才真正限流。
`Citizen::walkInstantiateProbability` 默认-1表示跟随全局；脚本 `spawn_npc` 的故事市民设成1，不受影响。该字段和本趟抽签结果随存档保存(未升版本)；全局概率与峰值不存档。
