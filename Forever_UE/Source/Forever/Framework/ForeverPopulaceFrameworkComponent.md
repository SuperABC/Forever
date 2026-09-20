# ForeverPopulaceFrameworkComponent.h / .cpp

## 职责

对应旧Framework Actor `Populace`（C++ Base:`PopulaceBase`）。阶段2-3是空骨架，这次进入
populace域时第一次真正填内容：**按玩家距离流式生成/销毁`ACitizenElement`**——和用户明确
确认过的架构决定（老工程`APopulaceBase`同款做法），不是像`ABuildingElement`那样"开局全
建好、常驻到关卡结束"——人口规模通常比建筑数量大得多，流式管理是更保险的默认选择。

`AForeverFrameworkActor::EnsurePopulaceGenerated()`在`map->Checkin(*populace)`（Core侧
人口分配完成）之后调用一次`GenerateCitizens(map, populace)`——这一步**不`SpawnActor`
任何东西**，只是把`populace->GetCitizens()`缓存成一份`TArray<Citizen*>`，真正的生成/
销毁全部在`TickComponent`里按距离做。

## 关键设计

### 流式生成/销毁：轮询分片 + 迟滞区间

老工程`APopulaceBase::Tick`按`step/stride`轮询分片（每帧只处理`1/stride`的citizen），
避免citizen总数很大时每帧全量距离检测——这次照抄同一个节流思路：`streamingBatchStride`
（默认20）决定每帧处理`allCitizens.Num()/streamingBatchStride`个citizen，`pollCursor`
循环回绕推进。

对轮到的每个citizen：
- **还没有`activeInstances`条目**：估算它当前的"逻辑位置"（`Citizen::HasPosition()`为
  true用记录值，否则用房间中心，**不加随机抖动**——抖动是`ACitizenElement::Init()`真正
  生成时才加的一次性效果，估算阶段只是为了判断距离，不应该影响citizen最终落地的位置）
  到玩家`GetPlayerPawn(0)`的距离，小于`citizenSpawnDistance`（默认2.0地图单位，老工程
  同款字面值）就`SpawnActor<ACitizenElement>()`+`Init()`，记入`activeInstances`。
- **已有`activeInstances`条目**：读对应Actor的`GetActorLocation()`到玩家的距离，大于
  `citizenDespawnDistance`（默认4.0地图单位，老工程同款字面值）就先把这个位置转回地图
  单位`SetPosition`回`Citizen`（这样下次重新进入范围直接用这份记录，不再随机抖动一次
  ——满足"反之直接在记录的位置出现"的要求），再`Destroy()`该Actor、从`activeInstances`
  移除。

两个距离阈值不同（`citizenSpawnDistance < citizenDespawnDistance`）形成迟滞区间，和
building的LOD双阈值同一个防抖动理由：避免玩家在临界距离附近小范围来回走动时citizen反复
生成/销毁。

### `FindOrSpawnCitizenByName`/`SpawnCitizen`：按姓名强制生成，不看玩家距离

阶段4 Story域接入`ChangeControlChange`（剧情/T键之外，第三种"切换玩家操控权"的触发源，见
`ForeverStoryFrameworkComponent.md`"ApplyControlChange"一节）之后新增：剧情指定的目标市民
不一定在玩家附近，不能像`TickComponent`那样等距离小于`citizenSpawnDistance`才生成——
`FindOrSpawnCitizenByName(name)`按`Citizen::GetName()`（UTF-8转`FString`比较）在
`allCitizens`里线性查找同名citizen，找到后：已经在`activeInstances`里就直接返回那个
`ACitizenElement*`（弱指针失效则先从map里移除，走强制生成分支）；否则调用`SpawnCitizen`
强制生成一个，不做任何距离判断。找不到同名citizen或生成失败统一返回`nullptr`。

`SpawnCitizen(Citizen*)`是从`TickComponent`原来内联的"估算位置+`SpawnActor`+`Init`+登记
`activeInstances`"这段逻辑里提炼出来的私有辅助函数——`TickComponent`的按距离生成分支和
`FindOrSpawnCitizenByName`的强制生成分支现在共用同一份实现，不再各自维护一份几乎相同的
代码。调用方（这两处）自己负责保证传入的`citizen`不为空、且不在`activeInstances`里，
`SpawnCitizen`内部不重复检查这两个前提。

### `activeInstances`：谁映射谁，生命周期归谁管

`TMap<Citizen*, TObjectPtr<ACitizenElement>> activeInstances`是这个组件自己维护的映射
——`Citizen`（Core）不知道自己有没有对应的Actor，`ACitizenElement`也不需要在别处登记
自己。`Destroy()`一个Actor后必须同步`activeInstances.Remove(citizen)`，否则下一帧还会
认为它"已经有实例"从而跳过重新估算距离/生成的判断。

### `EndPlay`：和`UForeverBuildingFrameworkComponent`同一个模式

`EndPlay`只把`map`置空，**不主动`Destroy()`场上还活着的`ACitizenElement`**——关卡卸载
会自动清理所有Actor，`activeInstances`这个`TMap`本身也会在组件销毁时一起被回收，不需要
手动清空。

### 市民走路（进入society域新增）：Dijkstra寻路 + 生成/未生成分流

`RequestWalk(Citizen* citizen, Room* destination)`：`Job`按调度产出`NPCNavigateChange`
时，`AForeverFrameworkActor::Tick`的回调解析出目标`Room*`后转发到这里。用
`destination`和市民当前所在room（`Citizen::GetCurrentRoom()`）各自
`GetNavigationNode()`的id调`map->FindPedestrianPath(...)`（对`pedestrianNavGraph`跑
Dijkstra，见`Source/Core/map/map.md`），路径点（Core绝对地图坐标，float）转成
`TArray<FVector>`（直接乘`POPULACE_WORLD_SCALE`——`Node`坐标已经是绝对地图坐标，不需要
像Room坐标那样再做building局部变换）：

- **这个citizen当前正被玩家占有（`GetController()!=nullptr`，这个项目没有
  `AIController`，非空Controller只可能是玩家占有）**：整次调度直接不生效——不走路、
  也不瞬移，应用户要求"正在被操控的人不应该自己上下班"。等玩家取消占有之后，下一次
  `Job`调度会正常处理（这次不补一次"错过的"调度，被占有期间产生的上下班节点就当没
  发生过）。
- 路径非空 且 这个citizen当前**有已生成的`ACitizenElement`**（查`activeInstances`）：调
  `WalkTo(waypoints, destination)`播放真实走路动画，全部路径点走完后
  `ACitizenElement`会回调`NotifyArrived(citizen, destination)`更新
  `Citizen::SetCurrentRoom`。
- **路径为空，但这个citizen当前有已生成的`ACitizenElement`**（起点/终点没有导航节点，
  或图不连通——Actor已经在场景里，是玩家看得见的）：调
  `(*existing)->TeleportToRoom(destination)`——**不能**退化成只改`Citizen`的逻辑状态、
  不管这个可见Actor。PIE验证复现过这个bug：市民到点该走了，`RequestWalk`寻路失败落到
  只改`SetCurrentRoom`的分支，`Citizen`逻辑上已经"到家"了，但眼前这个人一直冻结在原地
  不动，因为压根没人碰过它的Actor。`TeleportToRoom`直接把Actor也瞬移到目标房间（复用
  `ACitizenElement::Init()`同一套落地位置计算），同时同步`SetCurrentRoom`+`SetPosition`。
- **否则**（这个citizen当前没有生成的`ACitizenElement`，不在流式加载范围内）：直接
  `citizen->SetCurrentRoom(destination)`+`citizen->ClearPosition()`（不是
  `SetPosition`——这个citizen当前没有对应Actor，不需要算一个精确3D坐标，
  `hasPosition`重置回`false`和"换房间后从未在场景里实例化过"是同一个状态，等它下次真的
  被生成时会按新的`GetCurrentRoom()`重新算一次位置+随机偏移）。

不使用`AIController`/`NavMesh`——`ACitizenElement::WalkTo`只是沿`Map::
FindPedestrianPath`算好的现成路径点，用`CharacterMovement`逐点插值前进，见
`Source/Forever/Element/CitizenElement.md`。

**已修复的bug：`ComputeLogicalPosition`算currentRoom的世界坐标时不能用
`citizen->GetBuilding()`**——那是"家"所在的building（`Map::Checkin()`一次性设好，
`RequestWalk`不会改它），`RequestWalk`把`currentRoom`换成工作单位的room之后，两者
不再保证同一栋楼；`GetBuilding()`+新`room`这个组合会用错误的building变换（错误的
`GetPosX/Y`/`GetRotation`/`GetBodySizeX/Y`）算世界坐标，结果是citizen的逻辑位置落在
"家building的坐标系里解释商店room的局部坐标"这种没有意义的地方——PIE验证的直接症状是
"上班时间去商店里找不到任何店员"（因为店员实际计算出的位置压根不在那家商店附近）。
修复：改成从`room->GetParentBuilding()`反查building（`ACitizenElement::Init()`里
估算首次生成位置的同一段逻辑也有这个bug，一起修了），不再依赖`Citizen`自己存的那份
"家"building字段。

## 依赖关系

- 依赖：`Source/Forever/Element/CitizenElement.h`（`SpawnActor<ACitizenElement>()`+
  `Init()`+`WalkTo()`）、`Source/Core/populace/populace.h`/`citizen.h`
  （`Populace::GetCitizens()`/`Citizen`）、`map/map.h`（`FindPedestrianPath`）、
  `map/building.h`/`map/room.h`/`map/geometry.h`（估算逻辑位置+`Node`坐标要用）、
  `Kismet/GameplayStatics.h`（`GetPlayerPawn`）。
- 被谁依赖：`AForeverFrameworkActor::EnsurePopulaceGenerated()`（`GenerateCitizens(map,
  populace)`）、`AForeverFrameworkActor::Tick`（`RequestWalk`，Job调度产出
  `NPCNavigateChange`时转发）、`UForeverStoryFrameworkComponent::ApplyControlChange`
  （`FindOrSpawnCitizenByName`，剧情`ChangeControlChange`指定切换控制的市民不一定在附近，
  需要强制生成，见`ForeverStoryFrameworkComponent.md`）。

## 待办/后续阶段

- `streamingBatchStride`/`citizenSpawnDistance`/`citizenDespawnDistance`是按经验给的
  初始值（后两个直接照抄老工程字面值2.0/4.0），实际人口规模/地图尺寸确定后可能需要
  按PIE验证结果调整，和这次会话早前building LOD阈值的调优是同一类后续工作。
- 目前每次判定只处理"生成/销毁"，不处理"citizen在两次判定之间发生了变化"——`RequestWalk`
  接入之后这一点已经不完全成立了：一个正在`WalkTo`途中的citizen如果被判定超出
  `citizenDespawnDistance`会被直接`Destroy()`，不会等它走完剩余路径点，也不会中断
  `WalkTo`本身的状态（`pendingWaypoints`等字段随Actor一起销毁）——这次没有特殊处理这个
  交叉场景，走路中途被销毁的citizen会瞬间"消失"，下次生成时用`Citizen`当时记录的位置
  （销毁前`GetActorLocation()`读回写的那份），不是`WalkTo`原本要走到的终点，属于已知的
  简化，真要处理可以在销毁前先检查`pendingWaypoints`是否非空、直接调用
  `NotifyArrived`兜底。
